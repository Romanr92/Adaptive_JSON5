#ifndef ADAPTIVE_JSON5_NUMERIC_CONVERSION_HPP_
#define ADAPTIVE_JSON5_NUMERIC_CONVERSION_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Configure bounded work for one numeric conversion.
   * @par Requirements
   * P038; J5-048; J5-049.
   * @note Defaults to resource_limits' work ceiling.
   * Zero permits no charged work. No unlimited sentinel is supported.
   * Construction performs no allocation.
   */
  struct [[nodiscard]] numeric_conversion_options
  {
      std::size_t max_work_units = resource_limits{}.max_work_units;
  };

  /**
   * @brief Convert an exact finite integer value to uintmax_t.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048; J5-052.
   * @param number Borrowed number whose spelling remains unchanged.
   * @param options Work ceiling for this conversion.
   * @return Exact unsigned value or an explicit error.
   * @note Accepts mathematically integral decimal and hexadecimal spellings.
   * Negative nonzero values return numeric_conversion with out_of_range.
   * Negative zero succeeds as unsigned zero.
   * Fractional values return non_integral; explicit infinity/NaN return non_finite.
   * Empty spelling returns wrong_type_or_access.
   * Budget exhaustion returns resource_limit.
   * Each call starts a fresh work budget; parsing budgets are not reused.
   * Performs no allocation, rounding, wrapping or locale-dependent operations.
   */
  [[nodiscard]] result<std::uintmax_t> to_unsigned_integer(const owned_number& number,
                                                           const numeric_conversion_options& options = {}) noexcept;

  /**
   * @brief Convert an exact finite integer value to intmax_t.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048; J5-052.
   * @param number Borrowed number whose spelling remains unchanged.
   * @param options Work ceiling for this conversion.
   * @return Exact signed value or an explicit error.
   * @note Accepts mathematically integral decimal and hexadecimal spellings.
   * Values outside the destination range return out_of_range.
   * Fractional values return non_integral; explicit infinity/NaN return non_finite.
   * Negative zero succeeds as signed zero.
   * Empty spelling returns wrong_type_or_access.
   * Budget exhaustion returns resource_limit.
   * Each call starts a fresh work budget.
   * Performs no allocation, rounding, wrapping or locale-dependent operations.
   */
  [[nodiscard]] result<std::intmax_t> to_signed_integer(const owned_number& number,
                                                        const numeric_conversion_options& options = {}) noexcept;

  /**
   * @brief Determine whether a number has an exact finite integer value.
   * @par Requirements
   * P038; J5-003; J5-036; J5-038; J5-048; J5-052.
   * @param number Borrowed number whose spelling remains unchanged.
   * @param options Work ceiling for this query.
   * @return True for finite mathematically integral values; false otherwise.
   * @note Integrality is independent of spelling and machine-integer range.
   * Infinity and NaN return successful false.
   * Empty spelling returns wrong_type_or_access.
   * Budget exhaustion returns resource_limit.
   * Each call starts a fresh work budget.
   * Performs no allocation, numeric conversion or locale-dependent operations.
   */
  [[nodiscard]] result<bool> is_integral_number(const owned_number& number,
                                                const numeric_conversion_options& options = {}) noexcept;

  /**
   * @brief Determine whether a document value is a mathematically integral number.
   * @par Requirements
   * P038; J5-003; J5-036; J5-039; J5-040.
   * @param value Borrowed document value to inspect.
   * @param options Work ceiling passed to numeric analysis.
   * @return Exact integrality or an explicit error.
   * @note Invalid views and non-number values return wrong_type_or_access.
   * Fractional numbers, infinity and NaN return successful false.
   * Booleans and numeric-looking strings are never coerced.
   * The owning document must remain alive and unmodified during this call.
   * Retains no borrowed pointers and performs no allocation.
   */
  [[nodiscard]] result<bool> is_integral_number(const value_view& value,
                                                const numeric_conversion_options& options = {}) noexcept;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_NUMERIC_CONVERSION_HPP_ */
