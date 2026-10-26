#ifndef PRIMITIVE_PARSER_HPP_
#define PRIMITIVE_PARSER_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "input/cursor.hpp"
#include "lexer/scalar_token.hpp"
#include "memory/work_budget.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Describe one constructed primitive and its lexical metadata.
   * @par Requirements
   * P032; J5-017; J5-028; J5-041.
   * @note index belongs to the destination document.
   * Source offsets refer to the original input without retaining that input.
   * Warning metadata follows the recognized string token.
   * Nonstring primitives have no string warnings.
   */
  struct [[nodiscard]] primitive_value
  {
      value_index index{};
      std::size_t byte_offset{};
      std::size_t byte_length{};
      std::size_t warning_count{};
      std::optional<string_warning> first_warning{};
  };

  /**
   * @brief Own a complete primitive document and its root metadata.
   * @par Requirements
   * P032; J5-017; J5-028; J5-040; J5-041.
   * @note root.index identifies the established root in values.
   * Moving transfers ownership; acquire views from the destination afterward.
   * All storage provider states must outlive the owned allocations.
   * Original input is not retained.
   */
  struct [[nodiscard]] primitive_document
  {
      document values;
      primitive_value root{};
  };

  namespace detail
  {
    /**
     * @brief Transfer one completed primitive token into a detached value.
     * @par Requirements
     * P032; J5-023; J5-028; J5-034; J5-040.
     * @param destination Document receiving the detached value.
     * @param token Completed token, consumed on entry.
     * @param input Original input used to recognize the token.
     * @param options Syntax and numeric spelling limit.
     * @param storage Provider for the owned numeric spelling.
     * @return New value index or an explicit failure.
     * @note Tokens must originate from the scalar lexer using this input
     * and syntax mode. Input must remain alive and unchanged during the call.
     * String content is transferred; numeric spelling is independently copied.
     * Provider state must outlive any transferred or copied allocation.
     * Failure leaves existing document values and its root unchanged.
     * Consumed string content may be released on failure.
     * Does not establish a root, advance a cursor or parse containers.
     * Source spans and string-warning publication are handled by composition.
     * Does not itself integrate owned-byte or work accounting.
     */
    [[nodiscard]] result<value_index> append_primitive_token(document& destination, scalar_token token,
                                                             input_view input, const parse_options& options,
                                                             const storage_callbacks& storage) noexcept;
  } // namespace detail

  /**
   * @brief Recognize and construct one detached primitive using shared work accounting.
   * @par Requirements
   * P032; J5-017; J5-023; J5-028; J5-040; J5-041; J5-047; J5-049.
   * @param destination Document receiving the constructed value.
   * @param source Cursor positioned after caller-managed leading trivia.
   * @param input Exact original input underlying source.
   * @param options Syntax, string-content and numeric component limits.
   * @param storage Provider for lexical storage and owned numeric spelling.
   * @param work Shared work budget for the enclosing operation.
   * @return Constructed primitive, successful nullopt at EOF or structural
   * punctuation, or an explicit failure.
   * @note Input must remain alive and unchanged throughout this call.
   * No-match and failure leave source and destination values unchanged.
   * Successful work charges remain consumed on no-match or failure.
   * Success appends one detached value and advances source past its token.
   * Does not set the root, skip trivia or require complete input consumption.
   * Containers are not parsed; structural punctuation remains unconsumed.
   * Warning metadata is returned without invoking warning callbacks.
   * Record source spans are not assigned by this component.
   * Each string is bounded by both per-string and total string limits.
   * Repeated calls do not share aggregate string or diagnostic accounting.
   * Temporary identifier units are bounded separately by input size.
   * A retained first-warning descriptor consumes one diagnostic entry.
   * warning_count summarizes occurrences without retaining additional entries.
   * Diagnostic checks bound returned metadata, not temporary lexical metadata.
   * Recognition scans within the remaining work and charges the bytes it reads (ADR 0003, decision 5).
   * Construction charges token bytes plus c_record_construction_work.
   * Construction charges conservatively include possible record relocation.
   * These are phase-accounting units, not measured execution time.
   */
  [[nodiscard]] result<std::optional<primitive_value>> consume_primitive(document& destination, cursor& source,
                                                                         input_view input, const parse_options& options,
                                                                         const storage_callbacks& storage,
                                                                         work_budget& work) noexcept;

  /**
   * @brief Parse exactly one primitive root with surrounding mode-valid trivia.
   * @par Requirements
   * P032; J5-009; J5-023; J5-027; J5-028; J5-040; J5-041; J5-046; J5-047; J5-049.
   * @param input Borrowed complete input, unchanged throughout this call.
   * @param options Syntax and configured component limits.
   * @param accounting Caller-owned allocation accounting with a stable address.
   * @return Complete owned primitive document or an explicit failure.
   * @note accounting and its provider state must outlive the returned document.
   * Its byte ceiling must equal options.limits.max_owned_bytes.
   * Existing live allocations through accounting share that ceiling.
   * Rejects empty input, additional tokens and container roots.
   * Failure releases this call's temporary allocations and publishes no document.
   * Does not invoke warning callbacks; root metadata retains warning information.
   * Checks input bytes and uses existing node/string/numeric component limits.
   * Lexical, numeric-spelling and document-record allocations use accounting.
   * The single root string obeys per-string and total decoded-unit limits.
   * Decimal exponent digits are checked during recognition.
   * The optional first-warning descriptor obeys the retained diagnostic limit.
   * Each trivia scan runs within the remaining work and charges the bytes it reads.
   * Primitive recognition and construction charge the same shared work budget.
   * Charges are conservative phase allowances, not measured execution time.
   * Successful charges remain consumed when a later phase fails.
   * Cleanup releases owned storage without requiring additional work charges.
   * The root span is stored in its value record and returned root metadata.
   */
  [[nodiscard]] result<primitive_document> parse_primitive_document(input_view input, const parse_options& options,
                                                                    accounted_storage& accounting) noexcept;
} // namespace adaptive_json5

#endif /* PRIMITIVE_PARSER_HPP_ */
