#include "value/number_analysis.hpp"

#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "common/checked_size.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    namespace
    {
      // Exceeds every supported decimal range, so a saturated exponent is certainly out of range.
      constexpr std::size_t c_exponent_margin = 2048;
    } // namespace

    /**
     * @brief Analyse a validated owned number without machine-number conversion.
     * @par Requirements
     * P038; J5-003; J5-036; J5-038; J5-048.
     * @param number Borrowed number to inspect without mutation.
     * @param work Mutable work budget charged before scanning.
     * @return result<number_analysis> containing exact properties or an error.
     * @note Allocates no storage.
     * @par ID SWU-VALUE-080
     */
    result<number_analysis> analyse_number(const owned_number& number, work_budget& work) noexcept
    {
      using analysis_result = result<number_analysis>;

      const auto spelling = number.spelling();

      // A moved-from number retains metadata but has no numeric value.
      if (spelling.size == 0)
      {
        return analysis_result::failure(error::access_failure(access_error_reason::invalid_argument));
      }

      const auto charged = work.charge(spelling.size);

      if (!charged.has_value())
      {
        return analysis_result::failure(*charged.error_if());
      }

      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.24)
      number_analysis analysis{};
      analysis.kind = number.kind();

      std::size_t position{};
      const char first = spelling.data[position];

      if ((first == '+') || (first == '-'))
      {
        analysis.negative = (first == '-');
        ++position;
      }

      if ((analysis.kind == number_kind::infinity) || (analysis.kind == number_kind::nan))
      {
        return analysis_result::success(analysis);
      }

      analysis.is_zero = true;

      if (analysis.kind == number_kind::hexadecimal)
      {
        // Validation already established the two-byte hexadecimal prefix.
        position += 2;

        while (position < spelling.size)
        {
          if (spelling.data[position] != '0')
          {
            analysis.is_zero = false;
          }

          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so the
          // count is at most its length and cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.110)
          ++analysis.digit_count;
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
          // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.109)
          ++position;
        }

        analysis.is_integral = true;
        return analysis_result::success(analysis);
      }

      bool after_point{};
      bool has_exponent{};
      std::size_t fractional_digits{};
      std::size_t trailing_zeros{};

      while (position < spelling.size)
      {
        const char current = spelling.data[position];
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
        // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
        // codeql::misra_deviation_next_line(DEV-004.108)
        ++position;

        if ((current == 'e') || (current == 'E'))
        {
          has_exponent = true;
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
          // codeql::misra_deviation_next_line(DEV-004.107)
          ++analysis.digit_count;

          if (after_point)
          {
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so
            // the count is at most its length and cannot wrap std::size_t.
            // codeql::misra_deviation_next_line(DEV-004.106)
            ++fractional_digits;
          }

          if (current == '0')
          {
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: Counts characters or sequences of the spelling or input being scanned, so
            // the count is at most its length and cannot wrap std::size_t.
            // codeql::misra_deviation_next_line(DEV-004.105)
            ++trailing_zeros;
          }
          else
          {
            analysis.is_zero = false;
            trailing_zeros = 0;
          }
        }
      }

      // Subtract the smaller count so no unsigned subtraction wraps.
      const bool needs_positive_exponent = fractional_digits > trailing_zeros;

      const std::size_t threshold =
        needs_positive_exponent ? (fractional_digits - trailing_zeros) : (trailing_zeros - fractional_digits);

      bool exponent_negative{};
      bool exponent_exceeds_threshold{};
      std::size_t exponent_magnitude{};

      const auto scale_limit = checked_add(spelling.size, c_exponent_margin, resource_limit_reason::conversion_size);

      if (!scale_limit.has_value())
      {
        return analysis_result::failure(*scale_limit.error_if());
      }

      // The size bound needs the full exponent, so it saturates separately from the integrality threshold.
      std::size_t scale_exponent{};

      if (has_exponent)
      {
        // Validated exponent syntax guarantees a byte after its introducer.
        const char first_exponent = spelling.data[position];

        if ((first_exponent == '+') || (first_exponent == '-'))
        {
          exponent_negative = (first_exponent == '-');
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
          // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.104)
          ++position;
        }

        while (position < spelling.size)
        {
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: Converts an already validated digit or hexadecimal letter to its value, or a
          // value 0-9 back to its digit. The C++ standard guarantees '0'-'9' are contiguous. The parser
          // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
          // character set, where these characters are contiguous ranges.
          // codeql::misra_deviation_next_line(DEV-015.40)
          const auto digit = static_cast<std::size_t>(spelling.data[position] - '0');
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: position indexes a byte of the spelling or input that was just read, so it is
          // below the size and the increment reaches at most the size; it cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.103)
          ++position;

          if (scale_exponent < *scale_limit.value_if())
          {
            // Saturate before multiplying so arbitrarily long exponents cannot wrap.
            scale_exponent = (scale_exponent > ((*scale_limit.value_if() - digit) / 10))
                               ? *scale_limit.value_if()
                               // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
                               // MISRA justification: The condition selects the saturated limit when the next
                               // value would exceed it, so this branch stays within the limit.
                               // codeql::misra_deviation_next_line(DEV-004.102)
                               : ((scale_exponent * 10) + digit);
          }

          if (!exponent_exceeds_threshold)
          {
            // Compare before multiplication to avoid overflowing size_t.
            if ((exponent_magnitude > (threshold / 10)) ||
                ((exponent_magnitude == (threshold / 10)) && (digit > (threshold % 10))))
            {
              exponent_exceeds_threshold = true;
            }
            else
            {
              // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
              // MISRA justification: The check above stops accumulating once the value would exceed threshold,
              // so this stays within threshold.
              // codeql::misra_deviation_next_line(DEV-004.101)
              exponent_magnitude = (exponent_magnitude * 10) + digit;
            }
          }
        }
      }

      // Combine the written exponent with the implied fractional scale without unsigned wrap.
      if (exponent_negative)
      {
        const auto magnitude = checked_add(scale_exponent, fractional_digits, resource_limit_reason::conversion_size);

        if (!magnitude.has_value())
        {
          return analysis_result::failure(*magnitude.error_if());
        }

        analysis.scale_negative = true;
        analysis.scale_magnitude = *magnitude.value_if();
      }
      else if (scale_exponent >= fractional_digits)
      {
        analysis.scale_magnitude = scale_exponent - fractional_digits;
      }
      else
      {
        analysis.scale_negative = true;
        analysis.scale_magnitude = fractional_digits - scale_exponent;
      }

      if (analysis.is_zero)
      {
        analysis.is_integral = true;
      }
      else if (exponent_negative)
      {
        analysis.is_integral = ((!needs_positive_exponent) && (!exponent_exceeds_threshold));
      }
      else
      {
        analysis.is_integral =
          ((!needs_positive_exponent) || (exponent_exceeds_threshold) || (exponent_magnitude == threshold));
      }

      return analysis_result::success(analysis);
    }
  } // namespace detail
} // namespace adaptive_json5
