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
    constexpr std::int64_t c_scale_cap = std::int64_t{1} << 40U;

    /**
     * @brief A finite number as text: integer and fraction digits, radix and power of ten.
     * @par Requirements
     * P048; J5-059.
     */
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
      std::uint32_t value = 16U;
      if ((c >= '0') && (c <= '9'))
      {
        value = static_cast<std::uint32_t>(c - '0');
      }
      else if ((c >= 'a') && (c <= 'f'))
      {
        value = static_cast<std::uint32_t>(c - 'a') + 10U;
      }
      else if ((c >= 'A') && (c <= 'F'))
      {
        value = static_cast<std::uint32_t>(c - 'A') + 10U;
      }
      else
      {
        return std::nullopt;
      }
      return (value < radix) ? std::optional<std::uint32_t>{value} : std::nullopt;
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
      if ((position < text.size()) && ((text[position] == '+') || (text[position] == '-')))
      {
        negative = text[position] == '-';
        ++position;
      }
      std::int64_t value = 0;
      for (; position < text.size(); ++position)
      {
        if (value < c_scale_cap)
        {
          value = (value * 10) + static_cast<std::int64_t>(text[position] - '0');
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
      if (!text.empty() && ((text[0U] == '+') || (text[0U] == '-')))
      {
        text.remove_prefix(1U);
      }
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
        parts.exponent = read_exponent(text.substr(exponent + 1U));
        text = text.substr(0U, exponent);
      }
      const std::size_t point = text.find('.');
      parts.integer = text.substr(0U, point);
      parts.fraction = (point == std::string_view::npos) ? std::string_view{} : text.substr(point + 1U);
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
      const std::size_t written = parts.integer.size() + parts.fraction.size();
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
      for (std::int64_t count = 0; built.has_value() && (count < zeros); ++count)
      {
        const auto appended = target.append_digit(0U, 10U, work);
        built = appended.has_value() ? status::success() : status::failure(failure_of(appended));
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
      for (std::size_t shift = (*value_bits.value_if() - *divisor_bits.value_if()) + 1U; shift != 0U; --shift)
      {
        auto step = shifted.assign(divisor, work);
        if (step.has_value())
        {
          step = shifted.shift_left(shift - 1U, work);
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
    if ((spelling.size != 0U) && (spelling.data[0U] == '-'))
    {
      return bool_result::success(false);
    }
    for (const std::string_view digits : {parts->integer, parts->fraction})
    {
      for (const char c : digits)
      {
        if (c != '0')
        {
          return bool_result::success(true);
        }
      }
    }
    return bool_result::success(false);
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
    const auto dividend_digits = scaled_digits(*value_parts, value_parts->exponent - common);
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
      built = build_scaled(dividend, *value_parts, value_parts->exponent - common, work);
    }
    if (built.has_value())
    {
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
