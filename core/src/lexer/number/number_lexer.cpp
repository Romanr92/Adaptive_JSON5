#include "lexer/number/number_lexer.hpp"

#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/number/decimal_lexer.hpp"
#include "lexer/number/hexadecimal_lexer.hpp"
#include "lexer/number/non_finite_lexer.hpp"
#include "lexer/number/number_token.hpp"
#include "lexer/token_boundary.hpp"

#include <cstddef>
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
    result<std::optional<number_token>> consume_number_spelling(cursor& source, syntax_mode mode,
                                                                std::size_t max_exponent_digits) noexcept
    {
      auto hexadecimal = consume_hexadecimal(source, mode);
      if (!hexadecimal.has_value())
      {
        return hexadecimal;
      }

      if (hexadecimal.value_if()->has_value())
      {
        return hexadecimal;
      }

      auto non_finite = consume_non_finite(source, mode);
      if (!non_finite.has_value())
      {
        return non_finite;
      }

      if (non_finite.value_if()->has_value())
      {
        return non_finite;
      }

      return consume_decimal(source, mode, max_exponent_digits);
    }
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
  result<std::optional<number_token>> consume_number(cursor& source, syntax_mode mode,
                                                     std::size_t max_exponent_digits) noexcept
  {
    using number_result = result<std::optional<number_token>>;

    // Include boundary validation in the same private cursor transaction.
    cursor candidate = source;
    auto recognized = detail::consume_number_spelling(candidate, mode, max_exponent_digits);

    if (!recognized.has_value())
    {
      return recognized;
    }

    if (!recognized.value_if()->has_value())
    {
      return recognized;
    }

    const auto boundary = check_unquoted_token_boundary(candidate, mode);
    if (!boundary.has_value())
    {
      return number_result::failure(*boundary.error_if());
    }

    source = candidate;
    return recognized;
  }
} // namespace adaptive_json5
