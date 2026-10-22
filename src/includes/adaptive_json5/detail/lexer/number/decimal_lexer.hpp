#ifndef DECIMAL_LEXER_HPP_
#define DECIMAL_LEXER_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/error.hpp"
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
     * @brief Check an optional byte for an ASCII decimal digit.
     * @par Requirements
     * P025; J5-021; J5-022.
     * @note Does not use locale-dependent character classification.
     */
    [[nodiscard]] inline constexpr bool is_decimal_digit(std::optional<char> byte) noexcept
    {
      return byte.has_value() && (*byte >= '0') && (*byte <= '9');
    }

    /**
     * @brief Consume consecutive ASCII decimal digits.
     * @par Requirements
     * P025; J5-021; J5-022.
     * @note Does not skip trivia or convert the digits into a number.
     */
    void consume_decimal_digits(cursor& source) noexcept;

    /**
     * @brief Locate a missing or invalid decimal grammar component.
     * @par Requirements
     * P025; J5-009; J5-021; J5-022.
     * @note Malformed UTF-8 at this position retains its encoding error.
     * Otherwise reports syntax at the current byte, including EOF.
     * The cursor is passed by value and the caller is unchanged.
     */
    [[nodiscard]] error decimal_expected_error(cursor source) noexcept;
  } // namespace detail

  /**
   * @brief Recognize a decimal numeric spelling without numeric conversion.
   * @par Requirements
   * P025; P032; J5-021; J5-022; J5-041; J5-047; J5-048.
   * @param source Cursor positioned at a possible number start.
   * @param mode Selected input syntax.
   * @param max_exponent_digits Inclusive exponent digit limit.
   * @return Source metadata, successful nullopt for no-match,
   * or an explicit located error.
   * @note No-match and failure leave source unchanged.
   * Does not skip trivia, allocate storage or require a terminating NUL.
   * Hexadecimal prefixes and non-finite names are left for other recognizers.
   * Complete suffix validation is a separate P028 operation.
   * Nonmatching input and unconsumed suffix bytes are not validated here.
   * Exponent signs are excluded from the digit count; leading zeros count.
   * Excess exponent digits report resource_limit at the first excess digit.
   * Zero permits exponent-free numbers but rejects any exponent digit.
   * The default permits any digit count representable by size_t.
   */
  result<std::optional<number_token>>
  consume_decimal(cursor& source, syntax_mode mode,
                  std::size_t max_exponent_digits = std::numeric_limits<std::size_t>::max()) noexcept;
} // namespace adaptive_json5

#endif /* DECIMAL_LEXER_HPP_ */
