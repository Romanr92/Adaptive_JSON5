#include "adaptive_json5/value/floating_conversion.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/value/big_unsigned.hpp"
#include "adaptive_json5/detail/value/floating_special.hpp"
#include "adaptive_json5/detail/value/number_analysis.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace adaptive_json5
{
  namespace
  {
    constexpr unsigned int c_decimal_radix = 10;
    constexpr unsigned int c_hexadecimal_radix = 16;
    constexpr unsigned int c_hexadecimal_letter_offset = 10;
    constexpr std::size_t c_hexadecimal_prefix_bytes = 2;

    // Exceeds every supported decimal range, so a saturated exponent is certainly out of range.
    constexpr std::size_t c_exponent_margin = 2048;

    /**
     * @brief Describe an exact value as significand digits times a power of ten.
     * @par Requirements
     * P038; J5-037.
     * @note The significand itself is accumulated separately in a big_unsigned.
     */
    struct decimal_parts
    {
        bool scale_negative{};         // Sign of the power-of-ten exponent.
        std::size_t scale_magnitude{}; // Absolute power-of-ten exponent, possibly saturated.
        std::size_t digit_count{};     // Upper bound on significand decimal digits.
    };

    /**
     * @brief Convert one validated hexadecimal digit character to its value.
     * @par Requirements
     * P038; J5-037.
     * @param digit Validated hexadecimal digit.
     * @return Digit value from 0 to 15.
     * @note Uses explicit ranges rather than locale-dependent classification.
     */
    [[nodiscard]] unsigned int hexadecimal_digit_value(char digit) noexcept
    {
      if ((digit >= '0') && (digit <= '9'))
      {
        return static_cast<unsigned int>(digit - '0');
      }

      if ((digit >= 'a') && (digit <= 'f'))
      {
        return static_cast<unsigned int>(digit - 'a') + c_hexadecimal_letter_offset;
      }

      return static_cast<unsigned int>(digit - 'A') + c_hexadecimal_letter_offset;
    }

    /**
     * @brief Count significant bits of a small quotient.
     * @par Requirements
     * P038; J5-037.
     * @param value Quotient or rounded significand.
     * @return Zero for zero; otherwise the highest set bit position plus one.
     */
    [[nodiscard]] std::size_t bit_length_of(std::uint_least64_t value) noexcept
    {
      std::size_t bits{};

      while (value != 0)
      {
        ++bits;
        value >>= 1U;
      }

      return bits;
    }

    /**
     * @brief Accumulate significand digits and the combined decimal exponent.
     * @par Requirements
     * P038; J5-003; J5-037; J5-048.
     * @param spelling Validated finite numeric spelling.
     * @param kind Decimal or hexadecimal spelling family.
     * @param digits Exact zero receiving every significand digit.
     * @param work Shared conversion work budget.
     * @return Power-of-ten scale or an explicit resource/storage failure.
     * @note Hexadecimal spellings are integers and always have zero scale.
     * Exponents saturate instead of overflowing; saturated values are certainly out of range.
     */
    [[nodiscard]] result<decimal_parts> read_significand(input_view spelling, number_kind kind,
                                                         detail::big_unsigned& digits, work_budget& work) noexcept
    {
      using parts_result = result<decimal_parts>;

      decimal_parts parts{};
      std::size_t position{};

      if ((spelling.data[position] == '+') || (spelling.data[position] == '-'))
      {
        ++position;
      }

      // Hexadecimal is handled separately because 'e' and 'E' are digits there.
      if (kind == number_kind::hexadecimal)
      {
        position += c_hexadecimal_prefix_bytes;

        for (; position < spelling.size; ++position)
        {
          const auto appended =
            digits.append_digit(hexadecimal_digit_value(spelling.data[position]), c_hexadecimal_radix, work);

          if (!appended.has_value())
          {
            return parts_result::failure(*appended.error_if());
          }
        }

        return parts_result::success(parts);
      }

      bool after_point{};
      std::size_t fractional_digits{};

      while (position < spelling.size)
      {
        const char current = spelling.data[position];
        ++position;

        if ((current == 'e') || (current == 'E'))
        {
          break;
        }

        if (current == '.')
        {
          after_point = true;
          continue;
        }

        // Leading zeros keep the integer canonical zero and allocate nothing.
        const auto appended = digits.append_digit(static_cast<unsigned int>(current - '0'), c_decimal_radix, work);

        if (!appended.has_value())
        {
          return parts_result::failure(*appended.error_if());
        }

        ++parts.digit_count;

        if (after_point)
        {
          ++fractional_digits;
        }
      }

      const auto limit = checked_add(spelling.size, c_exponent_margin);

      if (!limit.has_value())
      {
        return parts_result::failure(*limit.error_if());
      }

      bool exponent_negative{};
      std::size_t exponent{};

      // Validated syntax guarantees at least one exponent digit after the introducer.
      if (position < spelling.size)
      {
        if ((spelling.data[position] == '+') || (spelling.data[position] == '-'))
        {
          exponent_negative = (spelling.data[position] == '-');
          ++position;
        }

        for (; position < spelling.size; ++position)
        {
          const auto digit = static_cast<std::size_t>(spelling.data[position] - '0');

          // Saturate before multiplying so arbitrarily long exponents cannot wrap.
          if (exponent < *limit.value_if())
          {
            exponent = (exponent > ((*limit.value_if() - digit) / c_decimal_radix))
                         ? *limit.value_if()
                         : (exponent * c_decimal_radix) + digit;
          }
        }
      }

      // Combine the written exponent with the implied fractional scale without signed overflow.
      if (exponent_negative)
      {
        const auto magnitude = checked_add(exponent, fractional_digits);

        if (!magnitude.has_value())
        {
          return parts_result::failure(*magnitude.error_if());
        }

        parts.scale_negative = true;
        parts.scale_magnitude = *magnitude.value_if();
      }
      else if (exponent >= fractional_digits)
      {
        parts.scale_magnitude = exponent - fractional_digits;
      }
      else
      {
        parts.scale_negative = true;
        parts.scale_magnitude = fractional_digits - exponent;
      }

      return parts_result::success(parts);
    }

    /**
     * @brief Apply the underflow policy to a nonzero value that rounds to zero.
     * @par Requirements
     * P038; J5-037; J5-038.
     * @tparam T Destination type: float or double.
     * @param negative Sign of the original value.
     * @param options Conversion policies.
     * @return Signed zero or an explicit underflow failure.
     */
    template <typename T>
    [[nodiscard]] result<T> finish_underflow(bool negative, const floating_conversion_options& options) noexcept
    {
      if (options.underflow != floating_underflow_policy::allow_zero)
      {
        return result<T>::failure(error::numeric_failure(numeric_conversion_reason::underflow));
      }

      constexpr T c_zero{};

      return result<T>::success(negative ? -c_zero : c_zero);
    }

    /**
     * @brief Convert an exact number to a correctly rounded float or double.
     * @par Requirements
     * P038; J5-003; J5-037; J5-038; J5-046; J5-048; J5-052.
     * @tparam T Destination type: float or double.
     * @param number Borrowed validated number.
     * @param scratch Caller-owned fallible temporary storage.
     * @param options Work, scratch and special-value policies.
     * @return Correctly rounded value or an explicit error.
     * @note Rounds once, directly to T, with ties to an even significand.
     * All scratch storage is released before returning.
     */
    template <typename T>
    [[nodiscard]] result<T> convert_to_floating(const owned_number& number, const storage_callbacks& scratch,
                                                const floating_conversion_options& options) noexcept
    {
      using float_result = result<T>;
      using limits = std::numeric_limits<T>;

      constexpr auto c_precision = static_cast<std::ptrdiff_t>(limits::digits);
      // Two extra quotient bits guarantee a rounding bit below the significand.
      constexpr std::ptrdiff_t c_quotient_bits = c_precision + 2;
      constexpr std::ptrdiff_t c_min_normal_exponent = limits::min_exponent - 1;
      constexpr auto c_overflow_scale = static_cast<std::size_t>(limits::max_exponent10) + 1;
      // 10^-c is below half the smallest subnormal, so such values always round to zero.
      constexpr auto c_underflow_scale = static_cast<std::size_t>(limits::digits10 - limits::min_exponent10) + 3;
      constexpr auto c_max_bits = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max() / 4);

      work_budget work{options.max_work_units};

      const auto analysis = detail::analyse_number(number, work);

      if (!analysis.has_value())
      {
        return float_result::failure(*analysis.error_if());
      }

      const auto special = detail::convert_special_floating<T>(*analysis.value_if(), options);

      if (!special.has_value())
      {
        return float_result::failure(*special.error_if());
      }

      if (special.value_if()->has_value())
      {
        return float_result::success(**special.value_if());
      }

      const bool negative = analysis.value_if()->negative;

      // Declared before integers so it outlives every allocation it accounts for.
      accounted_storage arena{scratch, options.max_scratch_bytes};
      const storage_callbacks provider = arena.callbacks();

      detail::big_unsigned numerator{provider};
      detail::big_unsigned denominator{provider};

      const auto read = read_significand(number.spelling(), number.kind(), numerator, work);

      if (!read.has_value())
      {
        return float_result::failure(*read.error_if());
      }

      const decimal_parts parts = *read.value_if();

      // Reject certain overflow before building a power of ten proportional to the exponent.
      if (!parts.scale_negative && (parts.scale_magnitude > c_overflow_scale))
      {
        return float_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      // The value is below 10^(digit_count - scale), so a large enough gap guarantees zero.
      if (parts.scale_negative && (parts.scale_magnitude >= parts.digit_count) &&
          ((parts.scale_magnitude - parts.digit_count) >= c_underflow_scale))
      {
        return finish_underflow<T>(negative, options);
      }

      const auto one = denominator.append_digit(1, c_decimal_radix, work);

      if (!one.has_value())
      {
        return float_result::failure(*one.error_if());
      }

      detail::big_unsigned& scaled = parts.scale_negative ? denominator : numerator;

      for (std::size_t index{}; index < parts.scale_magnitude; ++index)
      {
        const auto multiplied = scaled.append_digit(0, c_decimal_radix, work);

        if (!multiplied.has_value())
        {
          return float_result::failure(*multiplied.error_if());
        }
      }

      const auto numerator_length = numerator.bit_length();

      if (!numerator_length.has_value())
      {
        return float_result::failure(*numerator_length.error_if());
      }

      const auto denominator_length = denominator.bit_length();

      if (!denominator_length.has_value())
      {
        return float_result::failure(*denominator_length.error_if());
      }

      const std::size_t numerator_bits = *numerator_length.value_if();
      const std::size_t denominator_bits = *denominator_length.value_if();

      // Keeps the signed exponent arithmetic below far from overflow.
      if ((numerator_bits > c_max_bits) || (denominator_bits > c_max_bits))
      {
        return float_result::failure(error::without_location(error_code::resource_limit));
      }

      // After shifting, the quotient lies in [2^(P+1), 2^(P+3)): enough bits to round once.
      const std::ptrdiff_t shift =
        c_quotient_bits - (static_cast<std::ptrdiff_t>(numerator_bits) - static_cast<std::ptrdiff_t>(denominator_bits));

      if (shift > 0)
      {
        const auto widened = numerator.shift_left(static_cast<std::size_t>(shift), work);

        if (!widened.has_value())
        {
          return float_result::failure(*widened.error_if());
        }
      }
      else if (shift < 0)
      {
        const auto widened = denominator.shift_left(static_cast<std::size_t>(-shift), work);

        if (!widened.has_value())
        {
          return float_result::failure(*widened.error_if());
        }
      }

      // Aligning the divisor with the highest quotient bit replaces division with compare/subtract.
      const auto aligned = denominator.shift_left(static_cast<std::size_t>(c_quotient_bits), work);

      if (!aligned.has_value())
      {
        return float_result::failure(*aligned.error_if());
      }

      std::uint_least64_t quotient{};

      for (std::ptrdiff_t bit = c_quotient_bits; bit >= 0; --bit)
      {
        quotient <<= 1U;

        const auto order = numerator.compare(denominator, work);

        if (!order.has_value())
        {
          return float_result::failure(*order.error_if());
        }

        if (*order.value_if() != detail::magnitude_order::less)
        {
          const auto reduced = numerator.subtract(denominator, work);

          if (!reduced.has_value())
          {
            return float_result::failure(*reduced.error_if());
          }

          quotient |= 1U;
        }

        if (bit != 0)
        {
          const auto doubled = numerator.shift_left(1, work);

          if (!doubled.has_value())
          {
            return float_result::failure(*doubled.error_if());
          }
        }
      }

      // Any remainder means the exact value lies strictly above the truncated quotient.
      const bool sticky = !numerator.is_zero();

      const auto quotient_bits = static_cast<std::ptrdiff_t>(bit_length_of(quotient));
      const std::ptrdiff_t leading_exponent = (quotient_bits - 1) - shift;

      // Below the normal range, each lower exponent leaves one fewer significand bit.
      std::ptrdiff_t kept_bits = c_precision;

      if (leading_exponent < c_min_normal_exponent)
      {
        kept_bits -= (c_min_normal_exponent - leading_exponent);
      }

      std::ptrdiff_t dropped_bits = quotient_bits - kept_bits;

      // One bit beyond the quotient already rounds to zero; clamping keeps shifts below 64.
      if (dropped_bits > (quotient_bits + 1))
      {
        dropped_bits = quotient_bits + 1;
      }

      const auto drop = static_cast<unsigned int>(dropped_bits);
      const std::uint_least64_t half = std::uint_least64_t{1} << (drop - 1U);
      const std::uint_least64_t remainder = quotient & ((std::uint_least64_t{1} << drop) - 1U);
      std::uint_least64_t significand = quotient >> drop;

      // Round to nearest; an exact tie (no sticky bits) chooses the even significand.
      if ((remainder > half) || ((remainder == half) && (sticky || ((significand & 1U) != 0))))
      {
        ++significand;
      }

      if (significand == 0)
      {
        return finish_underflow<T>(negative, options);
      }

      const std::ptrdiff_t scale_exponent = dropped_bits - shift;

      // Rounding may carry into a new leading bit, so overflow is checked after rounding.
      const std::ptrdiff_t rounded_leading =
        (static_cast<std::ptrdiff_t>(bit_length_of(significand)) - 1) + scale_exponent;

      if (rounded_leading >= limits::max_exponent)
      {
        return float_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      // The significand fits in T's precision and the exponent is in range, so scaling is exact.
      const T magnitude = std::ldexp(static_cast<T>(significand), static_cast<int>(scale_exponent));

      return float_result::success(negative ? -magnitude : magnitude);
    }
  } // namespace

  /**
   * @brief Convert directly to float without an intermediate double.
   * @par Requirements
   * P038; J5-003; J5-037; J5-038; J5-046; J5-048; J5-052.
   */
  result<float> to_float(const owned_number& number, const storage_callbacks& scratch,
                         const floating_conversion_options& options) noexcept
  {
    return convert_to_floating<float>(number, scratch, options);
  }

  /**
   * @brief Convert directly to double.
   * @par Requirements
   * P038; J5-003; J5-037; J5-038; J5-046; J5-048; J5-052.
   */
  result<double> to_double(const owned_number& number, const storage_callbacks& scratch,
                           const floating_conversion_options& options) noexcept
  {
    return convert_to_floating<double>(number, scratch, options);
  }
} // namespace adaptive_json5
