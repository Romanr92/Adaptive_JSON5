#ifndef DECIMAL_LEXER_HPP_
#define DECIMAL_LEXER_HPP_

#include "cursor.hpp"
#include "error.hpp"
#include "number_token.hpp"
#include "options.hpp"
#include "result.hpp"
#include "utf8.hpp"

#include <cstddef>
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
    inline constexpr bool is_decimal_digit(std::optional<char> byte) noexcept
    {
      return byte.has_value() && (*byte >= '0') && (*byte <= '9');
    }

    /**
     * @brief Consume consecutive ASCII decimal digits.
     * @par Requirements
     * P025; J5-021; J5-022.
     * @note Does not skip trivia or convert the digits into a number.
     */
    inline void consume_decimal_digits(cursor& source) noexcept
    {
      while (is_decimal_digit(source.peek()))
      {
        source.advance(1);
      }
    }

    /**
     * @brief Locate a missing or invalid decimal grammar component.
     * @par Requirements
     * P025; J5-009; J5-021; J5-022.
     * @note Malformed UTF-8 at this position retains its encoding error.
     * Otherwise reports syntax at the current byte, including EOF.
     * The cursor is passed by value and the caller is unchanged.
     */
    inline error decimal_expected_error(cursor source) noexcept
    {
      const std::size_t offset = source.offset();
      const auto decoded = decode_utf8(source);
      if (!decoded.has_value())
      {
        return *decoded.error_if();
      }

      return error::at_offset(error_code::syntax, offset);
    }
  } // namespace detail

  /**
   * @brief Recognize a decimal numeric spelling without numeric conversion.
   * @par Requirements
   * P025; J5-021; J5-022; J5-041.
   * @param source Cursor positioned at a possible number start.
   * @param mode Selected input syntax.
   * @return Source metadata, successful nullopt for no-match,
   * or an explicit located error.
   * @note No-match and failure leave source unchanged.
   * Does not skip trivia, allocate storage or require a terminating NUL.
   * Hexadecimal prefixes and non-finite names are left for other recognizers.
   * Complete suffix validation is a separate P028 operation.
   * Nonmatching input and unconsumed suffix bytes are not validated here.
   */
  inline result<std::optional<number_token>> consume_decimal(cursor& source, syntax_mode mode) noexcept
  {
    using decimal_result = result<std::optional<number_kind>>;

    const std::size_t start = source.offset();
    const auto initial = source.peek();

    if (!initial.has_value())
    {
      return decimal_result::success(std::nullopt);
    }

    const bool signed_start = (*initial == '+') || (*initial == '-');
    if (!signed_start && (*initial != '.') && !detail::is_decimal_digit(initial))
    {
      return decimal_result::success(std::nullopt);
    }

    // keep all grammar progress private until recognition succeeds
  }
} // namespace adaptive_json5

#endif /* DECIMAL_LEXER_HPP_ */
