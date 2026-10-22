#ifndef ADAPTIVE_JSON5_DETAIL_FLOATING_FORMAT_HPP_
#define ADAPTIVE_JSON5_DETAIL_FLOATING_FORMAT_HPP_

#include <limits>
#include <type_traits>

namespace adaptive_json5::detail
{
  /**
   * @brief Check whether a destination matches the initial floating engine formats.
   * @par Requirements
   * P038; J5-037; J5-101.
   * @tparam T Destination type to inspect.
   * @return True for supported float/double numeric formats; false otherwise.
   * @note Checks numeric properties without assuming byte order or object layout.
   * Does not make unsupported formats a parser compilation failure.
   * Performs no allocation or runtime environment inspection.
   */
  template <typename T>
  [[nodiscard]] constexpr bool supports_floating_conversion() noexcept
  {
    constexpr int c_binary_radix = 2;

    constexpr int c_float_precision = 24;
    constexpr int c_float_min_exponent = -125;
    constexpr int c_float_max_exponent = 128;

    constexpr int c_double_precision = 53;
    constexpr int c_double_min_exponent = -1021;
    constexpr int c_double_max_exponent = 1024;

    if constexpr (std::is_same_v<T, float>)
    {
      return std::numeric_limits<T>::is_iec559 && (std::numeric_limits<T>::radix == c_binary_radix) &&
             (std::numeric_limits<T>::digits == c_float_precision) &&
             (std::numeric_limits<T>::min_exponent == c_float_min_exponent) &&
             (std::numeric_limits<T>::max_exponent == c_float_max_exponent) &&
             (std::numeric_limits<T>::has_denorm == std::denorm_present);
    }
    else if constexpr (std::is_same_v<T, double>)
    {
      return std::numeric_limits<T>::is_iec559 && (std::numeric_limits<T>::radix == c_binary_radix) &&
             (std::numeric_limits<T>::digits == c_double_precision) &&
             (std::numeric_limits<T>::min_exponent == c_double_min_exponent) &&
             (std::numeric_limits<T>::max_exponent == c_double_max_exponent) &&
             (std::numeric_limits<T>::has_denorm == std::denorm_present);
    }
    else
    {
      return false;
    }
  }
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_DETAIL_FLOATING_FORMAT_HPP_ */
