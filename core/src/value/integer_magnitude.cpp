#include "value/integer_magnitude.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "value/number_analysis.hpp"

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
   */
  result<std::uintmax_t> append_integer_digit(std::uintmax_t current, unsigned int digit, unsigned int radix,
                                              std::uintmax_t maximum) noexcept
  {
    using magnitude_result = result<std::uintmax_t>;

    constexpr unsigned int c_decimal_radix = 10;
    constexpr unsigned int c_hexadecimal_radix = 16;

    if (((radix != c_decimal_radix) && (radix != c_hexadecimal_radix)) || (digit >= radix))
    {
      return magnitude_result::failure(error::without_location(error_code::wrong_type_or_access));
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

    return magnitude_result::success((current * radix) + digit);
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
   */
  result<integer_magnitude> read_integer_magnitude(const owned_number& number, std::uintmax_t maximum,
                                                   work_budget& work) noexcept
  {
    using magnitude_result = result<integer_magnitude>;

    constexpr unsigned int c_hexadecimal_radix = 16;
    constexpr unsigned int c_first_hex_letter_value = 10;
    constexpr std::size_t c_hexadecimal_prefix_bytes = 2;

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
      // Validated hexadecimal spelling guarantees the prefix and its digits.
      position += c_hexadecimal_prefix_bytes;

      while (position < spelling.size)
      {
        const char current = spelling.data[position];
        ++position;

        unsigned int digit{};

        if ((current >= '0') && (current <= '9'))
        {
          digit = static_cast<unsigned int>(current - '0');
        }
        else if ((current >= 'a') && (current <= 'f'))
        {
          digit = static_cast<unsigned int>(current - 'a') + c_first_hex_letter_value;
        }
        else
        {
          // Validation guarantees that the remaining alternative is A throug F.
          digit = static_cast<unsigned int>(current - 'A') + c_first_hex_letter_value;
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

    constexpr unsigned int c_decimal_radix = 10;

    const std::size_t significand_start = position;

    std::size_t significand_end = spelling.size;
    std::size_t significand_digits{};
    std::size_t fractional_digits{};
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
        ++position;
        break;
      }

      if (current == '.')
      {
        after_point = true;
      }
      else
      {
        ++significand_digits;

        if (after_point)
        {
          ++fractional_digits;
        }
      }
      ++position;
    }

    bool exponent_negative{};
    std::uintmax_t exponent_magnitude{};

    if (has_exponent)
    {
      // Validated syntax guarantees a sign or digit after exponent marker.
      const char first_exponent = spelling.data[position];

      if ((first_exponent == '+') || (first_exponent == '-'))
      {
        exponent_negative = (first_exponent == '-');
        ++position;
      }

      constexpr auto c_maximum_exponent = std::numeric_limits<std::uintmax_t>::max();

      while (position < spelling.size)
      {
        const auto digit = static_cast<unsigned int>(spelling.data[position] - '0');
        ++position;

        const auto appended = append_integer_digit(exponent_magnitude, digit, c_decimal_radix, c_maximum_exponent);

        if (!appended.has_value())
        {
          // A nonzero integral value cannot cancel an exponent this large
          // using the fractional digits present in its bounded spelling.
          return magnitude_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
        }

        exponent_magnitude = *appended.value_if();
      }
    }

    std::size_t digits_to_keep = significand_digits;
    std::uintmax_t zeros_to_append{};

    if (exponent_negative)
    {
      // Check before adding so the number of removed digits cannot overflow.
      const std::size_t integer_digits = significand_digits - fractional_digits;

      if (exponent_magnitude > integer_digits)
      {
        return magnitude_result::failure(error::numeric_failure(numeric_conversion_reason::non_integral));
      }

      const std::size_t digits_to_remove = fractional_digits + static_cast<std::size_t>(exponent_magnitude);

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

      digits_to_keep -= digits_to_remove;
    }

    position = significand_start;
    std::size_t retained_digits{};

    while ((position < significand_end) && (retained_digits < digits_to_keep))
    {
      const char current = spelling.data[position];
      ++position;

      if (current == '.')
      {
        continue;
      }

      const auto digit = static_cast<unsigned int>(current - '0');

      const auto appended = append_integer_digit(converted.magnitude, digit, c_decimal_radix, maximum);

      if (!appended.has_value())
      {
        return magnitude_result::failure(*appended.error_if());
      }

      converted.magnitude = *appended.value_if();
      ++retained_digits;
    }

    constexpr unsigned int c_appended_zero_digits{};
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
} // namespace adaptive_json5::detail
