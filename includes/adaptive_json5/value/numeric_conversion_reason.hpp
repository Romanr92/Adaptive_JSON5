#ifndef ADAPTIVE_JSON5_NUMERIC_CONVERSION_REASON_HPP_
#define ADAPTIVE_JSON5_NUMERIC_CONVERSION_REASON_HPP_

namespace adaptive_json5
{
  /**
   * @brief Distinguish why a requested numeric conversion cannot succeed.
   * @par Requirements
   * P038; J5-035; J5-038.
   * @note Describes numeric conversion failures, not syntax or access failures.
   * Resource and allocation failures retain their separate error categories.
   */
  enum class numeric_conversion_reason
  {
    /**
     * @brief The finite value has a nonzero mathematical fractional part.
     * @note Integer conversion must not round or truncate this value.
     */
    non_integral,

    /**
     * @brief The value lies outside the destination's permitted numeric range.
     * @note Integer conversion must not wrap or clamp this value.
     */
    out_of_range,

    /**
     * @brief An explicit infinity or NaN is unsupported by the conversion.
     * @note A finite spelling that overflows conversion is not an explicit
     * non-finite value.
     */
    non_finite
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_NUMERIC_CONVERSION_REASON_HPP_ */
