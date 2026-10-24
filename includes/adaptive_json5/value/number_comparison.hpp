#ifndef ADAPTIVE_JSON5_NUMBER_COMPARISON_HPP_
#define ADAPTIVE_JSON5_NUMBER_COMPARISON_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Describe the exact mathematical ordering of two numbers.
   * @par Requirements
   * P038; J5-036; J5-038.
   * @par Outcomes
   * - less: The left value is smaller.
   * - equal: Both values are mathematically equal, regardless of spelling.
   * - greater: The left value is larger.
   * - unordered: At least one operand is NaN.
   * @note Negative zero equals positive zero.
   * Infinities order beyond every finite value; equal infinities are equal.
   */
  enum class number_order
  {
    less,
    equal,
    greater,
    unordered
  };

  /**
   * @brief Configure work and scratch limits for one exact comparison.
   * @par Requirements
   * P038; J5-046; J5-048; J5-049.
   * @note Zero permits no charged work or scratch allocation; no unlimited sentinel is supported.
   * max_scratch_bytes is an inclusive ceiling on simultaneously outstanding temporary bytes.
   * Construction performs no allocation.
   */
  struct [[nodiscard]] number_comparison_options
  {
      std::size_t max_work_units = resource_limits{}.max_work_units;
      std::size_t max_scratch_bytes = resource_limits{}.max_owned_bytes;
  };

  /**
   * @brief Compare two numbers by exact mathematical value.
   * @par Requirements
   * P038; J5-003; J5-036; J5-038; J5-046; J5-048; J5-052.
   * @param left Borrowed left operand whose spelling remains unchanged.
   * @param right Borrowed right operand whose spelling remains unchanged.
   * @param scratch Caller-owned fallible temporary-storage provider.
   * @param options Work and scratch limits.
   * @return Exact ordering or an explicit error.
   * @note Never converts through floating point; spelling and radix do not affect the result.
   * Empty spelling returns wrong_type_or_access.
   * Exponent magnitudes above PTRDIFF_MAX / 4 return resource_limit (documented project limit).
   * Work or scratch-limit exhaustion returns resource_limit; provider exhaustion returns allocation_failure.
   * All temporary allocations are released before returning.
   */
  [[nodiscard]] result<number_order> compare_numbers(const owned_number& left, const owned_number& right,
                                                     const storage_callbacks& scratch,
                                                     const number_comparison_options& options = {}) noexcept;

} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_NUMBER_COMPARISON_HPP_ */
