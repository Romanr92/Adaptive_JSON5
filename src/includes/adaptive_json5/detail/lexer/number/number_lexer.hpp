#ifndef NUMBER_LEXER_HPP_
#define NUMBER_LEXER_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <limits>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Select a numeric spelling recognizer without skipping input.
     * @par Requirements
     * P028; P032; J5-021; J5-022; J5-024; J5-047.
     * @param source Cursor used by the selected spelling recognizer.
     * @param mode Selected input syntax.
     * @param max_exponent_digits Inclusive decimal exponent digit limit.
     * @return Recognized numeric metadata, no-match, or an explicit error.
     * @note A recognizer's failure is final and must not trigger fallback.
     * No-match leaves source unchanged before trying another recognizer.
     * This helper does not establish a complete token boundary.
     * Exponent limits apply only to decimal spellings.
     */
    [[nodiscard]] result<std::optional<number_token>>
    consume_number_spelling(cursor& source, syntax_mode mode,
                            std::size_t max_exponent_digits = std::numeric_limits<std::size_t>::max()) noexcept;
  } // namespace detail

  /**
   * @brief Recognize one numeric token and validate its immediate boundary.
   * @par Requirements
   * P028; P032; J5-009; J5-021; J5-022; J5-024; J5-041; J5-047.
   * @param source Cursor positioned at a possible numeric token.
   * @param mode Selected input syntax.
   * @param max_exponent_digits Inclusive decimal exponent digit limit.
   * @return Numeric metadata, successful nullopt for no-match,
   * or an explicit located error.
   * @note No-match and failure leave source unchanged.
   * Requires no allocation, conversion or terminating NUL.
   * Does not skip trivia or join spellings across whitespace/comments.
   * Success leaves following punctuation and trivia unconsumed.
   * Later trivia and document/container syntax require separate validation.
   * Source metadata does not own the numeric spelling.
   * Excess exponent digits preserve the decimal recognizer's resource error.
   * The default permits any exponent digit count representable by size_t.
   */
  [[nodiscard]] result<std::optional<number_token>>
  consume_number(cursor& source, syntax_mode mode,
                 std::size_t max_exponent_digits = std::numeric_limits<std::size_t>::max()) noexcept;
} // namespace adaptive_json5

#endif /* NUMBER_LEXER_HPP_ */
