#ifndef ADAPTIVE_JSON5_FLOATING_CONVERSION_HPP_
#define ADAPTIVE_JSON5_FLOATING_CONVERSION_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Configure floating conversion work and special-value handling.
   * @par Requirements
   * P038; J5-037; J5-038; J5-048; J5-049; J5-052; J5-046 .
   * @note Defaults reject explicit non-finite values and nonzero-to-zero underflow.
   * Finite conversion uses round-to-nearest with ties to an even significand.
   * Finite overflow returns out_of_range regardless of the non-finite policy.
   * Representable nonzero subnormal results are permitted.
   * Zero permits no charged work; no unlimited sentinel is supported.
   * Construction performs no allocation.
   * Configuration alone does not implement or enforce conversion.
   * max_scratch_bytes is an inclusive ceiling on simultaneously outstanding
   * requested temporary bytes, including overlapping allocations during growth.
   * Zero permits no scratch allocation.
   */
  struct [[nodiscard]] floating_conversion_options
  {
      std::size_t max_work_units = resource_limits{}.max_work_units;
      std::size_t max_scratch_bytes = resource_limits{}.max_owned_bytes;

      floating_non_finite_policy non_finite = floating_non_finite_policy::reject;

      floating_underflow_policy underflow = floating_underflow_policy::reject;
  };

  /**
   * @brief Convert an exact numeric value directly to float.
   * @par Requirements
   * P038; J5-003; J5-037; J5-038; J5-046; J5-048; J5-052.
   * @param number Borrowed number whose spelling remains unchanged.
   * @param scratch Caller-owned fallible temporary-storage provider.
   * @param options Work, storage and special-value policies.
   * @return Rounded float value or an explicit error.
   * @note Rounds directly to float rather than converting through double.
   * Uses round-to-nearest with ties to an even significand.
   * Preserves negative zero.
   * Explicit infinity/NaN follow the non_finite policy and destination support.
   * Finite overflow returns out_of_range.
   * Nonzero-to-zero rounding follows the underflow policy.
   * Empty spelling returns wrong_type_or_access.
   * Work or scratch-limit exhaustion returns resource_limit.
   * Provider exhaustion returns allocation_failure.
   * All temporary allocations are released before returning.
   * Provider state must remain alive throughout the call.
   * No returned value borrows the provider or number.
   */
  [[nodiscard]] result<float> to_float(const owned_number& number, const storage_callbacks& scratch,
                                       const floating_conversion_options& options = {}) noexcept;

  /**
   * @brief Convert an exact numeric value directly to float.
   * @par Requirements
   * P038; J5-003; J5-037; J5-038; J5-046; J5-048; J5-052.
   * @param number Borrowed number whose spelling remains unchanged.
   * @param scratch Caller-owned fallible temporary-storage provider.
   * @param options Work, storage and special-value policies.
   * @return Rounded float value or an explicit error.
   * @note Rounds directly to float rather than converting through double.
   * Uses round-to-nearest with ties to an even significand.
   * Preserves negative zero.
   * Explicit infinity/NaN follow the non_finite policy and destination support.
   * Finite overflow returns out_of_range.
   * Nonzero-to-zero rounding follows the underflow policy.
   * Empty spelling returns wrong_type_or_access.
   * Work or scratch-limit exhaustion returns resource_limit.
   * Provider exhaustion returns allocation_failure.
   * All temporary allocations are released before returning.
   * Provider state must remain alive throughout the call.
   * No returned value borrows the provider or number.
   */
  [[nodiscard]] result<double> to_double(const owned_number& number, const storage_callbacks& scratch,
                                         const floating_conversion_options& options = {}) noexcept;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_FLOATING_CONVERSION_HPP_ */
