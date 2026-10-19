#ifndef PARSER_ENGINE_HPP_
#define PARSER_ENGINE_HPP_

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/member_name_lexer.hpp"
#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/lexer/scalar_lexer.hpp"
#include "adaptive_json5/detail/lexer/scalar_token.hpp"
#include "adaptive_json5/detail/lexer/string/string_token.hpp"
#include "adaptive_json5/detail/lexer/string/string_warning.hpp"
#include "adaptive_json5/detail/lexer/trivia.hpp"
#include "adaptive_json5/detail/memory/resource_budget.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_attachment.hpp"
#include "adaptive_json5/detail/parser/parser_container_close.hpp"
#include "adaptive_json5/detail/parser/parser_container_open.hpp"
#include "adaptive_json5/detail/parser/parser_dispatch.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/detail/parser/parser_stack.hpp"
#include "adaptive_json5/detail/parser/primitive_parser.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Own one completed value tree and its consumed input prefix.
     * @par Requirements
     * P033; J5-013; J5-027; J5-040; J5-041.
     * @note The document has an established root.
     * consumed_bytes includes leading trivia and ends immediately after the root.
     * Remaining input has not been validated.
     * The caller's accounting object and storage provider must outlive values.
     */
    struct parsed_value_tree
    {
        document values;
        std::size_t consumed_bytes{};
        std::size_t warning_count{};
        std::optional<string_warning> first_warning{};
    };

    /**
     * @brief Construct a private value tree using iterative container frames.
     * @par Requirements
     * P033; J5-013; J5-027; J5-048.
     * @note Internal operation object; construct a fresh instance for each run.
     * Partial documents are never returned after failure.
     * No recursive descent is used.
     */
    class parser_engine
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
        std::size_t m_string_units{};
        std::size_t m_warning_count{};
        std::optional<string_warning> m_first_warning{};

        /**
         * @brief Start a container or construct and accept a scalar value.
         * @par Requirements
         * P033; J5-013; J5-027; J5-028; J5-048.
         * @note Reserve node/member resources before construction and depth
         * before growing container frames.
         * Scalar decoding uses the remaining aggregate string allowance.
         * Failure preserves the source cursor but may change private state;
         * the failed engine must be abandoned.
         * Failed container opening releases its matching reservations.
         * Successful work charges are not refunded.
         */
        status_result read_value() noexcept;

        /**
         * @brief Decode and retain the innermost object's next member name.
         * @par Requirements
         * P033; P035; J5-017; J5-019; J5-020; J5-048.
         * @note Limits lexical growth to the remaining decoded-unit allowance.
         * Success retains the name and selects object_colon.
         * Retains only the first warning descriptor and counts all warnings.
         * Failure preserves the source cursor and pending name.
         * Earlier budget reservations are not rolled back after later failure;
         * the failed engine must be abandoned.
         */
        status_result read_name() noexcept;

        /**
         * @brief Consume a dispatched colon or comma and update its frame.
         * @par Requirements
         * P033; J5-013; J5-027.
         * @note Validates action, state and punctuation before mutation.
         * Success consumes exactly one byte.
         * Failure preserves the cursor and frame.
         * Dispatch work is charged by run().
         */
        status_result consume_separator(parser_action action) noexcept;

        /**
         * @brief Close the innermost container and accept its completed value.
         * @par Requirements
         * P033; J5-013; J5-027; J5-048.
         * @note Successful closure records the span and removes the frame.
         * Releases depth before accepting the completed value.
         * Failure requires abandoning this engine; earlier internal mutations
         * are not rolled back after a later attachment failure.
         * The source cursor is committed only after all steps succeed.
         */
        status_result finish_container() noexcept;

        /**
         * @brief Attach a completed value or record the completed root.
         * @par Requirements
         * P033; J5-013; J5-027.
         * @note Reuse attach_parsed_value when a parent frame exists.
         * Otherwise record m_root without publishing the document yet.
         */
        status_result accept_completed_value(value_index completed) noexcept;

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
         * P033; J5-013; J5-027; J5-040; J5-048.
         * @note Invoke once on a fresh engine.
         * This internal operation does not validate trailing input.
         * Every failure requires abandoning this engine instance.
         */
        result<parsed_value_tree> run() noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_ENGINE_HPP_ */
