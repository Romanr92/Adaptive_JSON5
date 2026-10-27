#ifndef PARSER_ENGINE_HPP_
#define PARSER_ENGINE_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/warning_list.hpp"
#include "input/cursor.hpp"
#include "memory/resource_budget.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_dispatch.hpp"
#include "parser/parser_frame.hpp"
#include "parser/parser_stack.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Own one completed value tree and its consumed input length.
     * @par Requirements
     * P033; P037; J5-013; J5-026; J5-027; J5-040; J5-041.
     * @note The document has an established root.
     * consumed_bytes includes leading trivia.
     * run() stops immediately after the root without validating remaining input.
     * run_complete() includes trailing trivia and requires complete consumption.
     * warnings holds every retained string and member-name warning in source order.
     * The caller's accounting object and storage provider must outlive values.
     */
    struct [[nodiscard]] parsed_value_tree
    {
        document values;
        std::size_t consumed_bytes{};
        warning_list warnings;
    };

    /**
     * @brief Construct a private value tree using iterative container frames.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027; J5-048.
     * @note Internal operation object; construct a fresh instance for each run.
     * Single use: after any result, construct a new engine to parse again.
     * Resource reservations are not rolled back after failure; the failed state is
     * discarded as a whole and owned storage is released by destructors (ADR 0003, decision 9).
     * Partial documents are never returned after failure.
     * No recursive descent is used.
     */
    class [[nodiscard]] parser_engine
    {
      private:
        input_view m_input;
        parse_options m_options;
        storage_callbacks m_storage;
        std::size_t m_owned_byte_limit;

        cursor m_source;
        document m_document;
        resource_budget m_resources;
        work_budget m_work;
        parser_stack m_frames;

        value_index m_root{};
        warning_list m_warnings;

        // Counting pre-pass state (ADR 0003, decision 7). The innermost container keeps its full
        // grammar state; each open level keeps one bit, set for an object (decision 7, D1).
        static constexpr std::size_t c_inline_nesting_bits = 512;
        static constexpr std::size_t c_bits_per_byte = 8;

        parser_frame m_count_top{};
        std::size_t m_count_depth{};
        bool m_count_root_done{};
        unsigned char m_inline_kinds[c_inline_nesting_bits / c_bits_per_byte]{};
        owned_block m_extra_kinds{};

        /**
         * @brief Address the nesting-kind bits for the current pre-pass.
         * @par Requirements
         * P033; J5-046; J5-048.
         * @note The inline array unless max_depth exceeded it and extra bits were allocated.
         */
        [[nodiscard]] unsigned char* nesting_kinds() noexcept;

        /**
         * @brief Count one value without building it: reserve its resources and validate it.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027; J5-046; J5-047; J5-048.
         * @note Reports the same errors as read_value and allocates nothing (ADR 0003, decision 7).
         * Charges the same scanning work as read_value, but no construction or decode work.
         */
        [[nodiscard]] status_result count_value() noexcept;

        /**
         * @brief Count one member name without decoding it.
         * @par Requirements
         * P033; P035; J5-017; J5-019; J5-020; J5-046; J5-047.
         * @note Reports the same errors as read_name except duplicate names, which are checked
         * only in the build pass (ADR 0003, decision 7, C1). Allocates nothing.
         */
        [[nodiscard]] status_result count_name() noexcept;

        /**
         * @brief Consume a dispatched colon or comma in the pre-pass.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027.
         * @note Same validation and errors as consume_separator.
         */
        [[nodiscard]] status_result count_separator(parser_action action) noexcept;

        /**
         * @brief Close the innermost container in the pre-pass.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027; J5-048.
         * @note Uses check_container_close, like close_container, so errors are identical.
         */
        [[nodiscard]] status_result count_close() noexcept;

        /**
         * @brief Record a completed value in the pre-pass: finish the root or advance the parent.
         * @par Requirements
         * P033; P034; P035; J5-013.
         */
        [[nodiscard]] status_result count_completed_value() noexcept;

        /**
         * @brief Run the counting pre-pass over the whole document.
         * @par Requirements
         * P033; P037; J5-026; J5-046; J5-047; J5-048; J5-049.
         * @param complete True to also require complete input consumption, as run_complete does.
         * @return Success with the counts held in m_resources, or the first error.
         * @note Allocates nothing with max_depth up to 512; above that, one block of nesting bits
         * that is released before returning. Leaves m_source where the pre-pass stopped.
         */
        [[nodiscard]] status_result count_document(bool complete) noexcept;

        /**
         * @brief Count, reserve exactly, then build one root value.
         * @par Requirements
         * P033; P037; J5-013; J5-026; J5-040; J5-046; J5-048; J5-049.
         * @param complete True to require complete input consumption.
         * @note Shared body of run() and run_complete() (ADR 0003, decision 7).
         */
        [[nodiscard]] result<parsed_value_tree> parse(bool complete) noexcept;

        /**
         * @brief Start a container or construct and accept a scalar value.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027; J5-028; J5-048.
         * @note Reserve node/member resources before construction and depth
         * before growing container frames.
         * Scalar decoding uses the remaining aggregate string allowance.
         * Failure preserves the source cursor but may change private state;
         * the failed engine must be abandoned.
         * Failed container opening releases its matching reservations.
         * Successful work charges are not refunded.
         */
        [[nodiscard]] status_result read_value() noexcept;

        /**
         * @brief Decode and retain the innermost object's next member name.
         * @par Requirements
         * P033; P035; P036; J5-032; J5-033; J5-049; J5-017; J5-019; J5-020; J5-048.
         * @note Limits lexical growth to the remaining decoded-unit allowance.
         * Success retains the name and selects object_colon.
         * Retains only the first warning descriptor and counts all warnings.
         * Failure preserves the source cursor and pending name.
         * Earlier budget reservations are not rolled back after later failure;
         * the failed engine must be abandoned.
         * Under reject_duplicates, checks completed members of the same object
         * before committing the name or cursor.
         * Duplicate failures retain both original name-token spans.
         * Preserve-all bypasses duplicate scanning.
         */
        [[nodiscard]] status_result read_name() noexcept;

        /**
         * @brief Consume a dispatched colon or comma and update its frame.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027.
         * @note Validates action, state and punctuation before mutation.
         * Success consumes exactly one byte.
         * Failure preserves the cursor and frame.
         * Dispatch work is charged by run().
         */
        [[nodiscard]] status_result consume_separator(parser_action action) noexcept;

        /**
         * @brief Close the innermost container and accept its completed value.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027; J5-048.
         * @note Successful closure records the span and removes the frame.
         * Releases depth before accepting the completed value.
         * Failure requires abandoning this engine; earlier internal mutations
         * are not rolled back after a later attachment failure.
         * The source cursor is committed only after all steps succeed.
         */
        [[nodiscard]] status_result finish_container() noexcept;

        /**
         * @brief Attach a completed value or record the completed root.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027.
         * @note Reuse attach_parsed_value when a parent frame exists.
         * Otherwise record m_root without publishing the document yet.
         */
        [[nodiscard]] status_result accept_completed_value(value_index completed) noexcept;

        /**
         * @brief Reserve and record every warning of one validated quoted string.
         * @par Requirements
         * P039; J5-017; J5-041; J5-046; J5-047; J5-049.
         * @param token_offset Original offset of the string's opening quote.
         * @param token_length Byte length of the string token, including both quotes.
         * @param count Number of warnings the lexer reported for that string.
         * @return Success, or an explicit work, diagnostic-limit, storage or internal failure.
         * @note Charges token_length work units before the re-scan; a string without
         * warnings is not re-scanned and not charged.
         * Reserves one diagnostic entry and one list slot per warning before recording any.
         * Failure may leave partial reservations; the engine must then be abandoned.
         */
        [[nodiscard]] status_result retain_warnings(std::size_t token_offset, std::size_t token_length,
                                                    std::size_t count) noexcept;

      public:
        /**
         * @brief Prepare private state without allocating parser storage.
         * @par Requirements
         * P033; J5-040; J5-047; J5-048.
         * @note accounting and its upstream provider must outlive returned data.
         * Input must remain alive and unchanged throughout run().
         */
        parser_engine(input_view input, const parse_options& options, accounted_storage& accounting) noexcept;

        /**
         * @brief Construct one root value and return its consumed prefix.
         * @par Requirements
         * P033; P034; P035; J5-013; J5-027; J5-040; J5-048.
         * @note Invoke once on a fresh engine.
         * Single use: after any result, construct a new engine to parse again.
         * Counts in an allocation-free pre-pass, reserves records and frames exactly once,
         * then builds; syntax and limit failures occur before any allocation (ADR 0003, decision 7).
         * This internal operation does not validate trailing input.
         * Every failure requires abandoning this engine instance.
         */
        [[nodiscard]] result<parsed_value_tree> run() noexcept;

        /**
         * @brief Construct one root and require complete input consumption.
         * @par Requirements
         * P037; J5-026; J5-049; J5-051.
         * @note Invoke once on a fresh engine instead of run().
         * Single use: after any result, construct a new engine to parse again.
         * Admits trailing trivia according to the selected syntax mode.
         * The pre-pass also checks trailing content, so it fails before any allocation.
         * Success sets consumed_bytes to the complete input size.
         * Failure returns no tree; discard this engine instance.
         */
        [[nodiscard]] result<parsed_value_tree> run_complete() noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_ENGINE_HPP_ */
