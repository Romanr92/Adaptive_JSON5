#ifndef ADAPTIVE_JSON5_FLOATING_CONVERSION_POLICY_HPP_
#define ADAPTIVE_JSON5_FLOATING_CONVERSION_POLICY_HPP_

namespace adaptive_json5
{
  /**
   * @brief Select handling of explicit infinity and NaN during floating conversion.
   * @par Requirements
   * P038; J5-037; J5-038.
   * @par Policies
   * - reject: Reject explicit infinity and NaN with a non_finite conversion reason.
   * - allow: Permit explicit infinity and NaN when supported by the destination.
   * @note Unsupported special values still return a non_finite failure.
   * This policy does not permit finite conversion overflow.
   * The original numeric spelling remains unchanged.
   */
  enum class floating_non_finite_policy
  {
    reject,
    allow
  };

  /**
   * @brief Select handling of a nonzero finite value that rounds to zero.
   * @par Requirements
   * P038; J5-037; J5-038.
   * @par Policies
   * - reject: Return an explicit conversion failure instead of rounding to zero.
   * - allow_zero: Permit rounded zero while preserving the original value's sign.
   * @note Does not reject representable nonzero subnormal results.
   * Exact zero is accepted independently of this policy.
   */
  enum class floating_underflow_policy
  {
    reject,
    allow_zero
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_FLOATING_CONVERSION_POLICY_HPP_ */
