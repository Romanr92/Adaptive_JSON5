#include "lexer/number/decimal_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/number/number_token.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Consume consecutive ASCII decimal digits.
     * @par Requirements
     * P025; J5-021; J5-022.
     * @note Does not skip trivia or convert the digits into a number.
     */
    void consume_decimal_digits(cursor& source) noexcept
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
    error decimal_expected_error(cursor source) noexcept
    {
      const std::size_t offset = source.offset();
      const auto decoded = decode_utf8(source);
      if (!decoded.has_value())
      {
        return *decoded.error_if();
      }

      // A missing component at end of input is truncation, not a malformed spelling.
      return error::syntax_at(offset, decoded.value_if()->has_value() ? syntax_error_reason::invalid_number
                                                                      : syntax_error_reason::unexpected_end);
    }
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
  result<std::optional<number_token>> consume_decimal(cursor& source, syntax_mode mode,
                                                      std::size_t max_exponent_digits) noexcept
  {
    using decimal_result = result<std::optional<number_token>>;
    constexpr std::size_t c_ascii_byte_count = 1;

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

    // keep all grammar progress private until recognition succeeds.
    cursor candidate = source;

    if (signed_start)
    {
      if ((*initial == '+') && (mode == syntax_mode::strict_json))
      {
        return decimal_result::failure(error::syntax_at(start, syntax_error_reason::not_allowed_in_strict_json));
      }

      candidate.advance(c_ascii_byte_count);
    }

    const auto first = candidate.peek();

    // Let the non-finite recognizer inspect the complete original spelling.
    if (first.has_value() && ((*first == 'I') || (*first == 'N')))
    {
      return decimal_result::success(std::nullopt);
    }

    // A hexadecimal prefix must not be consumed as the decimal token zero.
    if (first == std::optional<char>{'0'})
    {
      const auto next = candidate.peek(c_ascii_byte_count);
      if ((next == std::optional<char>{'x'}) || (next == std::optional<char>{'X'}))
      {
        return decimal_result::success(std::nullopt);
      }
    }

    bool has_integer = false;

    if (detail::is_decimal_digit(first))
    {
      has_integer = true;

      if (*first == '0')
      {
        candidate.advance(c_ascii_byte_count);

        if (detail::is_decimal_digit(candidate.peek()))
        {
          return decimal_result::failure(error::syntax_at(candidate.offset(), syntax_error_reason::invalid_number));
        }
      }
      else
      {
        detail::consume_decimal_digits(candidate);
      }
    }

    if (candidate.peek() == std::optional<char>{'.'})
    {
      if (!has_integer && (mode == syntax_mode::strict_json))
      {
        return decimal_result::failure(
          error::syntax_at(candidate.offset(), syntax_error_reason::not_allowed_in_strict_json));
      }

      candidate.advance(c_ascii_byte_count);
      const std::size_t fraction_start = candidate.offset();
      detail::consume_decimal_digits(candidate);

      // A leading point always needs digits; strict JSON also requires digits the after an integer.
      if ((candidate.offset() == fraction_start) && (!has_integer || (mode == syntax_mode::strict_json)))
      {
        return decimal_result::failure(detail::decimal_expected_error(candidate));
      }
    }
    else if (!has_integer)
    {
      return decimal_result::failure(detail::decimal_expected_error(candidate));
    }

    const auto exponent = candidate.peek();
    if ((exponent == std::optional<char>{'e'}) || (exponent == std::optional<char>{'E'}))
    {
      candidate.advance(c_ascii_byte_count);

      const auto sign = candidate.peek();
      if ((sign == std::optional<char>{'+'}) || (sign == std::optional<char>{'-'}))
      {
        candidate.advance(c_ascii_byte_count);
      }

      const std::size_t exponent_start = candidate.offset();

      while (detail::is_decimal_digit(candidate.peek()))
      {
        // Reject the first excess digit before consuming it.
        if (candidate.offset() - exponent_start >= max_exponent_digits)
        {
          return decimal_result::failure(error::at_offset(error_code::resource_limit, candidate.offset()));
        }

        candidate.advance(c_ascii_byte_count);
      }

      if (candidate.offset() == exponent_start)
      {
        return decimal_result::failure(detail::decimal_expected_error(candidate));
      }
    }

    const number_token token{number_kind::decimal, start, candidate.offset() - start};
    auto completed = decimal_result::success(std::optional<number_token>{token});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5
