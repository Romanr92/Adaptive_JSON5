#include "adaptive_json5/value/floating_conversion.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "common/checked_size.hpp"
#include "memory/work_budget.hpp"
#include "value/big_unsigned.hpp"
#include "value/floating_format.hpp"
#include "value/floating_special.hpp"
#include "value/number_analysis.hpp"
#include "value/power_of_five_table.hpp"
#include "value/wide_multiply.hpp"

#include <array>
#include <cfenv>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace adaptive_json5
{
  namespace
  {
    constexpr std::uint32_t c_decimal_radix = 10;
    constexpr std::uint32_t c_hexadecimal_radix = 16;
    constexpr std::size_t c_hexadecimal_prefix_bytes = 2;
    constexpr std::size_t c_sign_byte_count = 1;
    constexpr std::uint32_t c_decimal_chunk_radix = 10000;
    constexpr std::size_t c_decimal_chunk_digits = 4;
    constexpr std::array<std::uint32_t, 6> c_hexadecimal_letter_values{10U, 11U, 12U, 13U, 14U, 15U};

#if defined(FLT_EVAL_METHOD) && (FLT_EVAL_METHOD == 0)
    // 10^k = 2^k * 5^k is exact while 5^k fits the significand: k <= 22 for double, k <= 10 for float.
    constexpr std::array<double, 23> c_double_powers_of_ten{1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                                            1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                                            1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    constexpr std::array<float, 11> c_float_powers_of_ten{1e0F, 1e1F, 1e2F, 1e3F, 1e4F, 1e5F,
                                                          1e6F, 1e7F, 1e8F, 1e9F, 1e10F};

    /**
     * @brief Look up a power of ten that T represents exactly.
     * @par Requirements
     * J5-037.
     * @tparam T Destination type: float or double.
     * @param exponent Decimal exponent magnitude.
     * @return 10^exponent, or an empty optional when it is not exact in T.
     * @par ID SWU-VALUE-177
     */
    template <typename T>
    [[nodiscard]] std::optional<T> exact_power_of_ten(std::size_t exponent) noexcept
    {
      if constexpr (std::numeric_limits<T>::digits == 53)
      {
        if (exponent < c_double_powers_of_ten.size())
        {
          return c_double_powers_of_ten[exponent];
        }
      }
      else
      {
        if (exponent < c_float_powers_of_ten.size())
        {
          return c_float_powers_of_ten[exponent];
        }
      }

      return std::nullopt;
    }

    /**
     * @brief Report whether the caller's floating-point environment rounds to nearest.
     * @par Requirements
     * J5-037; J5-052.
     * @return False also when the target cannot report its mode; the integer-only steps then decide.
     * @par ID SWU-VALUE-178
     */
    [[nodiscard]] bool rounds_to_nearest() noexcept
    {
  #if defined(FE_TONEAREST)
      return std::fegetround() == FE_TONEAREST;
  #else
      return false;
  #endif
    }
#endif // FLT_EVAL_METHOD == 0

    /**
     * @brief Convert one validated hexadecimal digit character to its value.
     * @par Requirements
     * P038; J5-037.
     * @param digit Validated hexadecimal digit.
     * @return Digit value from 0 to 15.
     * @note Uses explicit ranges rather than locale-dependent classification.
     * @par ID SWU-VALUE-049
     */
    [[nodiscard]] std::uint32_t hexadecimal_digit_value(char digit) noexcept
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Classifies the byte with a range check on ASCII digits or letters; the result is
      // only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so
      // it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.06)
      if ((digit >= '0') && (digit <= '9'))
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares
        // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character
        // set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.05)
        return static_cast<std::uint32_t>(digit - '0');
      }

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Classifies the byte with a range check on ASCII digits or letters; the result is
      // only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so
      // it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.04)
      if ((digit >= 'a') && (digit <= 'f'))
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares
        // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character
        // set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.03)
        return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'a')];
      }

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a value
      // 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8
      // input bytes with character literals, so it relies on an ASCII-compatible execution character set, where
      // these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.02)
      return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'A')];
    }

    /**
     * @brief Count significant bits of a small quotient.
     * @par Requirements
     * P038; J5-037.
     * @param value Quotient or rounded significand.
     * @return Zero for zero; otherwise the highest set bit position plus one.
     * @par ID SWU-VALUE-050
     */
    [[nodiscard]] std::size_t bit_length_of(std::uint_least64_t value) noexcept
    {
      std::size_t bits{};

      while (value != 0)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Each iteration shifts a 64-bit value right by one bit, so
        // the loop runs at most 64 times and bits never exceeds 64.
        // codeql::misra_deviation_next_line(DEV-004.17)
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
     * @par ID SWU-VALUE-051
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

        while (position < spelling.size)
        {
          const auto appended =
            digits.append_digit(hexadecimal_digit_value(spelling.data[position]), c_hexadecimal_radix, work);

          if (!appended.has_value())
          {
            return status_result::failure(*appended.error_if());
          }

          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: The loop condition position < spelling.size holds here, so
          // position + 1 is at most spelling.size and cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.16)
          ++position;
        }

        return status_result::success();
      }

      std::uint32_t chunk{};
      std::uint32_t chunk_radix{1};

      while (position < spelling.size)
      {
        const char current = spelling.data[position];
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: The loop condition position < spelling.size holds here, so
        // position + 1 is at most spelling.size and cannot wrap std::size_t.
        // codeql::misra_deviation_next_line(DEV-004.15)
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

        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares
        // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character
        // set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.01)
        const auto digit = static_cast<std::uint32_t>(current - '0');

        // At most four decimal digits are accumulated, including leading zeros.
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: At most three digits are held before this step, so chunk is
        // at most 999, and number analysis guarantees digit is 0 to 9. The result is at
        // most 9999 and cannot wrap std::uint32_t.
        // codeql::misra_deviation_next_line(DEV-004.14)
        chunk = (chunk * c_decimal_radix) + digit;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: chunk_radix is reset to 1 when it reaches
        // c_decimal_chunk_radix (10000), so it is at most 1000 before this multiply.
        // codeql::misra_deviation_next_line(DEV-004.13)
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
     * @par ID SWU-VALUE-052
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

#if ADAPTIVE_JSON5_EISEL_LEMIRE

    /**
     * @brief Round a scaled product to a normal floating-point value or defer to exact conversion.
     * @par Requirements
     * J5-037; J5-039.
     * @pre The exponent is inside the power-of-five table range and the product has passed the ambiguity check.
     * @par ID SWU-VALUE-200
     */
    template <typename T>
    [[nodiscard]] std::optional<T> round_eisel_product(const detail::product_128& product, std::int32_t power,
                                                       std::int32_t leading_zeros, bool negative) noexcept
    {
      using limits = std::numeric_limits<T>;

      constexpr std::int32_t c_explicit_bits = limits::digits - 1;             // 52 or 23
      constexpr std::int32_t c_minimum_exponent = -(limits::max_exponent - 1); // -1023 or -127
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: Compile-time constant: limits::max_exponent is 1024 or 128, giving 2047 or 255. An
      // overflowing constant expression would not compile.
      // codeql::misra_deviation_next_line(DEV-005.15)
      constexpr std::int32_t c_infinite_power = (2 * limits::max_exponent) - 1; // 2047 or 255
      // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
      // MISRA justification: Compile-time property of the template parameter or target type. The condition is
      // constant for one instantiation but differs between the supported types, so it is needed.
      // codeql::misra_deviation_next_line(DEV-006.12)
      constexpr std::int32_t c_round_even_min = (limits::digits == 53) ? -4 : -17;
      // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
      // MISRA justification: Compile-time property of the template parameter or target type. The condition is
      // constant for one instantiation but differs between the supported types, so it is needed.
      // codeql::misra_deviation_next_line(DEV-006.11)
      constexpr std::int32_t c_round_even_max = (limits::digits == 53) ? 23 : 10;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Compile-time constant: c_explicit_bits is 52 for double and
      // 23 for float, giving 9 or 38. The static_assert below rejects any type for which
      // the subtraction would wrap.
      // codeql::misra_deviation_next_line(DEV-004.12)
      constexpr std::uint32_t c_shift = 64U - static_cast<std::uint32_t>(c_explicit_bits) - 3U;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Compile-time constant: c_shift is 9 for double and 38 for
      // float, so adding one gives 10 or 39. The static_assert below rejects any type for
      // which the addition would wrap or the shift would leave 64 bits.
      // codeql::misra_deviation_next_line(DEV-004.18)
      constexpr std::uint32_t c_shift_upper = c_shift + 1U;
      constexpr auto c_explicit_shift = static_cast<std::uint32_t>(c_explicit_bits);
      // floor(power * log2(10)) is (power * 217706) / 2^16 for every table exponent.
      constexpr std::int64_t c_log2_ten_scaled = 217706;
      constexpr std::int64_t c_log2_ten_scale = 65536;
      // The product's top bit selects one extra shift; a choice, not an addition, keeps it in range.
      const bool upper_bit = (product.high >> 63U) != 0U;
      const std::uint32_t mantissa_shift = upper_bit ? c_shift_upper : c_shift;
      std::uint64_t mantissa = product.high >> mantissa_shift;

      // Floor division written out: right-shifting a negative value is implementation-defined in C++17.
      const std::int64_t scaled = c_log2_ten_scaled * power;
      const std::int64_t floor_log2 =
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: the caller bounds power to the power-of-five table range, so |scaled| =
        // |217706 * power| is below 10^8; negating it, adding the scale and dividing by 65536 cannot overflow
        // std::int64_t.
        // codeql::misra_deviation_next_line(DEV-005.13)
        (scaled >= 0) ? (scaled / c_log2_ten_scale) : -((-scaled + c_log2_ten_scale - 1) / c_log2_ten_scale);
      std::int32_t power2 =
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: power lies in the table range, so |floor_log2| is below 1200; leading_zeros is 0
        // to 63 and c_minimum_exponent is -1023 or -127, so the sum stays within a few thousand.
        // codeql::misra_deviation_next_line(DEV-005.12)
        static_cast<std::int32_t>(floor_log2) + 63 + (upper_bit ? 1 : 0) - leading_zeros - c_minimum_exponent;

      if (power2 <= 0)
      {
        return std::nullopt; // Subnormal or zero: the exact path applies the underflow policy.
      }

      // An exact halfway product rounds to even instead of up.
      if ((product.low <= 1U) && (power >= c_round_even_min) && (power <= c_round_even_max) &&
          ((mantissa & 3U) == 1U) && ((mantissa << mantissa_shift) == product.high))

      {
        mantissa &= ~static_cast<std::uint64_t>(1U);
      }

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: mantissa is product.high shifted right by at least
      // c_shift (9 for double, 38 for float) bits, so it is below 2^55. Adding its
      // lowest bit cannot wrap std::uint64_t.
      // codeql::misra_deviation_next_line(DEV-004.08)
      mantissa += mantissa & 1U;
      mantissa >>= 1U;

      // Rounding carried into a new leading bit.
      if (mantissa >= (static_cast<std::uint64_t>(2U) << c_explicit_shift))
      {
        mantissa = static_cast<std::uint64_t>(1U) << c_explicit_shift;
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: power2 is below c_infinite_power plus a few units here (it comes from the
        // bounded table exponent), so the increment cannot overflow std::int32_t.
        // codeql::misra_deviation_next_line(DEV-005.11)
        ++power2;
      }

      if (power2 >= c_infinite_power)
      {
        return std::nullopt; // Overflow: the exact path reports out_of_range.
      }

      // mantissa now includes the implicit leading bit and is below 2^digits, so ldexp is exact.
      // Plain int on purpose: the standard declares std::ldexp(T, int). The exponent stays within a few
      // thousand, inside the 16-bit minimum range of int.
      const T magnitude =
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: power2 is positive and below c_infinite_power after the range checks, so adding
        // c_minimum_exponent and subtracting c_explicit_bits stays within a few thousand.
        // codeql::misra_deviation_next_line(DEV-005.10)
        std::ldexp(static_cast<T>(mantissa), static_cast<int>(power2 + c_minimum_exponent - c_explicit_bits));

      return negative ? -magnitude : magnitude;
    }

    /**
     * @brief Convert w * 10^power to T without scratch using the Eisel-Lemire algorithm.
     * @par Requirements
     * J5-037; J5-039.
     * @param significand Nonzero decimal significand of at most 19 digits.
     * @param power Decimal exponent.
     * @param negative Sign of the result.
     * @return The correctly rounded normal value, or an empty optional when the exact path
     * must decide: exponent outside the table, subnormal or zero result, overflow, or an
     * undecidable approximation.
     * @note Lemire (2021). Integer arithmetic only, so it needs no FLT_EVAL_METHOD guard.
     * @par ID SWU-VALUE-152
     */
    template <typename T>
    [[nodiscard]] std::optional<T> try_eisel_lemire(std::uint64_t significand, std::int32_t power,
                                                    bool negative) noexcept
    {
      using limits = std::numeric_limits<T>;

      constexpr std::int32_t c_explicit_bits = limits::digits - 1; // 52 or 23
      constexpr std::int32_t c_safe_power_min = -27;
      constexpr std::int32_t c_safe_power_max = 55;
      constexpr std::uint64_t c_all_ones = std::numeric_limits<std::uint64_t>::max();
      constexpr auto c_explicit_shift = static_cast<std::uint32_t>(c_explicit_bits);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Compile-time constant: c_explicit_shift is 52 or 23, so the shift is 55 or 26.
      // codeql::misra_deviation_next_line(DEV-004.19)
      constexpr std::uint64_t c_precision_mask = c_all_ones >> (c_explicit_shift + 3U);

      if ((significand == 0U) || (power < detail::c_smallest_power_of_five) ||
          (power > detail::c_largest_power_of_five))
      {
        return std::nullopt;
      }

      // Normalize so the significand's top bit is set.
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: significand is nonzero (checked above), so bit_length_of()
      // returns 1 to 64 and the subtraction yields 0 to 63 without wrapping.
      // codeql::misra_deviation_next_line(DEV-004.11)
      const auto leading_zeros = static_cast<std::int32_t>(64U - bit_length_of(significand));
      const std::uint64_t normalized = significand << static_cast<std::uint32_t>(leading_zeros);
      const detail::power_of_five_128& entry =
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: power is between c_smallest_power_of_five and c_largest_power_of_five (checked
        // above), so the difference is a table index from 0 to the table size minus one.
        // codeql::misra_deviation_next_line(DEV-005.14)
        detail::c_power_of_five_table[static_cast<std::size_t>(power - detail::c_smallest_power_of_five)];

      detail::product_128 product = detail::multiply_64x64_to_128(normalized, entry.high);

      // Only when the bits below the rounding position are all ones can the low table word matter.
      if ((product.high & c_precision_mask) == c_precision_mask)
      {
        const detail::product_128 second = detail::multiply_64x64_to_128(normalized, entry.low);

        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: This is the low word of a 128-bit addition and is meant to
        // wrap modulo 2^64. The next statement detects the wrap (second.high > product.low)
        // and carries it into product.high, so no value is lost.
        // codeql::misra_deviation_next_line(DEV-004.10)
        product.low += second.high;
        if (second.high > product.low)
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: product.high is the high word of a 64x64-bit product, so
          // it is at most 2^64 - 2. Adding the carry cannot wrap std::uint64_t.
          // codeql::misra_deviation_next_line(DEV-004.09)
          ++product.high;
        }

        if ((product.low == c_all_ones) && ((power < c_safe_power_min) || (power > c_safe_power_max)))
        {
          return std::nullopt;
        }
      }

      return round_eisel_product<T>(product, power, leading_zeros, negative);
    }
#endif // ADAPTIVE_JSON5_EISEL_LEMIRE

    /**
     * @brief Accumulate a validated significand whose digit count fits the fast-path integer.
     * @par Requirements
     * J5-037; J5-039.
     * @par ID SWU-VALUE-201
     */
    [[nodiscard]] std::uint64_t read_fast_significand(input_view spelling, bool hexadecimal) noexcept
    {
      // Accumulate significand digits; skip sign, 0x prefix and '.', stop at the exponent.
      std::uint64_t significand{};

      // Skip an optional sign; analysis guarantees at least one character.
      std::size_t position{};
      if ((spelling.data[0] == '+') || (spelling.data[0] == '-'))
      {
        position = c_sign_byte_count;
      }

      // Hexadecimal 'e' and 'E' are digits, so keep this path separate.
      if (hexadecimal)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: number analysis has already accepted the optional sign and
        // the 0x prefix, so position + c_hexadecimal_prefix_bytes is at most spelling.size.
        // The increment runs only while position < spelling.size. Neither can wrap std::size_t.
        // codeql::misra_deviation_next_line(DEV-004.07)
        for (position += c_hexadecimal_prefix_bytes; position < spelling.size; ++position)
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: try_fast_path() returns before this loop when
          // parts.digit_count exceeds 16, and number analysis only accepts hexadecimal
          // digits here. The accumulated value is at most 16^16 - 1 = 2^64 - 1, so
          // neither the multiply nor the add can wrap.
          // codeql::misra_deviation_next_line(DEV-004.06)
          significand = (significand * c_hexadecimal_radix) + hexadecimal_digit_value(spelling.data[position]);
        }
      }
      else
      {
        while (position < spelling.size)
        {
          const char current = spelling.data[position];

          if ((current == 'e') || (current == 'E'))
          {
            break;
          }

          // Analysis already accounts for the point in the decimal scale.
          if (current != '.')
          {
            // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: try_fast_path() returns before this loop when
            // parts.digit_count exceeds 19, and number analysis only accepts '0'-'9' here
            // besides '.' and the exponent marker. The accumulated value is at most
            // 10^19 - 1, which is below 2^64 - 1, so neither the multiply nor the add can wrap.
            // Converts an already validated digit or hexadecimal letter to its value, or a value 0-9 back to
            // its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8 input
            // bytes with character literals, so it relies on an ASCII-compatible execution character set, where
            // these characters are contiguous ranges.
            // codeql::misra_deviation_next_line(DEV-004.04)
            significand = (significand * c_decimal_radix) + static_cast<std::uint32_t>(current - '0');
          }

          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: The loop condition position < spelling.size holds here, so
          // position + 1 is at most spelling.size and cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.05)
          ++position;
        }
      }
      return significand;
    }
#if defined(FLT_EVAL_METHOD) && (FLT_EVAL_METHOD == 0)

    /**
     * @brief Convert an exactly representable significand and power of ten under the supported rounding mode.
     * @par Requirements
     * J5-037; J5-039.
     * @par ID SWU-VALUE-202
     */
    template <typename T>
    [[nodiscard]] std::optional<T> try_clinger(std::uint64_t significand, const detail::number_analysis& parts) noexcept
    {
      {
        using limits = std::numeric_limits<T>;

        // 2^digits is the largest integer range T holds exactly.
        constexpr std::uint64_t c_max_exact_significand = static_cast<std::uint64_t>(1U)
                                                          << static_cast<std::uint32_t>(limits::digits);

        if (significand <= c_max_exact_significand)
        {
          // Exact by the bound above, so the result is the same in every rounding mode.
          const T exact = static_cast<T>(significand);

          if (parts.scale_magnitude == 0)
          {
            return parts.negative ? -exact : exact;
          }

          const std::optional<T> power = exact_power_of_ten<T>(parts.scale_magnitude);

          // One multiply or divide rounds in the caller's mode: use it only when that mode is to nearest.
          if (power.has_value() && rounds_to_nearest())
          {
            const T value = parts.scale_negative ? (exact / *power) : (exact * *power);
            return parts.negative ? -value : value;
          }
        }
      }
      return std::nullopt;
    }
#endif
    /**
     * @brief Convert without scratch: Clinger for exact cases, then Eisel-Lemire.
     * @par Requirements
     * J5-037; J5-039.
     * @return The correctly rounded value, an empty optional when the exact path is needed,
     * or resource_limit (work) when work is exhausted.
     * @note Clinger (1990): one exact multiply or divide is correctly rounded under IEEE
     * round-to-nearest, so it runs only when the caller's rounding mode is to nearest;
     * disabled when intermediate precision is wider than T.
     * Eisel-Lemire (Lemire 2021) uses integers only and runs when ADAPTIVE_JSON5_EISEL_LEMIRE is ON.
     * @par ID SWU-VALUE-153
     */
    template <typename T>
    [[nodiscard]] result<std::optional<T>> try_fast_path(input_view spelling, const detail::number_analysis& parts,
                                                         work_budget& work) noexcept
    {
      using fast_result = result<std::optional<T>>;

      // A uint64_t holds 19 decimal or 16 hexadecimal digits; leading zeros only make this conservative.
      const bool hexadecimal = (parts.kind == number_kind::hexadecimal);
      if (parts.digit_count > (hexadecimal ? 16U : 19U))
      {
        return fast_result::success(std::nullopt);
      }

      const auto charged = work.charge(parts.digit_count);
      if (!charged.has_value())
      {
        return fast_result::failure(*charged.error_if());
      }

      const auto significand = read_fast_significand(spelling, hexadecimal);

#if defined(FLT_EVAL_METHOD) && (FLT_EVAL_METHOD == 0)
      const auto exact = try_clinger<T>(significand, parts);
      if (exact.has_value())
      {
        return fast_result::success(exact);
      }
#endif // FLT_EVAL_METHOD == 0: x87-style excess precision would round Clinger's operation twice.

#if ADAPTIVE_JSON5_EISEL_LEMIRE
      // Integer-only, so it also runs where Clinger is disabled. Hexadecimal has no decimal exponent.
      constexpr auto c_largest_table_scale = static_cast<std::size_t>(-detail::c_smallest_power_of_five);
      if (!hexadecimal && (parts.scale_magnitude <= c_largest_table_scale))
      {
        const auto scale = static_cast<std::int32_t>(parts.scale_magnitude);
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: The branch condition limits scale_magnitude to c_largest_table_scale, so scale
        // is small and positive and negating it cannot overflow std::int32_t.
        // codeql::misra_deviation_next_line(DEV-005.09)
        const auto converted = try_eisel_lemire<T>(significand, parts.scale_negative ? -scale : scale, parts.negative);

        if (converted.has_value())
        {
          return fast_result::success(*converted);
        }
      }
#endif // ADAPTIVE_JSON5_EISEL_LEMIRE

      return fast_result::success(std::nullopt);
    }

    /**
     * @brief Settle values whose decimal scale alone proves overflow or a zero result.
     * @par Requirements
     * P038; J5-037; J5-048; J5-049.
     * @tparam T Destination type: float or double.
     * @param parts Analysis of the validated number.
     * @param options Conversion policies.
     * @return The overflow failure or the underflow result, or no value when conversion must continue.
     * @note Runs after spelling analysis and before scratch reservation.
     * @par ID SWU-VALUE-186
     */
    template <typename T>
    [[nodiscard]] result<std::optional<T>> reject_by_scale(const detail::number_analysis& parts,
                                                           const floating_conversion_options& options) noexcept
    {
      using scale_result = result<std::optional<T>>;
      using limits = std::numeric_limits<T>;

      constexpr auto c_overflow_scale = static_cast<std::size_t>(limits::max_exponent10 + 1);
      // 10^-c is below half the smallest subnormal, so such values always round to zero.
      constexpr auto c_underflow_scale = static_cast<std::size_t>(limits::digits10 - limits::min_exponent10 + 3);

      // Reject certain overflow before reserving scratch or loading digits.
      if (!parts.scale_negative && (parts.scale_magnitude > c_overflow_scale))
      {
        return scale_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      // The value is below 10^(digit_count - scale), so this gap guarantees zero.
      if (parts.scale_negative)
      {
        const auto threshold =
          checked_add(parts.digit_count, c_underflow_scale, resource_limit_reason::conversion_size);
        if (threshold.has_value() && (parts.scale_magnitude >= *threshold.value_if()))
        {
          const auto zero = finish_underflow<T>(parts.negative, options);

          return zero.has_value() ? scale_result::success(std::optional<T>{*zero.value_if()})
                                  : scale_result::failure(*zero.error_if());
        }
      }

      return scale_result::success(std::nullopt);
    }

    /**
     * @brief Compute a checked limb bound for both aligned operands and the temporary remainder.
     * @par Requirements
     * P038; J5-037; J5-048; J5-049.
     * @par ID SWU-VALUE-203
     */
    [[nodiscard]] result<std::size_t> operand_capacity(const detail::number_analysis& parts,
                                                       std::size_t quotient_bits) noexcept
    {
      // Bound the significand without scanning its spelling again.
      const auto digit_bits =
        (parts.kind == number_kind::hexadecimal)
          ? checked_multiply(parts.digit_count, static_cast<std::size_t>(4U), resource_limit_reason::conversion_size)
          : detail::decimal_digit_bits(parts.digit_count);

      if (!digit_bits.has_value())
      {
        return result<std::size_t>::failure(*digit_bits.error_if());
      }

      const auto scale_bits = detail::decimal_digit_bits(parts.scale_magnitude);

      if (!scale_bits.has_value())
      {
        return result<std::size_t>::failure(*scale_bits.error_if());
      }

      // Before scaling, the numerator holds the significand and the denominator holds one.
      std::size_t numerator_bound = *digit_bits.value_if();
      std::size_t denominator_bound = 1;

      if (parts.scale_negative)
      {
        // The denominator becomes 10^scale; the extra bit also covers 10^0.
        const auto scaled_bound =
          checked_add(denominator_bound, *scale_bits.value_if(), resource_limit_reason::conversion_size);

        if (!scaled_bound.has_value())
        {
          return result<std::size_t>::failure(*scaled_bound.error_if());
        }

        denominator_bound = *scaled_bound.value_if();
      }
      else
      {
        const auto scaled_bound =
          checked_add(numerator_bound, *scale_bits.value_if(), resource_limit_reason::conversion_size);

        if (!scaled_bound.has_value())
        {
          return result<std::size_t>::failure(*scaled_bound.error_if());
        }

        numerator_bound = *scaled_bound.value_if();
      }

      // Both integers must fit the later alignment and remainder operations.
      const std::size_t larger_bound = (numerator_bound > denominator_bound) ? numerator_bound : denominator_bound;

      const auto aligned_bound =
        // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
        // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
        // every path assigns the real value before use. For constants and results, the value is read later in
        // the same function or template instantiation, which the tool does not track.
        // codeql::misra_deviation_next_line(DEV-007.12)
        checked_add(larger_bound, quotient_bits, resource_limit_reason::conversion_size);

      if (!aligned_bound.has_value())
      {
        return result<std::size_t>::failure(*aligned_bound.error_if());
      }

      // A remainder can temporarily double before the next subtraction.
      const auto capacity_bits =
        // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
        // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
        // every path assigns the real value before use. For constants and results, the value is read later in
        // the same function or template instantiation, which the tool does not track.
        // codeql::misra_deviation_next_line(DEV-007.11)
        checked_add(*aligned_bound.value_if(), static_cast<std::size_t>(1U), resource_limit_reason::conversion_size);

      if (!capacity_bits.has_value())
      {
        return result<std::size_t>::failure(*capacity_bits.error_if());
      }

      const auto capacity_limbs = detail::limbs_for_bits(*capacity_bits.value_if());
      return capacity_limbs;
    }

    /**
     * @brief Reserve scratch for the exact numerator and denominator before any digit arithmetic.
     * @par Requirements
     * P038; J5-037; J5-048; J5-049.
     * @param parts Analysis of the validated number.
     * @param quotient_bits Index of the highest quotient bit (precision plus two).
     * @param numerator Empty integer that receives the significand.
     * @param denominator Empty integer that receives the power of ten.
     * @param work Work budget; charged one unit for the bound calculation.
     * @return Success, or the size, work or scratch failure.
     * @note Reserves room for alignment and the remainder doubling, so later steps do not grow.
     * @par ID SWU-VALUE-187
     */
    [[nodiscard]] status_result reserve_operands(const detail::number_analysis& parts, std::size_t quotient_bits,
                                                 detail::big_unsigned& numerator, detail::big_unsigned& denominator,
                                                 work_budget& work) noexcept
    {
      const auto capacity_limbs = operand_capacity(parts, quotient_bits);

      if (!capacity_limbs.has_value())
      {
        return status_result::failure(*capacity_limbs.error_if());
      }

      // The bound calculation has constant cost; reservation does not initialize limbs.
      const auto bound_charged = work.charge(1);

      if (!bound_charged.has_value())
      {
        return status_result::failure(*bound_charged.error_if());
      }

      const auto numerator_reserved = numerator.reserve(*capacity_limbs.value_if());

      if (!numerator_reserved.has_value())
      {
        return status_result::failure(*numerator_reserved.error_if());
      }

      const auto denominator_reserved = denominator.reserve(*capacity_limbs.value_if());

      if (!denominator_reserved.has_value())
      {
        return status_result::failure(*denominator_reserved.error_if());
      }

      return status_result::success();
    }

    /**
     * @brief Load the significand and scale numerator or denominator by the decimal exponent.
     * @par Requirements
     * P038; J5-037; J5-048; J5-049.
     * @param spelling Number spelling.
     * @param parts Analysis of the spelling.
     * @param numerator Reserved integer for the significand.
     * @param denominator Reserved integer, set to one and then scaled for a negative exponent.
     * @param work Work budget.
     * @return Success, or the work or size failure.
     * @note Afterwards the exact value is numerator / denominator.
     * @par ID SWU-VALUE-188
     */
    [[nodiscard]] status_result load_ratio(input_view spelling, const detail::number_analysis& parts,
                                           detail::big_unsigned& numerator, detail::big_unsigned& denominator,
                                           work_budget& work) noexcept
    {
      // Start digit arithmetic only after both scratch reservations succeed.
      const auto loaded = load_significand(spelling, parts.kind, numerator, work);

      if (!loaded.has_value())
      {
        return status_result::failure(*loaded.error_if());
      }

      const auto one = denominator.append_digit(1, c_decimal_radix, work);

      if (!one.has_value())
      {
        return status_result::failure(*one.error_if());
      }

      detail::big_unsigned& scaled = parts.scale_negative ? denominator : numerator;

      const std::size_t full_chunks = parts.scale_magnitude / c_decimal_chunk_digits;
      const std::size_t remaining_scale = parts.scale_magnitude % c_decimal_chunk_digits;

      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
      // bound that the body does not modify, and changed only in the increment expression. The counter is
      // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
      // codeql::misra_deviation_next_line(DEV-017.02)
      for (std::size_t chunk{}; chunk < full_chunks; ++chunk)
      {
        const auto multiplied = scaled.append_digit(0, c_decimal_chunk_radix, work);

        if (!multiplied.has_value())
        {
          return status_result::failure(*multiplied.error_if());
        }
      }

      if (remaining_scale != 0)
      {
        static constexpr std::array<std::uint32_t, c_decimal_chunk_digits> c_tail_radix{1U, 10U, 100U, 1000U};
        const auto multiplied = scaled.append_digit(0, c_tail_radix[remaining_scale], work);

        if (!multiplied.has_value())
        {
          return status_result::failure(*multiplied.error_if());
        }
      }

      return status_result::success();
    }

    /**
     * @brief Shift the ratio so the quotient has quotient_bits + 1 bits, then align the divisor.
     * @par Requirements
     * P038; J5-037; J5-048; J5-049.
     * @param quotient_bits Index of the highest quotient bit; at most 63.
     * @param numerator Loaded numerator.
     * @param denominator Loaded denominator.
     * @param work Work budget.
     * @return The binary shift applied to the ratio, or the work or size failure.
     * @note Rejects operands longer than PTRDIFF_MAX / 4 bits, so the returned shift is within
     * +/- (PTRDIFF_MAX / 4 + quotient_bits); round_quotient() relies on that bound.
     * @par ID SWU-VALUE-189
     */
    [[nodiscard]] result<std::ptrdiff_t> align_ratio(std::ptrdiff_t quotient_bits, detail::big_unsigned& numerator,
                                                     detail::big_unsigned& denominator, work_budget& work) noexcept
    {
      using shift_result = result<std::ptrdiff_t>;

      // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
      // every path assigns the real value before use. For constants and results, the value is read later in the
      // same function or template instantiation, which the tool does not track. Compile-time constant: the
      // maximum of std::ptrdiff_t is converted to std::size_t and divided by four, which cannot overflow.
      // codeql::misra_deviation_next_line(DEV-007.15)
      constexpr std::size_t c_max_bits = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / 4U;

      const auto numerator_length = numerator.bit_length();

      if (!numerator_length.has_value())
      {
        return shift_result::failure(*numerator_length.error_if());
      }

      const auto denominator_length = denominator.bit_length();

      if (!denominator_length.has_value())
      {
        return shift_result::failure(*denominator_length.error_if());
      }

      const std::size_t numerator_bits = *numerator_length.value_if();
      const std::size_t denominator_bits = *denominator_length.value_if();

      // Keeps the signed exponent arithmetic below far from overflow.
      if ((numerator_bits > c_max_bits) || (denominator_bits > c_max_bits))
      {
        return shift_result::failure(error::limit_without_location(resource_limit_reason::conversion_size));
      }

      // After shifting, the quotient lies in [2^(P+1), 2^(P+3)): enough bits to round once.
      const std::ptrdiff_t shift =
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: Both bit counts are at most c_max_bits (PTRDIFF_MAX / 4),
        // checked above, so their difference is within +/- PTRDIFF_MAX / 4 and subtracting
        // it from quotient_bits cannot overflow std::ptrdiff_t.
        // codeql::misra_deviation_next_line(DEV-005.07)
        quotient_bits - (static_cast<std::ptrdiff_t>(numerator_bits) - static_cast<std::ptrdiff_t>(denominator_bits));

      if (shift > 0)
      {
        // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
        // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
        // every path assigns the real value before use. For constants and results, the value is read later in
        // the same function or template instantiation, which the tool does not track.
        // codeql::misra_deviation_next_line(DEV-007.10)
        const auto widened = numerator.shift_left(static_cast<std::size_t>(shift), work);

        if (!widened.has_value())
        {
          return shift_result::failure(*widened.error_if());
        }
      }
      else if (shift < 0)
      {
        // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
        // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
        // every path assigns the real value before use. For constants and results, the value is read later in
        // the same function or template instantiation, which the tool does not track.
        // codeql::misra_deviation_next_line(DEV-007.09)
        const auto widened = denominator.shift_left(static_cast<std::size_t>(-shift), work);

        if (!widened.has_value())
        {
          return shift_result::failure(*widened.error_if());
        }
      }
      else
      {
        // A zero shift means the operands are already aligned.
      }

      // Aligning the divisor with the highest quotient bit replaces division with compare/subtract.
      // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
      // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
      // every path assigns the real value before use. For constants and results, the value is read later in the
      // same function or template instantiation, which the tool does not track.
      // codeql::misra_deviation_next_line(DEV-007.08)
      const auto aligned = denominator.shift_left(static_cast<std::size_t>(quotient_bits), work);

      if (!aligned.has_value())
      {
        return shift_result::failure(*aligned.error_if());
      }

      return shift_result::success(shift);
    }

    /**
     * @brief Divide by compare and subtract, producing quotient_bits + 1 quotient bits.
     * @par Requirements
     * P038; J5-037; J5-048; J5-049.
     * @param quotient_bits Index of the highest quotient bit; at most 63.
     * @param numerator Aligned numerator; holds the remainder afterwards.
     * @param denominator Aligned denominator.
     * @param work Work budget.
     * @return The truncated quotient, or the work failure.
     * @par ID SWU-VALUE-190
     */
    [[nodiscard]] result<std::uint_least64_t> divide_ratio(std::ptrdiff_t quotient_bits,
                                                           detail::big_unsigned& numerator,
                                                           const detail::big_unsigned& denominator,
                                                           work_budget& work) noexcept
    {
      using quotient_result = result<std::uint_least64_t>;

      std::uint_least64_t quotient{};

      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
      // bound that the body does not modify, and changed only in the increment expression. The counter is
      // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
      // codeql::misra_deviation_next_line(DEV-017.01)
      for (std::ptrdiff_t bit = quotient_bits; bit >= 0; --bit)
      {
        const auto order = numerator.compare(denominator, work);

        if (!order.has_value())
        {
          return quotient_result::failure(*order.error_if());
        }

        if (*order.value_if() != detail::magnitude_order::less)
        {
          const auto reduced = numerator.subtract(denominator, work);

          if (!reduced.has_value())
          {
            return quotient_result::failure(*reduced.error_if());
          }

          quotient |= (static_cast<std::uint_least64_t>(1U) << static_cast<std::uint32_t>(bit));
        }

        if (bit != 0)
        {
          const auto doubled = numerator.shift_left(1, work);

          if (!doubled.has_value())
          {
            return quotient_result::failure(*doubled.error_if());
          }
        }
      }

      return quotient_result::success(quotient);
    }

    /**
     * @brief Round the quotient once, to nearest with ties to even, and scale it to T.
     * @par Requirements
     * P038; J5-037; J5-048; J5-049.
     * @tparam T Destination type: float or double.
     * @param quotient Truncated quotient from divide_ratio().
     * @param sticky Whether a nonzero remainder lies below the quotient.
     * @param shift Shift returned by align_ratio().
     * @param negative Sign of the original value.
     * @param options Conversion policies.
     * @return The correctly rounded value, the underflow result or an out_of_range failure.
     * @pre |shift| is at most PTRDIFF_MAX / 4 + 55, as align_ratio() guarantees; the MISRA
     * justifications below rely on this bound.
     * @par ID SWU-VALUE-191
     */
    template <typename T>
    [[nodiscard]] result<T> round_quotient(std::uint_least64_t quotient, bool sticky, std::ptrdiff_t shift,
                                           bool negative, const floating_conversion_options& options) noexcept
    {
      using float_result = result<T>;
      using limits = std::numeric_limits<T>;

      constexpr auto c_precision = static_cast<std::ptrdiff_t>(limits::digits);
      // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
      // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
      // every path assigns the real value before use. For constants and results, the value is read later in the
      // same function or template instantiation, which the tool does not track.
      // codeql::misra_deviation_next_line(DEV-007.16)
      constexpr std::ptrdiff_t c_min_normal_exponent = limits::min_exponent - 1;

      const auto quotient_bits = static_cast<std::ptrdiff_t>(bit_length_of(quotient));
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: quotient_bits is 0 to 64 and |shift| is at most
      // PTRDIFF_MAX / 4 + 55 because the bit counts are limited by c_max_bits, so
      // neither subtraction can overflow std::ptrdiff_t.
      // codeql::misra_deviation_next_line(DEV-005.06)
      const std::ptrdiff_t leading_exponent = (quotient_bits - 1) - shift;

      // Below the normal range, each lower exponent leaves one fewer significand bit.
      std::ptrdiff_t kept_bits = c_precision;

      if (leading_exponent < c_min_normal_exponent)
      {
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: In this branch leading_exponent < c_min_normal_exponent, so
        // the difference is positive and at most PTRDIFF_MAX / 4 + 1200 (|leading_exponent|
        // is bounded through shift). Subtracting it from c_precision cannot overflow.
        // codeql::misra_deviation_next_line(DEV-005.05)
        kept_bits -= (c_min_normal_exponent - leading_exponent);
      }

      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: quotient_bits is 0 to 64 and kept_bits is at most c_precision,
      // and at least c_precision - (PTRDIFF_MAX / 4 + 1200) after the adjustment above.
      // The difference cannot overflow std::ptrdiff_t.
      // codeql::misra_deviation_next_line(DEV-005.04)
      std::ptrdiff_t dropped_bits = quotient_bits - kept_bits;

      // One bit beyond the quotient already rounds to zero; clamping keeps shifts below 64.
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: quotient_bits is the bit length of a 64-bit value, 0 to 64,
      // so adding one cannot overflow std::ptrdiff_t.
      // codeql::misra_deviation_next_line(DEV-005.03)
      const std::ptrdiff_t max_dropped_bits = quotient_bits + 1;

      // One bit beyond the quotient already rounds to zero; clamping keeps shifts below 64.
      if (dropped_bits > max_dropped_bits)
      {
        dropped_bits = max_dropped_bits;
      }

      const auto drop = static_cast<std::uint32_t>(dropped_bits);
      const std::uint_least64_t rounding_unit = static_cast<std::uint_least64_t>(1U) << drop;
      const std::uint_least64_t half = rounding_unit >> 1U;
      const std::uint_least64_t remainder = quotient % rounding_unit;
      std::uint_least64_t significand = quotient >> drop;

      // Round to nearest; an exact tie (no sticky bits) chooses the even significand.
      if ((remainder > half) || ((remainder == half) && (sticky || ((significand & 1U) != 0))))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: significand is quotient >> drop, and the quotient holds at
        // most c_quotient_bits + 1 (56) bits, so significand is below 2^56. Adding one
        // cannot wrap a 64-bit unsigned value. A carry into a new bit is handled below
        // as an exponent adjustment.
        // codeql::misra_deviation_next_line(DEV-004.03)
        significand += 1U;
      }

      if (significand == 0)
      {
        return finish_underflow<T>(negative, options);
      }

      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: dropped_bits is between -53 and quotient_bits + 1 (at most 65),
      // and |shift| is at most PTRDIFF_MAX / 4 + 55 because the bit counts are limited by
      // c_max_bits. The difference cannot overflow std::ptrdiff_t.
      // codeql::misra_deviation_next_line(DEV-005.02)
      const std::ptrdiff_t scale_exponent = dropped_bits - shift;

      // Rounding may carry into a new leading bit, so overflow is checked after rounding.
      const std::ptrdiff_t rounded_leading =
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: significand is nonzero (checked above), so the bit length
        // minus one is 0 to 63. scale_exponent is at most PTRDIFF_MAX / 4 + 120 in
        // magnitude (see the bounds at shift), so the sum cannot overflow std::ptrdiff_t.
        // codeql::misra_deviation_next_line(DEV-005.01)
        (static_cast<std::ptrdiff_t>(bit_length_of(significand)) - 1) + scale_exponent;

      if (rounded_leading >= limits::max_exponent)
      {
        return float_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      // The significand fits in T's precision and the exponent is in range, so scaling is exact.
      // Plain int on purpose: the standard declares std::ldexp(T, int), so the exponent must be
      // exactly int. std::int32_t would be long on 16-bit-int targets and narrow silently here. The range
      // checks above keep the exponent within a few thousand, inside the 16-bit minimum range of int.
      const T magnitude = std::ldexp(static_cast<T>(significand), static_cast<int>(scale_exponent));

      return float_result::success(negative ? -magnitude : magnitude);
    }

    /**
     * @brief Own conversion scratch through ratio construction, division and final rounding.
     * @par Requirements
     * P038; J5-003; J5-037; J5-038; J5-046; J5-048; J5-052.
     * @par ID SWU-VALUE-204
     */
    template <typename T>
    [[nodiscard]] result<T> convert_exact_ratio(input_view spelling, const detail::number_analysis& parts,
                                                const storage_callbacks& scratch,
                                                const floating_conversion_options& options, work_budget& work) noexcept
    {
      using float_result = result<T>;
      using limits = std::numeric_limits<T>;

      constexpr auto c_precision = static_cast<std::ptrdiff_t>(limits::digits);
      // Two extra quotient bits guarantee a rounding bit below the significand.
      // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: Compile-time constant: c_precision is 53 for double and 24
      // for float, so the result is 55 or 26. The static_assert below rejects any other
      // value that would leave the 64-bit quotient.
      // Initialized at the declaration so that no path can read an indeterminate value; every path assigns the
      // real value before use. For constants and results, the value is read later in the same function or
      // template instantiation, which the tool does not track.
      // codeql::misra_deviation_next_line(DEV-005.08)
      constexpr std::ptrdiff_t c_quotient_bits = c_precision + 2;
      static_assert(c_quotient_bits < 64, "The quotient must fit std::uint_least64_t.");
      // Declared before integers so it outlives every allocation it accounts for.
      accounted_storage arena{scratch, options.max_scratch_bytes};
      const storage_callbacks provider = arena.callbacks();

      detail::big_unsigned numerator{provider};
      detail::big_unsigned denominator{provider};

      const auto reserved =
        reserve_operands(parts, static_cast<std::size_t>(c_quotient_bits), numerator, denominator, work);

      if (!reserved.has_value())
      {
        return float_result::failure(*reserved.error_if());
      }

      const auto loaded = load_ratio(spelling, parts, numerator, denominator, work);

      if (!loaded.has_value())
      {
        return float_result::failure(*loaded.error_if());
      }

      const auto shift = align_ratio(c_quotient_bits, numerator, denominator, work);

      if (!shift.has_value())
      {
        return float_result::failure(*shift.error_if());
      }

      const auto quotient = divide_ratio(c_quotient_bits, numerator, denominator, work);

      if (!quotient.has_value())
      {
        return float_result::failure(*quotient.error_if());
      }

      // Any remainder means the exact value lies strictly above the truncated quotient.
      const bool sticky = !numerator.is_zero();

      return round_quotient<T>(*quotient.value_if(), sticky, *shift.value_if(), parts.negative, options);
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
     * @par ID SWU-VALUE-053
     */
    template <typename T>
    [[nodiscard]] result<T> convert_to_floating(const owned_number& number, const storage_callbacks& scratch,
                                                const floating_conversion_options& options) noexcept
    {
      using float_result = result<T>;
      work_budget work{options.max_work_units};

      const auto analysis = detail::analyse_number(number, work);

      if (!analysis.has_value())
      {
        return float_result::failure(*analysis.error_if());
      }

      // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
      // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
      // every path assigns the real value before use. For constants and results, the value is read later in the
      // same function or template instantiation, which the tool does not track.
      // codeql::misra_deviation_next_line(DEV-007.14)
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

      const auto by_scale = reject_by_scale<T>(parts, options);

      if (!by_scale.has_value())
      {
        return float_result::failure(*by_scale.error_if());
      }

      if (by_scale.value_if()->has_value())
      {
        return float_result::success(**by_scale.value_if());
      }

      // The fast steps assume IEEE binary32 or binary64, and their constants do not compile for
      // other formats. convert_special_floating() rejects those at run time, so drop the
      // call at compile time too.
      if constexpr (detail::supports_floating_conversion<T>())
      {
        // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
        // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
        // every path assigns the real value before use. For constants and results, the value is read later in
        // the same function or template instantiation, which the tool does not track.
        // codeql::misra_deviation_next_line(DEV-007.13)
        const auto fast = try_fast_path<T>(number.spelling(), parts, work);

        if (!fast.has_value())
        {
          return float_result::failure(*fast.error_if());
        }

        if (fast.value_if()->has_value())
        {
          return float_result::success(**fast.value_if());
        }
      }

      return convert_exact_ratio<T>(number.spelling(), parts, scratch, options, work);
    }
  } // namespace

  /**
   * @brief Convert directly to float without an intermediate double.
   * @par Requirements
   * P038; J5-003; J5-037; J5-038; J5-046; J5-048; J5-052.
   * @par ID SWU-VALUE-054
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
   * @par ID SWU-VALUE-055
   */
  result<double> to_double(const owned_number& number, const storage_callbacks& scratch,
                           const floating_conversion_options& options) noexcept
  {
    return convert_to_floating<double>(number, scratch, options);
  }
} // namespace adaptive_json5
