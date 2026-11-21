#include "value/integer_magnitude.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "value/number_analysis.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace adaptive_json5::detail
{
  /**
   * @brief Append a digit after checking the destination magnitude bound.
   * @par Requirements
   * P038; J5-003; J5-035.
   * @param current Previously accumulated magnitude.
   * @param digit Numeric digit value below radix.
   * @param radix Decimal or hexadecimal base.
   * @param maximum Inclusive permitted magnitude.
   * @return Exact updated magnitude or an explicit error.
   * @note Checks precede arithmetic that could overflow. Performs no allocation.
   * @par ID SWU-VALUE-056
   */
  result<std::uintmax_t> append_integer_digit(std::uintmax_t current, std::uint32_t digit, std::uint32_t radix,
                                              std::uintmax_t maximum) noexcept
  {
    using magnitude_result = result<std::uintmax_t>;

    constexpr std::uint32_t c_decimal_radix = 10;
    constexpr std::uint32_t c_hexadecimal_radix = 16;

    if (((radix != c_decimal_radix) && (radix != c_hexadecimal_radix)) || (digit >= radix))
    {
      return magnitude_result::failure(error::internal_fault());
    }

    // Check before subtracting so an eversized digit cannot cause unsigned wrap.
    if (digit > maximum)
    {
      return magnitude_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
    }

    // Rearrange current * radix + digit <= maximum before doing the multiplication.
    if (current > ((maximum - digit) / radix))
    {
      return magnitude_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
    }

    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: The check above returns when current * radix + digit would exceed maximum, so the
    // expression stays within maximum.
    // codeql::misra_deviation_next_line(DEV-004.60)
    return magnitude_result::success((current * radix) + digit);
  }

  /**
   * @brief Append the retained decimal digits and charge each required zero extension.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048.
   * @par ID SWU-VALUE-205
   */
  static result<integer_magnitude> accumulate_decimal_magnitude(input_view spelling, std::size_t position,
                                                                std::size_t significand_end, std::size_t digits_to_keep,
                                                                std::uintmax_t zeros_to_append,
                                                                integer_magnitude converted, std::uintmax_t maximum,
                                                                work_budget& work) noexcept
  {
    using magnitude_result = result<integer_magnitude>;
    constexpr std::uint32_t c_decimal_radix = 10;

    std::size_t retained_digits{};

    while ((position < significand_end) && (retained_digits < digits_to_keep))
    {
      const char current = spelling.data[position];
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
      // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.48)
      ++position;

      if (current == '.')
      {
        continue;
      }

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a value
      // 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8
      // input bytes with character literals, so it relies on an ASCII-compatible execution character set, where
      // these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.12)
      const auto digit = static_cast<std::uint32_t>(current - '0');

      const auto appended = append_integer_digit(converted.magnitude, digit, c_decimal_radix, maximum);

      if (!appended.has_value())
      {
        return magnitude_result::failure(*appended.error_if());
      }

      converted.magnitude = *appended.value_if();
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so the
      // count is at most its length and cannot wrap std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.47)
      ++retained_digits;
    }

    constexpr std::uint32_t c_appended_zero_digits{};
    constexpr std::size_t c_zero_append_work_units = 1;

    while (zeros_to_append != 0)
    {
      // Exponent-implied digits are additional work beyond scanning the spelling.
      const auto zero_change = work.charge(c_zero_append_work_units);

      if (!zero_change.has_value())
      {
        return magnitude_result::failure(*zero_change.error_if());
      }

      const auto appended = append_integer_digit(converted.magnitude, c_appended_zero_digits, c_decimal_radix, maximum);

      if (!appended.has_value())
      {
        return magnitude_result::failure(*appended.error_if());
      }

      converted.magnitude = *appended.value_if();
      --zeros_to_append;
    }
    return magnitude_result::success(converted);
  }

  /**
   * @brief Read a signed decimal exponent with checked magnitude accumulation.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048.
   * @par ID SWU-VALUE-206
   */
  static status_result scan_integer_exponent(input_view spelling, std::size_t position, bool& exponent_negative,
                                             std::uintmax_t& exponent_magnitude) noexcept
  {
    constexpr std::uint32_t c_decimal_radix = 10;
    // Validated syntax guarantees a sign or digit after exponent marker.
    const char first_exponent = spelling.data[position];

    if ((first_exponent == '+') || (first_exponent == '-'))
    {
      exponent_negative = (first_exponent == '-');
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
      // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.54)
      ++position;
    }

    constexpr auto c_maximum_exponent = std::numeric_limits<std::uintmax_t>::max();

    while (position < spelling.size)
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
      // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares
      // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character
      // set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.13)
      const auto digit = static_cast<std::uint32_t>(spelling.data[position] - '0');
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
      // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.53)
      ++position;

      const auto appended = append_integer_digit(exponent_magnitude, digit, c_decimal_radix, c_maximum_exponent);

      if (!appended.has_value())
      {
        // A nonzero integral value cannot cancel an exponent this large
        // using the fractional digits present in its bounded spelling.
        return status_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      exponent_magnitude = *appended.value_if();
    }
    return status_result::success();
  }

  /**
   * @brief Locate the significand boundary and count its decimal and fractional digits.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048.
   * @par ID SWU-VALUE-207
   */
  static bool scan_integer_significand(input_view spelling, std::size_t& position, std::size_t& significand_end,
                                       std::size_t& significand_digits, std::size_t& fractional_digits) noexcept
  {
    bool after_point{};
    bool has_exponent{};

    while (position < spelling.size)
    {
      const char current = spelling.data[position];

      if ((current == 'e') || (current == 'E'))
      {
        // Retain the boundary so the later accumulation never reads exponent digits.
        significand_end = position;
        has_exponent = true;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
        // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
        // codeql::misra_deviation_next_line(DEV-004.58)
        ++position;
        break;
      }

      if (current == '.')
      {
        after_point = true;
      }
      else
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so the
        // count is at most its length and cannot wrap std::size_t.
        // codeql::misra_deviation_next_line(DEV-004.57)
        ++significand_digits;

        if (after_point)
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so the
          // count is at most its length and cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.56)
          ++fractional_digits;
        }
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
      // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.55)
      ++position;
    }
    return has_exponent;
  }

  /**
   * @brief Accumulate validated hexadecimal digits within the requested integer bound.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048.
   * @par ID SWU-VALUE-208
   */
  static result<integer_magnitude> read_hex_magnitude(input_view spelling, std::size_t position,
                                                      integer_magnitude converted, std::uintmax_t maximum) noexcept
  {
    using magnitude_result = result<integer_magnitude>;
    constexpr std::uint32_t c_hexadecimal_radix = 16;
    constexpr std::array<std::uint32_t, 6> c_hex_letter_values{10U, 11U, 12U, 13U, 14U, 15U};
    constexpr std::size_t c_hexadecimal_prefix_bytes = 2;
    // Validated hexadecimal spelling guarantees the prefix and its digits.
    position += c_hexadecimal_prefix_bytes;

    while (position < spelling.size)
    {
      const char current = spelling.data[position];
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
      // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.59)
      ++position;

      // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
      // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
      // every path assigns the real value before use. For constants and results, the value is read later in
      // the same function or template instantiation, which the tool does not track.
      // codeql::misra_deviation_next_line(DEV-007.22)
      std::uint32_t digit{};

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Classifies the byte with a range check on ASCII digits or letters; the result is
      // only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals,
      // so it relies on an ASCII-compatible execution character set, where these characters are contiguous
      // ranges.
      // codeql::misra_deviation_next_line(DEV-015.18)
      if ((current >= '0') && (current <= '9'))
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser
        // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
        // character set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.17)
        digit = static_cast<std::uint32_t>(current - '0');
      }
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Classifies the byte with a range check on ASCII digits or letters; the result is
      // only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals,
      // so it relies on an ASCII-compatible execution character set, where these characters are contiguous
      // ranges.
      // codeql::misra_deviation_next_line(DEV-015.16)
      else if ((current >= 'a') && (current <= 'f'))
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser
        // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
        // character set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.15)
        digit = c_hex_letter_values[static_cast<std::size_t>(current - 'a')];
      }
      else
      {
        // Validation guarantees that the remaining alternative is A through F.
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser
        // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
        // character set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.14)
        digit = c_hex_letter_values[static_cast<std::size_t>(current - 'A')];
      }

      const auto appended = append_integer_digit(converted.magnitude, digit, c_hexadecimal_radix, maximum);

      if (!appended.has_value())
      {
        return magnitude_result::failure(*appended.error_if());
      }

      converted.magnitude = *appended.value_if();
    }

    return magnitude_result::success(converted);
  }

  /**
   * @brief Determine retained and appended digits from the scale before accumulating the integer.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048.
   * @par ID SWU-VALUE-209
   */
  static result<integer_magnitude> read_decimal_magnitude(input_view spelling, std::size_t position,
                                                          integer_magnitude converted, std::uintmax_t maximum,
                                                          work_budget& work) noexcept
  {
    using magnitude_result = result<integer_magnitude>;
    const std::size_t significand_start = position;

    std::size_t significand_end = spelling.size;
    std::size_t significand_digits{};
    std::size_t fractional_digits{};
    const bool has_exponent =
      scan_integer_significand(spelling, position, significand_end, significand_digits, fractional_digits);

    bool exponent_negative{};
    std::uintmax_t exponent_magnitude{};

    if (has_exponent)
    {
      const auto scanned = scan_integer_exponent(spelling, position, exponent_negative, exponent_magnitude);
      if (!scanned.has_value())
      {
        return magnitude_result::failure(*scanned.error_if());
      }
    }

    std::size_t digits_to_keep = significand_digits;
    std::uintmax_t zeros_to_append{};

    if (exponent_negative)
    {
      // Check before adding so the number of removed digits cannot overflow.
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Fractional digits are a subset of the significand digits, so the difference cannot
      // wrap.
      // codeql::misra_deviation_next_line(DEV-004.52)
      const std::size_t integer_digits = significand_digits - fractional_digits;

      if (exponent_magnitude > integer_digits)
      {
        return magnitude_result::failure(error::numeric_failure(numeric_conversion_reason::non_integral));
      }

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The check above ensures exponent_magnitude is at most the integer digits, so the
      // sum is at most significand_digits.
      // codeql::misra_deviation_next_line(DEV-004.51)
      const std::size_t digits_to_remove = fractional_digits + static_cast<std::size_t>(exponent_magnitude);

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: digits_to_remove is at most significand_digits, which is the initial value of
      // digits_to_keep.
      // codeql::misra_deviation_next_line(DEV-004.50)
      digits_to_keep -= digits_to_remove;
    }
    else if (exponent_magnitude >= fractional_digits)
    {
      zeros_to_append = exponent_magnitude - fractional_digits;
    }
    else
    {
      // This case is safe because of the exponent is below a size_t digit count.
      const std::size_t digits_to_remove = fractional_digits - static_cast<std::size_t>(exponent_magnitude);

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: digits_to_remove is at most significand_digits, which is the initial value of
      // digits_to_keep.
      // codeql::misra_deviation_next_line(DEV-004.49)
      digits_to_keep -= digits_to_remove;
    }

    return accumulate_decimal_magnitude(spelling, significand_start, significand_end, digits_to_keep, zeros_to_append,
                                        converted, maximum, work);
  }

  /**
   * @brief Read an exact integer magnitude while retaining its original sign.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048.
   * @param number Borrowed validated numeric spelling.
   * @param maximum Inclusive permitted unsigned magnitude.
   * @param work Shared budget charged before spelling processing.
   * @return Exact magnitude and sign, or an explicit error.
   * @note Allocates nothing and never modifies the number.
   * @par ID SWU-VALUE-057
   */
  result<integer_magnitude> read_integer_magnitude(const owned_number& number, std::uintmax_t maximum,
                                                   work_budget& work) noexcept
  {
    using magnitude_result = result<integer_magnitude>;

    const auto analysed = analyse_number(number, work);

    if (!analysed.has_value())
    {
      return magnitude_result::failure(*analysed.error_if());
    }

    const auto& analysis = *analysed.value_if();

    if ((analysis.kind == number_kind::infinity) || (analysis.kind == number_kind::nan))
    {
      return magnitude_result::failure(error::numeric_failure(numeric_conversion_reason::non_finite));
    }

    if (!analysis.is_integral)
    {
      return magnitude_result::failure(error::numeric_failure(numeric_conversion_reason::non_integral));
    }

    // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
    // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
    // implicit zero of the initialization as an unsuffixed octal literal.
    // codeql::misra_deviation_next_line(DEV-010.03)
    integer_magnitude converted{};
    converted.negative = analysis.negative;

    // Zero fits every unsigned magnitude bound, including a zero bound.
    if (analysis.is_zero)
    {
      return magnitude_result::success(converted);
    }

    const auto spelling = number.spelling();
    const auto charged = work.charge(spelling.size);

    if (!charged.has_value())
    {
      return magnitude_result::failure(*charged.error_if());
    }

    std::size_t position{};

    const char first = spelling.data[position];

    if ((first == '+') || (first == '-'))
    {
      ++position;
    }

    if (analysis.kind == number_kind::hexadecimal)
    {
      return read_hex_magnitude(spelling, position, converted, maximum);
    }

    return read_decimal_magnitude(spelling, position, converted, maximum, work);
  }
} // namespace adaptive_json5::detail
