#ifndef ADAPTIVE_JSON5_NUMERIC_CONVERSION_REASON_HPP_
#define ADAPTIVE_JSON5_NUMERIC_CONVERSION_REASON_HPP_

namespace adaptive_json5
{
  /**
   * @brief Distinguish why a requested numeric conversion cannot succeed.
   * @par Requirements
   * P038; J5-035; J5-037; J5-038.
   * @par Reasons
   * - non_integral: A finite value has a nonzero mathematical fractional part.
   * - out_of_range: The value cannot fit the destination under its conversion rules.
   * - non_finite: An explicit infinity or NaN is unsupported by the conversion.
   * - underflow: A nonzero finite value rounds to zero and policy rejects it.
   * - unsupported_destination: The destination's numeric format is not supported.
   * @note Integer conversion never rounds, truncates, wraps or clamps.
   * Finite floating overflow is distinct from an explicit non-finite token.
   * Resource and allocation failures retain their separate error categories.
   */
  enum class numeric_conversion_reason
  {
    non_integral,
    out_of_range,
    non_finite,
    underflow,
    unsupported_destination
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_NUMERIC_CONVERSION_REASON_HPP_ */
