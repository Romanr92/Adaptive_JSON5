#include "adaptive_json5/value/number_comparison.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "common/checked_size.hpp"
#include "memory/work_budget.hpp"
#include "value/big_unsigned.hpp"
#include "value/number_analysis.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace adaptive_json5
{
  namespace
  {
    constexpr std::uint32_t c_decimal_radix = 10;
    // Largest decimal chunk per big_unsigned step: 16-bit limbs keep limb x 10^4 + carry below 2^32 (decision 1b).
    constexpr std::size_t c_chunk_digits = 4;
    constexpr std::uint32_t c_chunk_radix = 10000;
    // 10^4 < 2^14, so a power more than 14 bits below a value stays at or below it after one chunk step.
    constexpr std::size_t c_chunk_bits = 14;
    constexpr std::uint32_t c_hexadecimal_radix = 16;
    constexpr std::array<std::uint32_t, 6> c_hexadecimal_letter_values{10U, 11U, 12U, 13U, 14U, 15U};
    constexpr std::size_t c_hexadecimal_prefix_bytes = 2;

    // The size bound is charged as one constant unit (ADR 0003, decision 1c).
    constexpr std::size_t c_bound_work_units = 1;

    // The first power of ten above a value is below 16 times it, so it needs at most 4 more bits.
    constexpr std::size_t c_power_headroom_bits = 4;

    // Leaves headroom so exponent sums and differences cannot overflow ptrdiff_t.
    constexpr auto c_max_exponent = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max() / 4);

    /**
     * @brief Describe a finite nonzero value as digits times an exact power of ten.
     * @par Requirements
     * P038; J5-036.
     * @note The digits are accumulated separately in a big_unsigned.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
    // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
    // implicit members that are never odr-used are not in its database.
    // codeql::misra_deviation_next_line(DEV-020.58)
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
     * @par ID SWU-VALUE-081
     */
    [[nodiscard]] std::uint32_t hexadecimal_digit_value(char digit) noexcept
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Classifies the byte with a range check on ASCII digits or letters; the result is
      // only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so
      // it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.37)
      if ((digit >= '0') && (digit <= '9'))
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares
        // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character
        // set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.36)
        return static_cast<std::uint32_t>(digit - '0');
      }

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Classifies the byte with a range check on ASCII digits or letters; the result is
      // only used to decide what the byte is. The parser compares UTF-8 input bytes with character literals, so
      // it relies on an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.35)
      if ((digit >= 'a') && (digit <= 'f'))
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares
        // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character
        // set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.34)
        return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'a')];
      }

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a value
      // 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares UTF-8
      // input bytes with character literals, so it relies on an ASCII-compatible execution character set, where
      // these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.33)
      return c_hexadecimal_letter_values[static_cast<std::size_t>(digit - 'A')];
    }

    /**
     * @brief Limbs to reserve for one operand, including its possible scaling.
     * @par Requirements
     * P038; J5-036; J5-046.
     * @param own Analysis of the operand.
     * @param other Analysis of the other operand.
     * @return Limb count or resource_limit (conversion_size) on overflow.
     * @note Scaling happens only for equal leading exponents; the power of ten then has
     * at most as many digits as the other operand's significand (ADR 0003, decision 1).
     * @par ID SWU-VALUE-082
     */
    [[nodiscard]] result<std::size_t> operand_limbs(const detail::number_analysis& own,
                                                    const detail::number_analysis& other) noexcept
    {
      using limbs_result = result<std::size_t>;

      // Hexadecimal digits carry 4 bits each; decimal digits use the 10/3 bound.
      const auto own_bits =
        (own.kind == number_kind::hexadecimal)
          ? checked_multiply(own.digit_count, static_cast<std::size_t>(4U), resource_limit_reason::conversion_size)
          : detail::decimal_digit_bits(own.digit_count);

      if (!own_bits.has_value())
      {
        return limbs_result::failure(*own_bits.error_if());
      }

      const auto scale_bits = detail::decimal_digit_bits(other.digit_count);

      if (!scale_bits.has_value())
      {
        return limbs_result::failure(*scale_bits.error_if());
      }

      const auto total_bits =
        checked_add(*own_bits.value_if(), *scale_bits.value_if(), resource_limit_reason::conversion_size);

      if (!total_bits.has_value())
      {
        return limbs_result::failure(*total_bits.error_if());
      }

      return detail::limbs_for_bits(*total_bits.value_if());
    }

    /**
     * @brief Return 10^digits for a decimal chunk of one to four digits.
     * @par Requirements
     * P038; J5-037.
     * @param digits Chunk length from 1 to c_chunk_digits.
     * @par ID SWU-VALUE-083
     */
    [[nodiscard]] std::uint32_t chunk_power(std::size_t digits) noexcept
    {
      std::uint32_t power = 1;

      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
      // bound that the body does not modify, and changed only in the increment expression. The counter is
      // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
      // codeql::misra_deviation_next_line(DEV-017.20)
      for (std::size_t index{}; index < digits; ++index)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: digits is at most c_chunk_digits (4), so power is at most 10^4.
        // codeql::misra_deviation_next_line(DEV-004.90)
        power *= c_decimal_radix;
      }

      return power;
    }

    /**
     * @brief Multiply an exact integer by 10^count in steps of up to 10^4.
     * @par Requirements
     * P038; J5-037; J5-048.
     * @param value Integer reserved for the scaled result.
     * @param count Decimal exponent to apply.
     * @param work Shared comparison work budget.
     * @return Success or the first failure; earlier steps are not undone.
     * @note Uses ceil(count / 4) multiplications instead of count (ADR 0003, decision 1b).
     * @par ID SWU-VALUE-084
     */
    [[nodiscard]] status_result scale_by_power_of_ten(detail::big_unsigned& value, std::size_t count,
                                                      work_budget& work) noexcept
    {
      while (count != 0)
      {
        const std::size_t step = (count < c_chunk_digits) ? count : c_chunk_digits;
        const auto multiplied = value.append_digit(0, chunk_power(step), work);

        if (!multiplied.has_value())
        {
          return multiplied;
        }

        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: step is the smaller of count and c_chunk_digits, so it is at most count.
        // codeql::misra_deviation_next_line(DEV-004.89)
        count -= step;
      }

      return status_result::success();
    }

    /**
     * @brief Count the decimal digits of a nonzero exact integer.
     * @par Requirements
     * P038; J5-036; J5-048.
     * @param value Nonzero magnitude to measure.
     * @param provider Scratch storage for the growing power of ten.
     * @param work Shared comparison work budget.
     * @return Number of decimal digits or an explicit resource/storage failure.
     * @note Reserves 10^count once from the value’s bit length, then multiplies it until it exceeds value.
     * @par ID SWU-VALUE-085
     */
    [[nodiscard]] result<std::size_t> count_decimal_digits(const detail::big_unsigned& value,
                                                           const storage_callbacks& provider,
                                                           work_budget& work) noexcept
    {
      using counter_result = result<std::size_t>;

      const auto value_bits = value.bit_length();

      if (!value_bits.has_value())
      {
        return counter_result::failure(*value_bits.error_if());
      }

      const auto power_bits =
        checked_add(*value_bits.value_if(), c_power_headroom_bits, resource_limit_reason::conversion_size);

      if (!power_bits.has_value())
      {
        return counter_result::failure(*power_bits.error_if());
      }

      const auto power_limbs = detail::limbs_for_bits(*power_bits.value_if());

      if (!power_limbs.has_value())
      {
        return counter_result::failure(*power_limbs.error_if());
      }

      detail::big_unsigned power{provider};
      const auto power_reserved = power.reserve(*power_limbs.value_if());

      if (!power_reserved.has_value())
      {
        return counter_result::failure(*power_reserved.error_if());
      }

      const auto one = power.append_digit(1, c_decimal_radix, work);

      if (!one.has_value())
      {
        return counter_result::failure(*one.error_if());
      }

      std::size_t count{};

      // While power is more than 14 bits shorter than value, power x 10^4 cannot pass value, so
      // whole chunks need no comparison; bit lengths are read in constant time (decision 1b).
      while (true)
      {
        const auto power_bits_now = power.bit_length();

        if (!power_bits_now.has_value())
        {
          return counter_result::failure(*power_bits_now.error_if());
        }

        if ((*power_bits_now.value_if() + c_chunk_bits) >= *value_bits.value_if())
        {
          break;
        }

        const auto chunked = power.append_digit(0, c_chunk_radix, work);

        if (!chunked.has_value())
        {
          return counter_result::failure(*chunked.error_if());
        }

        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: count is the number of decimal digits found so far, which is bounded by the bit
        // length of the value, so it cannot wrap.
        // codeql::misra_deviation_next_line(DEV-004.88)
        count += c_chunk_digits;
      }

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

        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: count is the number of decimal digits found so far, which is bounded by the bit
        // length of the value, so it cannot wrap.
        // codeql::misra_deviation_next_line(DEV-004.87)
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
     * @note Exponents above c_max_exponent return resource_limit (conversion_size) instead of saturating,
     * because saturation would make different values compare equal.
     * @par ID SWU-VALUE-086
     */
    [[nodiscard]] result<decimal_form> read_decimal_form(input_view spelling, number_kind kind,
                                                         detail::big_unsigned& digits,
                                                         const storage_callbacks& provider, work_budget& work) noexcept
    {
      using form_result = result<decimal_form>;

      // Digit counts below are bounded by the spelling, so bound it like the exponent.
      if (spelling.size > c_max_exponent)
      {
        return form_result::failure(error::limit_without_location(resource_limit_reason::conversion_size));
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

        while (position < spelling.size)
        {
          const auto appended =
            digits.append_digit(hexadecimal_digit_value(spelling.data[position]), c_hexadecimal_radix, work);

          if (!appended.has_value())
          {
            return form_result::failure(*appended.error_if());
          }

          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
          // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.86)
          ++position;
        }

        // Hexadecimal digits do not reveal the decimal leading exponent, so count it exactly.
        const auto count = count_decimal_digits(digits, provider, work);

        if (!count.has_value())
        {
          return form_result::failure(*count.error_if());
        }

        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: count is the number of decimal digits of a nonzero value, at least one and far
        // below PTRDIFF_MAX, so subtracting one cannot overflow.
        // codeql::misra_deviation_next_line(DEV-005.19)
        form.leading_exponent = static_cast<std::ptrdiff_t>(*count.value_if()) - 1;
        return form_result::success(form);
      }

      bool after_point{};
      bool seen_nonzero{};
      std::size_t fractional_digits{};
      std::size_t significant_digits{};

      // Up to four digits are collected and appended in one step (ADR 0003, decision 1b).
      std::uint32_t chunk{};
      std::size_t chunk_digits{};

      while (position < spelling.size)
      {
        const char current = spelling.data[position];
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
        // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
        // codeql::misra_deviation_next_line(DEV-004.85)
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

        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
        // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser compares
        // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character
        // set, where these characters are contiguous ranges.
        // codeql::misra_deviation_next_line(DEV-015.32)
        const auto digit = static_cast<std::uint32_t>(current - '0');
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: chunk holds at most three digits (999) before this step, so the result is at
        // most 9999.
        // codeql::misra_deviation_next_line(DEV-004.84)
        chunk = (chunk * c_decimal_radix) + digit;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: chunk_digits is reset after c_chunk_digits digits, so it is at most 4.
        // codeql::misra_deviation_next_line(DEV-004.83)
        ++chunk_digits;

        if (chunk_digits == c_chunk_digits)
        {
          const auto appended = digits.append_digit(chunk, c_chunk_radix, work);

          if (!appended.has_value())
          {
            return form_result::failure(*appended.error_if());
          }

          chunk = 0;
          chunk_digits = 0;
        }

        if (after_point)
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so the
          // count is at most its length and cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.82)
          ++fractional_digits;
        }

        // Leading zeros do not move the leading exponent; every digit after the first nonzero does.
        if (digit != 0)
        {
          seen_nonzero = true;
        }

        if (seen_nonzero)
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so the
          // count is at most its length and cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.81)
          ++significant_digits;
        }
      }

      // A shorter final chunk is appended with its own power of ten.
      if (chunk_digits != 0)
      {
        const auto appended = digits.append_digit(chunk, chunk_power(chunk_digits), work);

        if (!appended.has_value())
        {
          return form_result::failure(*appended.error_if());
        }
      }

      bool exponent_negative{};
      std::size_t exponent{};

      if (position < spelling.size)
      {
        if ((spelling.data[position] == '+') || (spelling.data[position] == '-'))
        {
          exponent_negative = (spelling.data[position] == '-');
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
          // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.80)
          ++position;
        }

        while (position < spelling.size)
        {
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
          // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser
          // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
          // character set, where these characters are contiguous ranges.
          // codeql::misra_deviation_next_line(DEV-015.31)
          const auto digit = static_cast<std::size_t>(spelling.data[position] - '0');

          // Comparison must stay exact, so an oversized exponent is a limit failure, not a saturation.
          if (exponent > ((c_max_exponent - digit) / c_decimal_radix))
          {
            return form_result::failure(error::limit_without_location(resource_limit_reason::conversion_size));
          }

          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: The check above returns a limit failure before exponent could exceed
          // c_max_exponent.
          // codeql::misra_deviation_next_line(DEV-004.79)
          exponent = (exponent * c_decimal_radix) + digit;
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
          // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.78)
          ++position;
        }
      }

      const auto signed_exponent = static_cast<std::ptrdiff_t>(exponent);

      form.scale =
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: |exponent| is at most c_max_exponent (PTRDIFF_MAX / 4, checked while parsing)
        // and fractional_digits is at most the spelling length, limited by max_number_bytes. The result stays
        // within std::ptrdiff_t while the spelling is shorter than 3/4 of PTRDIFF_MAX.
        // codeql::misra_deviation_next_line(DEV-005.18)
        (exponent_negative ? -signed_exponent : signed_exponent) - static_cast<std::ptrdiff_t>(fractional_digits);
      // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
      // MISRA justification: form.scale is bounded as above and significant_digits is at most the spelling
      // length, so the sum stays within std::ptrdiff_t for spellings shorter than 3/4 of PTRDIFF_MAX.
      // codeql::misra_deviation_next_line(DEV-005.17)
      form.leading_exponent = form.scale + static_cast<std::ptrdiff_t>(significant_digits) - 1;

      return form_result::success(form);
    }

    /**
     * @brief Translate a private magnitude ordering into the public ordering.
     * @par Requirements
     * P038; J5-036.
     * @par ID SWU-VALUE-087
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
     * @param left_info Analysis of left, providing its digit count.
     * @param right_info Analysis of right.
     * @note Releases all scratch before returning.
     * @par ID SWU-VALUE-088
     */
    [[nodiscard]] result<number_order> compare_finite_magnitudes(const owned_number& left, const owned_number& right,
                                                                 const detail::number_analysis& left_info,
                                                                 const detail::number_analysis& right_info,
                                                                 const storage_callbacks& scratch,
                                                                 const number_comparison_options& options,
                                                                 work_budget& work) noexcept
    {
      using order_result = result<number_order>;

      const auto bound_charged = work.charge(c_bound_work_units);

      if (!bound_charged.has_value())
      {
        return order_result::failure(*bound_charged.error_if());
      }

      const auto left_limbs = operand_limbs(left_info, right_info);

      if (!left_limbs.has_value())
      {
        return order_result::failure(*left_limbs.error_if());
      }

      const auto right_limbs = operand_limbs(right_info, left_info);

      if (!right_limbs.has_value())
      {
        return order_result::failure(*right_limbs.error_if());
      }

      // Declared before the integers so it outlives every allocation it accounts for.
      accounted_storage arena{scratch, options.max_scratch_bytes};
      const storage_callbacks provider = arena.callbacks();

      detail::big_unsigned left_digits{provider};
      detail::big_unsigned right_digits{provider};

      // One allocation each; the arena rejects a bound above max_scratch_bytes with resource_limit.
      const auto left_reserved = left_digits.reserve(*left_limbs.value_if());

      if (!left_reserved.has_value())
      {
        return order_result::failure(*left_reserved.error_if());
      }

      const auto right_reserved = right_digits.reserve(*right_limbs.value_if());

      if (!right_reserved.has_value())
      {
        return order_result::failure(*right_reserved.error_if());
      }

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
        // MISRA rule: RULE-4-1-3 cpp/misra/signed-integer-overflow
        // MISRA justification: Equal leading exponents bound the scale difference by the two digit counts,
        // which are at most the spelling lengths, so the difference is far below PTRDIFF_MAX.
        // codeql::misra_deviation_next_line(DEV-005.16)
        left_larger_scale ? (left_parts.scale - right_parts.scale) : (right_parts.scale - left_parts.scale);

      const auto multiplied = scale_by_power_of_ten(scaled, static_cast<std::size_t>(difference), work);

      if (!multiplied.has_value())
      {
        return order_result::failure(*multiplied.error_if());
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
     * @par ID SWU-VALUE-089
     */
    [[nodiscard]] std::int32_t sign_of(const detail::number_analysis& analysis) noexcept
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
     * @par ID SWU-VALUE-090
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
   * @par ID SWU-VALUE-091
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

    const std::int32_t left_sign = sign_of(left_info);
    const std::int32_t right_sign = sign_of(right_info);

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
      const auto finite = compare_finite_magnitudes(left, right, left_info, right_info, scratch, options, work);

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
