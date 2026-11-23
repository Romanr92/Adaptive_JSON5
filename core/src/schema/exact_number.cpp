#include "schema/exact_number.hpp"

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "memory/work_budget.hpp"
#include "value/big_unsigned.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  namespace
  {
    using bool_result = result<bool, schema_error>;
    using status = result<void, schema_error>;

    // Scales beyond this cannot be built within the parser's default work limit anyway.
    constexpr std::int64_t c_scale_cap = 1099511627776; // 2^40
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.1)
    constexpr std::string_view c_signs{"+-"};
    constexpr std::size_t c_minus_index = 1U;

    /**
     * @brief A finite number as text: integer and fraction digits, radix and power of ten.
     * @par Requirements
     * P048; J5-059.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.31)
    struct decimal_parts
    {
        std::string_view integer{};
        std::string_view fraction{};
        std::uint32_t radix{10U};
        std::int64_t exponent{};
    };

    /**
     * @brief Return the value of a digit in radix, or nullopt.
     * @par Requirements
     * P048; J5-059.
     * @par ID SWU-SCHEMA-218
     */
    std::optional<std::uint32_t> digit_value(char c, std::uint32_t radix) noexcept
    {
      constexpr std::string_view c_small_digits{"0123456789abcdef"};
      constexpr std::string_view c_capital_digits{"0123456789ABCDEF"};
      const std::size_t small = c_small_digits.find(c);
      const std::size_t found = (small != std::string_view::npos) ? small : c_capital_digits.find(c);
      return ((found != std::string_view::npos) && (found < radix))
               ? std::optional<std::uint32_t>{static_cast<std::uint32_t>(found)}
               : std::nullopt;
    }

    /**
     * @brief Read the signed decimal exponent after 'e' or 'E', saturating at c_scale_cap.
     * @par Requirements
     * P048; J5-059.
     * @par ID SWU-SCHEMA-219
     */
    std::int64_t read_exponent(std::string_view text) noexcept
    {
      bool negative = false;
      std::size_t position = 0U;
      if ((position < text.size()) && (c_signs.find(text[position]) != std::string_view::npos))
      {
        negative = c_signs.find(text[position]) == c_minus_index;
        ++position;
      }
      std::int64_t value = 0;
      for (const char c : text.substr(position))
      {
        if (value < c_scale_cap)
        {
          // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
          // MISRA justification: Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are
          // bounded by the document size, so sums and differences of two exponents stay far inside std::int64_t;
          // CodeQL does not follow the saturation.
          // codeql::misra_deviation_next_line(DEV-113.7)
          value = (value * 10) + static_cast<std::int64_t>(digit_value(c, 10U).value_or(0U));
        }
      }
      return negative ? -value : value;
    }

    /**
     * @brief Split a finite number spelling (decimal or JSON5 hexadecimal) into its parts.
     * @par Requirements
     * P048; J5-059.
     * @return The parts, or nullopt for Infinity/NaN.
     * @par ID SWU-SCHEMA-220
     */
    std::optional<decimal_parts> split_number(const owned_number& number) noexcept
    {
      if ((number.kind() == number_kind::infinity) || (number.kind() == number_kind::nan))
      {
        return std::nullopt;
      }
      const input_view spelling = number.spelling();
      std::string_view text{spelling.data, spelling.size};
      if (!text.empty() && (c_signs.find(text[0U]) != std::string_view::npos))
      {
        text.remove_prefix(1U);
      }
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.39)
      decimal_parts parts{};
      if (number.kind() == number_kind::hexadecimal)
      {
        parts.integer = text.substr(2U);
        parts.radix = 16U;
        return parts;
      }
      const std::size_t exponent = text.find_first_of("eE");
      if (exponent != std::string_view::npos)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.92)
        parts.exponent = read_exponent(text.substr(exponent + 1U));
        text = text.substr(0U, exponent);
      }
      const std::size_t point = text.find('.');
      parts.integer = text.substr(0U, point);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.91)
      parts.fraction = (point == std::string_view::npos) ? std::string_view{} : text.substr(point + 1U);
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are bounded
      // by the document size, so sums and differences of two exponents stay far inside std::int64_t; CodeQL does not
      // follow the saturation.
      // codeql::misra_deviation_next_line(DEV-113.6)
      parts.exponent -= static_cast<std::int64_t>(parts.fraction.size());
      return parts;
    }

    /**
     * @brief Translate a parser arithmetic failure.
     * @par Requirements
     * P048; J5-043.
     * @par ID SWU-SCHEMA-221
     */
    schema_error failure_of(const status_result& failed) noexcept
    {
      const error* reason = failed.error_if();
      return (reason == nullptr) ? schema_error::of(schema_error_code::internal_error)
                                 : schema_error::from_storage(*reason);
    }

    /**
     * @brief Append the digits of one digit run to an integer.
     * @par Requirements
     * P048; J5-059.
     * @par ID SWU-SCHEMA-222
     */
    status append_digits(big_unsigned& target, std::string_view digits, std::uint32_t radix, work_budget& work) noexcept
    {
      for (const char c : digits)
      {
        const auto digit = digit_value(c, radix);
        if (!digit.has_value())
        {
          return status::failure(schema_error::of(schema_error_code::internal_error));
        }
        const auto appended = target.append_digit(*digit, radix, work);
        if (!appended.has_value())
        {
          return status::failure(failure_of(appended));
        }
      }
      return status::success();
    }

    /**
     * @brief Return the decimal digit count of significand x 10^zeros, or nullopt above the work limit.
     * @par Requirements
     * P048; J5-059; J5-065.
     * @par ID SWU-SCHEMA-229
     */
    std::optional<std::size_t> scaled_digits(const decimal_parts& parts, std::int64_t zeros) noexcept
    {
      const std::size_t limit = resource_limits{}.max_work_units;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.90)
      const std::size_t written = parts.integer.size() + parts.fraction.size();
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.89)
      const std::size_t digits = (parts.radix == 16U) ? (written * 2U) : written;
      if ((zeros < 0) || (static_cast<std::uint64_t>(zeros) > static_cast<std::uint64_t>(limit)) ||
          (digits > (limit - static_cast<std::size_t>(zeros))))
      {
        return std::nullopt;
      }
      return digits + static_cast<std::size_t>(zeros);
    }

    /**
     * @brief Reserve limbs for an integer of at most digits decimal digits.
     * @par Requirements
     * P048; J5-046; J5-059.
     * @par ID SWU-SCHEMA-230
     */
    status reserve_digits(big_unsigned& target, std::size_t digits) noexcept
    {
      const auto bits = decimal_digit_bits(digits);
      const auto limbs =
        bits.has_value() ? limbs_for_bits(*bits.value_if()) : result<std::size_t>::failure(*bits.error_if());
      if (!limbs.has_value())
      {
        return status::failure(schema_error::from_storage(*limbs.error_if()));
      }
      const auto reserved = target.reserve(*limbs.value_if());
      return reserved.has_value() ? status::success() : status::failure(failure_of(reserved));
    }

    /**
     * @brief Build significand x 10^zeros as an integer.
     * @par Requirements
     * P048; J5-059.
     * @par ID SWU-SCHEMA-223
     */
    status build_scaled(big_unsigned& target, const decimal_parts& parts, std::int64_t zeros,
                        work_budget& work) noexcept
    {
      auto built = append_digits(target, parts.integer, parts.radix, work);
      if (built.has_value())
      {
        built = append_digits(target, parts.fraction, parts.radix, work);
      }
      std::int64_t count = 0;
      while (built.has_value() && (count < zeros))
      {
        const auto appended = target.append_digit(0U, 10U, work);
        built = appended.has_value() ? status::success() : status::failure(failure_of(appended));
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are bounded
        // by the document size, so sums and differences of two exponents stay far inside std::int64_t; CodeQL does
        // not follow the saturation.
        // codeql::misra_deviation_next_line(DEV-113.5)
        ++count;
      }
      return built;
    }

    /**
     * @brief Replace value by value mod divisor (shift-and-subtract long division).
     * @par Requirements
     * P048; J5-059.
     * @par ID SWU-SCHEMA-224
     */
    status reduce_modulo(big_unsigned& value, const big_unsigned& divisor, big_unsigned& shifted,
                         work_budget& work) noexcept
    {
      const auto value_bits = value.bit_length();
      const auto divisor_bits = divisor.bit_length();
      if (!value_bits.has_value() || !divisor_bits.has_value())
      {
        return status::failure(schema_error::of(schema_error_code::internal_error));
      }
      if (*value_bits.value_if() < *divisor_bits.value_if())
      {
        return status::success();
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.88)
      std::size_t shift = (*value_bits.value_if() - *divisor_bits.value_if()) + 1U;
      while (shift != 0U)
      {
        --shift;
        auto step = shifted.assign(divisor, work);
        if (step.has_value())
        {
          step = shifted.shift_left(shift, work);
        }
        const auto order =
          step.has_value() ? value.compare(shifted, work) : result<magnitude_order>::failure(*step.error_if());
        if (!order.has_value())
        {
          return status::failure(schema_error::from_storage(*order.error_if()));
        }
        if (*order.value_if() != magnitude_order::less)
        {
          const auto subtracted = value.subtract(shifted, work);
          if (!subtracted.has_value())
          {
            return status::failure(failure_of(subtracted));
          }
        }
      }
      return status::success();
    }
  } // namespace

  /**
   * @brief Report whether a finite number is strictly greater than zero.
   * @par Requirements
   * P048; J5-054; J5-059.
   * @par ID SWU-SCHEMA-216
   */
  result<bool, schema_error> is_positive(const owned_number& number) noexcept
  {
    const auto parts = split_number(number);
    if (!parts.has_value())
    {
      return bool_result::failure(schema_error::of(schema_error_code::internal_error));
    }
    const input_view spelling = number.spelling();
    if ((spelling.size != 0U) && (c_signs.find(spelling.data[0U]) == c_minus_index))
    {
      return bool_result::success(false);
    }
    const bool nonzero_integer = parts->integer.find_first_not_of('0') != std::string_view::npos;
    const bool nonzero_fraction = parts->fraction.find_first_not_of('0') != std::string_view::npos;
    return bool_result::success(nonzero_integer || nonzero_fraction);
  }

  /**
   * @brief Report whether value is an integer multiple of divisor, exactly (no rounding).
   * @par Requirements
   * P048; J5-057; J5-059; Draft-07 Validation section 6.2.1.
   * @par ID SWU-SCHEMA-217
   */
  result<bool, schema_error> is_multiple_of(const owned_number& value, const owned_number& divisor,
                                            const storage_callbacks& scratch) noexcept
  {
    const auto value_parts = split_number(value);
    const auto divisor_parts = split_number(divisor);
    if (!value_parts.has_value() || !divisor_parts.has_value())
    {
      return bool_result::failure(schema_error::of(schema_error_code::internal_error));
    }
    const std::int64_t common =
      (value_parts->exponent < divisor_parts->exponent) ? value_parts->exponent : divisor_parts->exponent;
    // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
    // MISRA justification: Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are bounded by
    // the document size, so sums and differences of two exponents stay far inside std::int64_t; CodeQL does not
    // follow the saturation.
    // codeql::misra_deviation_next_line(DEV-113.4)
    const auto dividend_digits = scaled_digits(*value_parts, value_parts->exponent - common);
    // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
    // MISRA justification: Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are bounded by
    // the document size, so sums and differences of two exponents stay far inside std::int64_t; CodeQL does not
    // follow the saturation.
    // codeql::misra_deviation_next_line(DEV-113.3)
    const auto modulus_digits = scaled_digits(*divisor_parts, divisor_parts->exponent - common);
    if (!dividend_digits.has_value() || !modulus_digits.has_value())
    {
      return bool_result::failure(schema_error::from_storage(error::without_location(error_code::resource_limit)));
    }
    const std::size_t widest = (*dividend_digits > *modulus_digits) ? *dividend_digits : *modulus_digits;
    work_budget work{resource_limits{}.max_work_units};
    big_unsigned dividend{scratch};
    big_unsigned modulus{scratch};
    big_unsigned shifted{scratch};
    auto built = reserve_digits(dividend, widest);
    if (built.has_value())
    {
      built = reserve_digits(modulus, widest);
    }
    if (built.has_value())
    {
      built = reserve_digits(shifted, widest);
    }
    if (built.has_value())
    {
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are bounded
      // by the document size, so sums and differences of two exponents stay far inside std::int64_t; CodeQL does not
      // follow the saturation.
      // codeql::misra_deviation_next_line(DEV-113.2)
      built = build_scaled(dividend, *value_parts, value_parts->exponent - common, work);
    }
    if (built.has_value())
    {
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: Decimal exponents are saturated at 2^40 (c_scale_cap) and fraction lengths are bounded
      // by the document size, so sums and differences of two exponents stay far inside std::int64_t; CodeQL does not
      // follow the saturation.
      // codeql::misra_deviation_next_line(DEV-113.1)
      built = build_scaled(modulus, *divisor_parts, divisor_parts->exponent - common, work);
    }
    if (built.has_value() && modulus.is_zero())
    {
      built = status::failure(schema_error::of(schema_error_code::internal_error));
    }
    if (built.has_value())
    {
      built = reduce_modulo(dividend, modulus, shifted, work);
    }
    if (!built.has_value())
    {
      return bool_result::failure(*built.error_if());
    }
    return bool_result::success(dividend.is_zero());
  }
} // namespace adaptive_json5::detail
