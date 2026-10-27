#include "adaptive_json5/value/floating_conversion.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "common/checked_size.hpp"
#include "memory/work_budget.hpp"
#include "value/big_unsigned.hpp"
#include "value/floating_special.hpp"
#include "value/number_analysis.hpp"

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
    constexpr unsigned int c_decimal_chunk_radix = 10000;
    constexpr std::size_t c_decimal_chunk_digits = 4;

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
     * @brief Load validated significand digits into a reserved exact integer.
     * @par Requirements
     * P038; J5-003; J5-037; J5-046; J5-048.
     * @param spelling Validated finite numeric spelling.
     * @param kind Decimal or hexadecimal spelling family.
     * @param digits Reserved zero receiving the significand.
     * @param work Shared conversion work budget.
     * @return Success or an explicit arithmetic/resource failure.
     * @note Loads decimal chunks of up to four digits and single hexadecimal digits.
     * Ignores the decimal point and stops before the decimal exponent.
     * The caller reserves sufficient capacity before calling.
     */
    [[nodiscard]] status_result load_significand(input_view spelling, number_kind kind, detail::big_unsigned& digits,
                                                 work_budget& work) noexcept
    {
      std::size_t position{};

      if ((spelling.data[position] == '+') || (spelling.data[position] == '-'))
      {
        ++position;
      }

      // Hexadecimal 'e' and 'E' are digits, so keep this path separate.
      if (kind == number_kind::hexadecimal)
      {
        position += c_hexadecimal_prefix_bytes;

        for (; position < spelling.size; ++position)
        {
          const auto appended =
            digits.append_digit(hexadecimal_digit_value(spelling.data[position]), c_hexadecimal_radix, work);

          if (!appended.has_value())
          {
            return status_result::failure(*appended.error_if());
          }
        }

        return status_result::success();
      }

      unsigned int chunk{};
      unsigned int chunk_radix{1};

      while (position < spelling.size)
      {
        const char current = spelling.data[position];
        ++position;

        if ((current == 'e') || (current == 'E'))
        {
          break;
        }

        // Analysis already accounts for the point in the decimal scale.
        if (current == '.')
        {
          continue;
        }

        const auto digit = static_cast<unsigned int>(current - '0');

        // At most four decimal digits are accumulated, including leading zeros.
        chunk = (chunk * c_decimal_radix) + digit;
        chunk_radix *= c_decimal_radix;

        if (chunk_radix == c_decimal_chunk_radix)
        {
          const auto appended = digits.append_digit(chunk, chunk_radix, work);

          if (!appended.has_value())
          {
            return status_result::failure(*appended.error_if());
          }

          chunk = 0;
          chunk_radix = 1;
        }
      }

      // A shorter final chunk must be loaded even when its numeric value is zero.
      if (chunk_radix != 1)
      {
        return digits.append_digit(chunk, chunk_radix, work);
      }

      return status_result::success();
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

      // Analysis already collected these properties during its existing scan.
      const detail::number_analysis& parts = *analysis.value_if();
      const bool negative = parts.negative;

      // Reject certain overflow before reserving scratch or loading digits.
      if (!parts.scale_negative && (parts.scale_magnitude > c_overflow_scale))
      {
        return float_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      // The value is below 10^(digit_count - scale), so this gap guarantees zero.
      if (parts.scale_negative && (parts.scale_magnitude >= parts.digit_count) &&
          ((parts.scale_magnitude - parts.digit_count) >= c_underflow_scale))
      {
        return finish_underflow<T>(negative, options);
      }

      // Declared before integers so it outlives every allocation it accounts for.
      accounted_storage arena{scratch, options.max_scratch_bytes};
      const storage_callbacks provider = arena.callbacks();

      detail::big_unsigned numerator{provider};
      detail::big_unsigned denominator{provider};

      // Bound the significand without scanning its spelling again.
      const auto digit_bits = (parts.kind == number_kind::hexadecimal)
                                ? checked_multiply(parts.digit_count, std::size_t{4})
                                : detail::decimal_digit_bits(parts.digit_count);

      if (!digit_bits.has_value())
      {
        return float_result::failure(*digit_bits.error_if());
      }

      const auto scale_bits = detail::decimal_digit_bits(parts.scale_magnitude);

      if (!scale_bits.has_value())
      {
        return float_result::failure(*scale_bits.error_if());
      }

      // Before scaling, the numerator holds the significand and the denominator holds one.
      std::size_t numerator_bound = *digit_bits.value_if();
      std::size_t denominator_bound = 1;

      if (parts.scale_negative)
      {
        // The denominator becomes 10^scale; the extra bit also covers 10^0.
        const auto scaled_bound = checked_add(denominator_bound, *scale_bits.value_if());

        if (!scaled_bound.has_value())
        {
          return float_result::failure(*scaled_bound.error_if());
        }

        denominator_bound = *scaled_bound.value_if();
      }
      else
      {
        const auto scaled_bound = checked_add(numerator_bound, *scale_bits.value_if());

        if (!scaled_bound.has_value())
        {
          return float_result::failure(*scaled_bound.error_if());
        }

        numerator_bound = *scaled_bound.value_if();
      }

      // Both integers must fit the later alignment and remainder operations.
      const std::size_t larger_bound = (numerator_bound > denominator_bound) ? numerator_bound : denominator_bound;

      const auto aligned_bound = checked_add(larger_bound, static_cast<std::size_t>(c_quotient_bits));

      if (!aligned_bound.has_value())
      {
        return float_result::failure(*aligned_bound.error_if());
      }

      // A remainder can temporarily double before the next subtraction.
      const auto capacity_bits = checked_add(*aligned_bound.value_if(), std::size_t{1});

      if (!capacity_bits.has_value())
      {
        return float_result::failure(*capacity_bits.error_if());
      }

      const auto capacity_limbs = detail::limbs_for_bits(*capacity_bits.value_if());

      if (!capacity_limbs.has_value())
      {
        return float_result::failure(*capacity_limbs.error_if());
      }

      // The bound calculation has constant cost; reservation does not initialize limbs.
      const auto bound_charged = work.charge(1);

      if (!bound_charged.has_value())
      {
        return float_result::failure(*bound_charged.error_if());
      }

      const auto numerator_reserved = numerator.reserve(*capacity_limbs.value_if());

      if (!numerator_reserved.has_value())
      {
        return float_result::failure(*numerator_reserved.error_if());
      }

      const auto denominator_reserved = denominator.reserve(*capacity_limbs.value_if());

      if (!denominator_reserved.has_value())
      {
        return float_result::failure(*denominator_reserved.error_if());
      }

      // Start digit arithmetic only after both scratch reservations succeed.
      const auto loaded = load_significand(number.spelling(), parts.kind, numerator, work);

      if (!loaded.has_value())
      {
        return float_result::failure(*loaded.error_if());
      }

      const auto one = denominator.append_digit(1, c_decimal_radix, work);

      if (!one.has_value())
      {
        return float_result::failure(*one.error_if());
      }

      detail::big_unsigned& scaled = parts.scale_negative ? denominator : numerator;

      std::size_t remaining_scale = parts.scale_magnitude;

      // Each full chunk applies four decimal positions in one limb operation.
      while (remaining_scale >= c_decimal_chunk_digits)
      {
        const auto multiplied = scaled.append_digit(0, c_decimal_chunk_radix, work);

        if (!multiplied.has_value())
        {
          return float_result::failure(*multiplied.error_if());
        }

        remaining_scale -= c_decimal_chunk_digits;
      }

      if (remaining_scale != 0)
      {
        unsigned int radix = 1;

        // At most three positions remain, so radix never exceeds 1000.
        for (std::size_t index{}; index < remaining_scale; ++index)
        {
          radix *= c_decimal_radix;
        }

        const auto multiplied = scaled.append_digit(0, radix, work);

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
