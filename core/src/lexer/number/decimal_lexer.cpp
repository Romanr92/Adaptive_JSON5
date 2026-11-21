#include "lexer/number/decimal_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
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
     * @par ID SWU-LEXER-014
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
     * @par ID SWU-LEXER-015
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

  namespace
  {
    /**
     * @brief Consume an optional exponent and enforce the exponent-digit limit at the first excess digit.
     * @par Requirements
     * P025; P032; J5-021; J5-022; J5-041; J5-047; J5-048.
     * @par ID SWU-LEXER-058
     */
    status_result scan_decimal_exponent(cursor& candidate, std::size_t max_exponent_digits) noexcept
    {
      constexpr std::size_t c_ascii_byte_count = 1;
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
          if ((candidate.offset() - exponent_start) >= max_exponent_digits)
          {
            return status_result::failure(error::limit_at(candidate.offset(), resource_limit_reason::exponent_digits));
          }

          candidate.advance(c_ascii_byte_count);
        }

        if (candidate.offset() == exponent_start)
        {
          return status_result::failure(detail::decimal_expected_error(candidate));
        }
      }
      return status_result::success();
    }

    /**
     * @brief Consume the integer and fraction according to the selected syntax mode.
     * @par Requirements
     * P025; P032; J5-021; J5-022; J5-041; J5-047; J5-048.
     * @par ID SWU-LEXER-059
     */
    status_result scan_decimal_significand(cursor& candidate, syntax_mode mode) noexcept
    {
      constexpr std::size_t c_ascii_byte_count = 1;
      const auto first = candidate.peek();
      bool has_integer = false;

      if (detail::is_decimal_digit(first))
      {
        has_integer = true;

        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
        // between two character operands does not use the numeric value; the tool reports the implicit promotion
        // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
        // an ASCII-compatible execution character set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.62)
        if (*first == '0')
        {
          candidate.advance(c_ascii_byte_count);

          if (detail::is_decimal_digit(candidate.peek()))
          {
            return status_result::failure(error::syntax_at(candidate.offset(), syntax_error_reason::invalid_number));
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
          return status_result::failure(
            error::syntax_at(candidate.offset(), syntax_error_reason::not_allowed_in_strict_json));
        }

        candidate.advance(c_ascii_byte_count);
        const std::size_t fraction_start = candidate.offset();
        detail::consume_decimal_digits(candidate);

        // A leading point always needs digits; strict JSON also requires them after an integer part.
        if ((candidate.offset() == fraction_start) && (!has_integer || (mode == syntax_mode::strict_json)))
        {
          return status_result::failure(detail::decimal_expected_error(candidate));
        }
      }
      else if (!has_integer)
      {
        return status_result::failure(detail::decimal_expected_error(candidate));
      }
      else
      {
        // An integer part without a fraction needs no further checks here.
      }
      return status_result::success();
    }

    /**
     * @brief Recognize prefixes handled by the hexadecimal and non-finite scanners.
     * @par Requirements
     * P025; P032; J5-021; J5-022; J5-041; J5-047; J5-048.
     * @par ID SWU-LEXER-060
     */
    bool has_other_number_prefix(cursor candidate) noexcept
    {
      constexpr std::size_t c_ascii_byte_count = 1;
      const auto first = candidate.peek();

      // Let the non-finite recognizer inspect the complete original spelling.
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison between
      // two character operands does not use the numeric value; the tool reports the implicit promotion of both
      // operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-
      // compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.63)
      if (first.has_value() && ((*first == 'I') || (*first == 'N')))
      {
        return true;
      }

      // A hexadecimal prefix must not be consumed as the decimal token zero.
      if (first == std::optional<char>{'0'})
      {
        const auto next = candidate.peek(c_ascii_byte_count);
        if ((next == std::optional<char>{'x'}) || (next == std::optional<char>{'X'}))
        {
          return true;
        }
      }
      return false;
    }
  } // namespace

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
   * Excess exponent digits report resource_limit (exponent_digits) at the first excess digit.
   * Zero permits exponent-free numbers but rejects any exponent digit.
   * The default permits any digit count representable by size_t.
   * @par ID SWU-LEXER-016
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

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Tests whether the byte equals one specific character. An equality comparison between
    // two character operands does not use the numeric value; the tool reports the implicit promotion of both
    // operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-
    // compatible execution character set, where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.66)
    const bool signed_start = (*initial == '+') || (*initial == '-');
    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Tests whether the byte equals one specific character. An equality comparison between
    // two character operands does not use the numeric value; the tool reports the implicit promotion of both
    // operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-
    // compatible execution character set, where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.65)
    if (!signed_start && (*initial != '.') && !detail::is_decimal_digit(initial))
    {
      return decimal_result::success(std::nullopt);
    }

    // keep all grammar progress private until recognition succeeds.
    cursor candidate = source;

    if (signed_start)
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.64)
      if ((*initial == '+') && (mode == syntax_mode::strict_json))
      {
        return decimal_result::failure(error::syntax_at(start, syntax_error_reason::not_allowed_in_strict_json));
      }

      candidate.advance(c_ascii_byte_count);
    }

    if (has_other_number_prefix(candidate))
    {
      return decimal_result::success(std::nullopt);
    }

    const auto digits = scan_decimal_significand(candidate, mode);
    if (!digits.has_value())
    {
      return decimal_result::failure(*digits.error_if());
    }

    const auto exponent = scan_decimal_exponent(candidate, max_exponent_digits);
    if (!exponent.has_value())
    {
      return decimal_result::failure(*exponent.error_if());
    }

    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: A cursor only moves forward, so the later offset is never below the earlier offset
    // it started from, and the difference is the number of bytes consumed.
    // codeql::misra_deviation_next_line(DEV-004.114)
    const number_token token{number_kind::decimal, start, candidate.offset() - start};
    auto completed = decimal_result::success(std::optional<number_token>{token});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5
