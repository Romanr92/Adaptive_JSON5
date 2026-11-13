#ifndef STRING_LEXER_HPP_
#define STRING_LEXER_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/string/string_boundary.hpp"
#include "lexer/string/string_token.hpp"

#include <cstddef>
#include <limits>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Deliver warnings from an already validated string body.
     * @par Requirements
     * P021; J5-017.
     * @param source Copy of the cursor immediately after the opening quote.
     * @note Requires the same unchanged input validated by the first pass.
     * The observer must satisfy string_warning_observer's lifetime contract.
     * Performs no allocation and does not modify the caller's cursor.
     * @par ID SWU-LEXER-039
     */
    [[nodiscard]] status_result notify_string_warnings(cursor source, string_quote quote, syntax_mode mode,
                                                       const string_warning_observer& observer) noexcept;
  } // namespace detail

  /**
   * @brief Validate one complete quoted string and count its units without allocating.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-041; J5-046; J5-047.
   * @param source Cursor positioned at a possible opening quote.
   * @param mode Selected input syntax.
   * @param max_units Maximum decoded UTF-16 units.
   * @param warning_allowance Warnings that still fit the caller's diagnostic budget; the default never limits.
   * @return The measurement, successful nullopt before nonquote input or EOF,
   * or the same located failure consume_string reports for the same input.
   * @note Success consumes through the matching closing quote only.
   * No-match and failure leave the caller's cursor unchanged.
   * Performs no allocation and invokes no warning callbacks (ADR 0003, decisions 2 and 7).
   * Does not skip trivia or validate input following the closing quote.
   * More warnings than warning_allowance are not a failure: the measurement carries the
   * first excess one as excess_warning, so the caller can report its exact offset (issue #125).
   * @par ID SWU-LEXER-040
   */
  [[nodiscard]] result<std::optional<string_measure>>
  measure_string(cursor& source, syntax_mode mode, std::size_t max_units,
                 std::size_t warning_allowance = std::numeric_limits<std::size_t>::max()) noexcept;

  /**
   * @brief Consume one complete quoted string into owned decoded storage.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-041; J5-046.
   * @param source Cursor positioned at a possible opening quote.
   * @param mode Selected input syntax.
   * @param storage Caller-owned fallible storage provider.
   * @param max_units Maximum decoded UTF-16 unit capacity.
   * @param observer Optional observer for all successful-string warnings.
   * @param sink Optional parser sink: reserve after the string is allocated, record during decoding.
   * @return Owned token, successful nullopt before nonquote input or EOF,
   * or an explicit located failure.
   * @note Success consumes through the matching closing quote only.
   * No-match and failure leave the caller's cursor unchanged.
   * Counts units first, then makes one allocation of exactly that size (ADR 0003, decision 2).
   * Partial owned storage is released on failure.
   * No warning callbacks occur for a string that fails validation or storage.
   * Input must remain unchanged throughout both passes and callbacks.
   * Observer callbacks must satisfy string_warning_observer's contract.
   * Does not skip trivia or validate input following the closing quote.
   * max_units is a component limit, not complete parser memory accounting.
   * @par ID SWU-LEXER-041
   */
  [[nodiscard]] result<std::optional<string_token>>
  consume_string(cursor& source, syntax_mode mode, const storage_callbacks& storage, std::size_t max_units,
                 string_warning_observer observer = {}, const string_warning_sink* sink = nullptr) noexcept;
} // namespace adaptive_json5

#endif /* STRING_LEXER_HPP_ */
