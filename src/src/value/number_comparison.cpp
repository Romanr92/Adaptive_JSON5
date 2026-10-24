#include "adaptive_json5/value/number_comparison.hpp"

#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/value/big_unsigned.hpp"
#include "adaptive_json5/detail/value/number_analysis.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cstddef>
#include <limits>

namespace adaptive_json5
{
  namespace
  {
    constexpr unsigned int c_decimal_radix = 10;
    constexpr unsigned int c_hexadecimal_radix = 16;
    constexpr unsigned int c_hexadecimal_letter_offset = 10;
    constexpr std::size_t c_hexadecimal_prefix_bytes = 2;

    // Leaves headroom so exponent sums and differences cannot overflow ptrdiff_t.
    constexpr auto c_max_exponent = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max() / 4);

    /**
     * @brief Describe a finite nonzero value as digits times an exact power of ten.
     * @par Requirements
     * P038; J5-036.
     * @note The digits are accumulated separately in a big_unsigned.
     */
    struct [[nodiscard]] decimal_form
    {
        std::ptrdiff_t scale{};            // Power of ten applied to the digits.
        std::ptrdiff_t leading_exponent{}; // Power of ten of the leading nonzero digit.
    };

    /**
     * @brief Convert one validated hexadecimal digit character to its value.
     * @par Requirements
     * P038; J5-036.
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
     * @brief Count the decimal digits of a nonzero exact integer.
     * @par Requirements
     * P038; J5-036; J5-048.
     * @param value Nonzero magnitude to measure.
     * @param provider Scratch storage for the growing power of ten.
     * @param work Shared comparison work budget.
     * @return Number of decimal digits or an explicit resource/storage failure.
     * @note Grows 10^count until it exceeds value; work is bounded by the value's size.
     */
    [[nodiscard]] result<std::size_t> count_decimal_digits(const detail::big_unsigned& value,
                                                           const storage_callbacks& provider,
                                                           work_budget& work) noexcept
    {
      using counter_result = result<std::size_t>;

      detail::big_unsigned power{provider};

      const auto one = power.append_digit(1, c_decimal_radix, work);

      if (!one.has_value())
      {
        return counter_result::failure(*one.error_if());
      }

      std::size_t count{};

      // 10^count <= value means value still has more than count digits.
      while (true)
      {
        const auto order = power.compare(value, work);

        if (!order.has_value())
        {
          return counter_result::failure(*order.error_if());
        }

        if (*order.value_if() == detail::magnitude_order::greater)
        {
          return counter_result::success(count);
        }

        const auto multiplied = power.append_digit(0, c_decimal_radix, work);

        if (!multiplied.has_value())
        {
          return counter_result::failure(*multiplied.error_if());
        }

        ++count;
      }
    }

    /**
     * @brief Read a finite nonzero spelling into digits and exact decimal exponents.
     * @par Requirements
     * P038; J5-003; J5-036; J5-048.
     * @param spelling Validated finite nonzero numeric spelling.
     * @param kind Decimal or hexadecimal spelling family.
     * @param digits Exact zero receiving every significand digit.
     * @param provider Scratch storage for hexadecimal digit counting.
     * @param work Shared comparison work budget.
     * @return Exact scale and leading exponent, or an explicit failure.
     * @note Exponents above c_max_exponent return resource_limit instead of saturating,
     * because saturation would make different values compare equal.
     */
    [[nodiscard]] result<decimal_form> read_decimal_form(input_view spelling, number_kind kind,
                                                         detail::big_unsigned& digits,
                                                         const storage_callbacks& provider, work_budget& work) noexcept
    {
      using form_result = result<decimal_form>;

      // Digit counts below are bounded by the spelling, so bound it like the exponent.
      if (spelling.size > c_max_exponent)
      {
        return form_result::failure(error::without_location(error_code::resource_limit));
      }

      decimal_form form{};
      std::size_t position{};

      if ((spelling.data[position] == '+') || (spelling.data[position] == '-'))
      {
        ++position;
      }

      if (kind == number_kind::hexadecimal)
      {
        position += c_hexadecimal_prefix_bytes;

        for (; position < spelling.size; ++position)
        {
          const auto appended =
            digits.append_digit(hexadecimal_digit_value(spelling.data[position]), c_hexadecimal_radix, work);

          if (!appended.has_value())
          {
            return form_result::failure(*appended.error_if());
          }
        }

        // Hexadecimal digits do not reveal the decimal leading exponent, so count it exactly.
        const auto count = count_decimal_digits(digits, provider, work);

        if (!count.has_value())
        {
          return form_result::failure(*count.error_if());
        }

        form.leading_exponent = static_cast<std::ptrdiff_t>(*count.value_if()) - 1;
        return form_result::success(form);
      }

      bool after_point{};
      bool seen_nonzero{};
      std::size_t fractional_digits{};
      std::size_t significant_digits{};

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

        const auto digit = static_cast<unsigned int>(current - '0');
        const auto appended = digits.append_digit(digit, c_decimal_radix, work);

        if (!appended.has_value())
        {
          return form_result::failure(*appended.error_if());
        }

        if (after_point)
        {
          ++fractional_digits;
        }

        // Leading zeros do not move the leading exponent; every digit after the first nonzero does.
        if (digit != 0)
        {
          seen_nonzero = true;
        }

        if (seen_nonzero)
        {
          ++significant_digits;
        }
      }

      bool exponent_negative{};
      std::size_t exponent{};

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

          // Comparison must stay exact, so an oversized exponent is a limit failure, not a saturation.
          if (exponent > ((c_max_exponent - digit) / c_decimal_radix))
          {
            return form_result::failure(error::without_location(error_code::resource_limit));
          }

          exponent = (exponent * c_decimal_radix) + digit;
        }
      }

      const auto signed_exponent = static_cast<std::ptrdiff_t>(exponent);

      form.scale =
        (exponent_negative ? -signed_exponent : signed_exponent) - static_cast<std::ptrdiff_t>(fractional_digits);
      form.leading_exponent = form.scale + static_cast<std::ptrdiff_t>(significant_digits) - 1;

      return form_result::success(form);
    }

    /**
     * @brief Translate a private magnitude ordering into the public ordering.
     * @par Requirements
     * P038; J5-036.
     */
    [[nodiscard]] number_order to_number_order(detail::magnitude_order order) noexcept
    {
      switch (order)
      {
      case detail::magnitude_order::less:
        return number_order::less;
      case detail::magnitude_order::equal:
        return number_order::equal;
      case detail::magnitude_order::greater:
        return number_order::greater;
      }

      return number_order::unordered;
    }

    /**
     * @brief Compare the magnitudes of two finite nonzero numbers exactly.
     * @par Requirements
     * P038; J5-003; J5-036; J5-046; J5-048.
     * @param left Finite nonzero left operand.
     * @param right Finite nonzero right operand.
     * @param scratch Caller-owned temporary storage.
     * @param options Scratch limit for this comparison.
     * @param work Shared comparison work budget.
     * @return Magnitude ordering or an explicit failure.
     * @note Releases all scratch before returning.
     */
    [[nodiscard]] result<number_order> compare_finite_magnitudes(const owned_number& left, const owned_number& right,
                                                                 const storage_callbacks& scratch,
                                                                 const number_comparison_options& options,
                                                                 work_budget& work) noexcept
    {
      using order_result = result<number_order>;

      // Declared before the integers so it outlives every allocation it accounts for.
      accounted_storage arena{scratch, options.max_scratch_bytes};
      const storage_callbacks provider = arena.callbacks();

      detail::big_unsigned left_digits{provider};
      detail::big_unsigned right_digits{provider};

      const auto left_form = read_decimal_form(left.spelling(), left.kind(), left_digits, provider, work);

      if (!left_form.has_value())
      {
        return order_result::failure(*left_form.error_if());
      }

      const auto right_form = read_decimal_form(right.spelling(), right.kind(), right_digits, provider, work);

      if (!right_form.has_value())
      {
        return order_result::failure(*right_form.error_if());
      }

      const decimal_form& left_parts = *left_form.value_if();
      const decimal_form& right_parts = *right_form.value_if();

      // Different leading exponents decide the order without building any power of ten.
      if (left_parts.leading_exponent != right_parts.leading_exponent)
      {
        return order_result::success(
          (left_parts.leading_exponent < right_parts.leading_exponent) ? number_order::less : number_order::greater);
      }

      // Equal leading exponents bound the scale difference by the digit counts, so scaling stays cheap.
      const bool left_larger_scale = left_parts.scale > right_parts.scale;
      detail::big_unsigned& scaled = left_larger_scale ? left_digits : right_digits;
      const std::ptrdiff_t difference =
        left_larger_scale ? (left_parts.scale - right_parts.scale) : (right_parts.scale - left_parts.scale);

      for (std::ptrdiff_t index{}; index < difference; ++index)
      {
        const auto multiplied = scaled.append_digit(0, c_decimal_radix, work);

        if (!multiplied.has_value())
        {
          return order_result::failure(*multiplied.error_if());
        }
      }

      const auto order = left_digits.compare(right_digits, work);

      if (!order.has_value())
      {
        return order_result::failure(*order.error_if());
      }

      return order_result::success(to_number_order(*order.value_if()));
    }

    /**
     * @brief Classify a non-NaN value as negative, zero or positive.
     * @par Requirements
     * P038; J5-036; J5-038.
     * @return -1, 0 or 1; zero ignores its spelled sign.
     */
    [[nodiscard]] int sign_of(const detail::number_analysis& analysis) noexcept
    {
      if (analysis.is_zero)
      {
        return 0;
      }

      return analysis.negative ? -1 : 1;
    }

    /**
     * @brief Mirror an ordering for two negative operands.
     * @par Requirements
     * P038; J5-036.
     */
    [[nodiscard]] number_order reversed(number_order order) noexcept
    {
      if (order == number_order::less)
      {
        return number_order::greater;
      }

      if (order == number_order::greater)
      {
        return number_order::less;
      }

      return order;
    }

  } // namespace

  /**
   * @brief Classify signs and special values, then compare finite magnitudes exactly.
   * @par Requirements
   * P038; J5-003; J5-036; J5-038; J5-046; J5-048; J5-052.
   */
  result<number_order> compare_numbers(const owned_number& left, const owned_number& right,
                                       const storage_callbacks& scratch,
                                       const number_comparison_options& options) noexcept
  {
    using order_result = result<number_order>;

    work_budget work{options.max_work_units};

    const auto left_analysis = detail::analyse_number(left, work);

    if (!left_analysis.has_value())
    {
      return order_result::failure(*left_analysis.error_if());
    }

    const auto right_analysis = detail::analyse_number(right, work);

    if (!right_analysis.has_value())
    {
      return order_result::failure(*right_analysis.error_if());
    }

    const detail::number_analysis& left_info = *left_analysis.value_if();
    const detail::number_analysis& right_info = *right_analysis.value_if();

    // NaN has no position in the ordering, not even relative to itself.
    if ((left_info.kind == number_kind::nan) || (right_info.kind == number_kind::nan))
    {
      return order_result::success(number_order::unordered);
    }

    const int left_sign = sign_of(left_info);
    const int right_sign = sign_of(right_info);

    if (left_sign != right_sign)
    {
      return order_result::success((left_sign < right_sign) ? number_order::less : number_order::greater);
    }

    // Equal zero sign mean both are zero, including -0 compared with 0.
    if (left_sign == 0)
    {
      return order_result::success(number_order::equal);
    }

    const bool left_infinite = (left_info.kind == number_kind::infinity);
    const bool right_infinite = (right_info.kind == number_kind::infinity);
    number_order magnitude = number_order::equal;

    if (left_infinite || right_infinite)
    {
      if (left_infinite != right_infinite)
      {
        magnitude = left_infinite ? number_order::greater : number_order::less;
      }
    }
    else
    {
      const auto finite = compare_finite_magnitudes(left, right, scratch, options, work);

      if (!finite.has_value())
      {
        return order_result::failure(*finite.error_if());
      }

      magnitude = *finite.value_if();
    }

    // Both operands share one sign; a larger negative magnitude is a smaller value.
    return order_result::success((left_sign > 0) ? magnitude : reversed(magnitude));
  }
} // namespace adaptive_json5
