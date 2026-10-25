#ifndef INTEGER_MAGNITUDE_HPP_
#define INTEGER_MAGNITUDE_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "memory/work_budget.hpp"

#include <cstdint>

namespace adaptive_json5::detail
{
  /**
   * @brief Retain an exact integer magnitude separately from its original sign.
   * @par Requirements
   * P038; J5-035; J5-038.
   * @note Negative zero retains negative set to true and magnitude equal to zero.
   * Contains no borrowed pointers and owns no allocated storage.
   */
  struct [[nodiscard]] integer_magnitude
  {
      std::uintmax_t magnitude{};
      bool negative{};
  };

  /**
   * @brief Append one numeric digit without exceeding an inclusive magnitude limit.
   * @par Requirements
   * P038; J5-003; J5-035.
   * @param current Previously accumulated unsigned magnitude.
   * @param digit Numeric digit value, not its character encoding.
   * @param radix Supported base: 10 or 16.
   * @param maximum Inclusive permitted magnitude.
   * @return Updated magnitude or an explicit error.
   * @note Invalid radix or digit returns wrong_type_or_access.
   * Exceeding maximum returns numeric_conversion with out_of_range.
   * Performs constant bounded work without allocation.
   * The enclosing conversion must account for each digit-processing step.
   */
  [[nodiscard]] result<std::uintmax_t> append_integer_digit(std::uintmax_t current, unsigned int digit,
                                                            unsigned int radix, std::uintmax_t maximum) noexcept;

  /**
   * @brief Read a finite integral number into a bounded unsigned magnitude.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048.
   * @param number Borrowed validated number whose spelling remains unchanged.
   * @param maximum Inclusive permitted magnitude, independent of sign.
   * @param work Shared work budget for analysis and magnitude construction.
   * @return Exact magnitude and original sign, or an explicit error.
   * @note Empty spelling returns wrong_type_or_access.
   * Explicit infinity or NaN returns numeric_conversion with non_finite.
   * Finite nonintegral values return numeric_conversion with non_integral.
   * Integral values exceeding maximum return numeric_conversion with out_of_range.
   * Work-budget failures retain resource_limit.
   * Accepts integral decimal fraction/exponent forms and hexadecimal spellings.
   * Preserves negative zero. Performs no allocation or floating-point conversion.
   * Reuses analyse_number and charges its analysis scan through work.
   * Before further spelling processing, charges one additional spelling.size.
   * Before each appended decimal zero, charges one additional work unit.
   * Additional spelling processing uses at most two forward scans.
   * Exponent handling must not overflow or expand an unbounded numeric value.
   */
  [[nodiscard]] result<integer_magnitude> read_integer_magnitude(const owned_number& number, std::uintmax_t maximum,
                                                                 work_budget& work) noexcept;
} // namespace adaptive_json5::detail

#endif /* INTEGER_MAGNITUDE_HPP_ */
