#ifndef ADAPTIVE_JSON5_INTERNAL_VALUE_FLOATING_FORMAT_HPP_
#define ADAPTIVE_JSON5_INTERNAL_VALUE_FLOATING_FORMAT_HPP_

#include <cstdint>
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
   * @par ID SWU-VALUE-001
   */
  template <typename T>
  [[nodiscard]] constexpr bool supports_floating_conversion() noexcept
  {
    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.07)
    constexpr std::int32_t c_binary_radix = 2;

    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.06)
    constexpr std::int32_t c_float_precision = 24;
    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.05)
    constexpr std::int32_t c_float_min_exponent = -125;
    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.04)
    constexpr std::int32_t c_float_max_exponent = 128;

    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.03)
    constexpr std::int32_t c_double_precision = 53;
    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.02)
    constexpr std::int32_t c_double_min_exponent = -1021;
    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.01)
    constexpr std::int32_t c_double_max_exponent = 1024;

    if constexpr (std::is_same_v<T, float>)
    {
      // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
      // MISRA justification: Compile-time property of the template parameter or target type. The condition is
      // constant for one instantiation but differs between the supported types, so it is needed.
      // codeql::misra_deviation_next_line(DEV-006.10)
      return std::numeric_limits<T>::is_iec559 && (std::numeric_limits<T>::radix == c_binary_radix) &&
             // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
             // MISRA justification: Compile-time property of the template parameter or target type. The
             // condition is constant for one instantiation but differs between the supported types, so it is
             // needed.
             // codeql::misra_deviation_next_line(DEV-006.09)
             (std::numeric_limits<T>::digits == c_float_precision) &&
             // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
             // MISRA justification: Compile-time property of the template parameter or target type. The
             // condition is constant for one instantiation but differs between the supported types, so it is
             // needed.
             // codeql::misra_deviation_next_line(DEV-006.08)
             (std::numeric_limits<T>::min_exponent == c_float_min_exponent) &&
             // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
             // MISRA justification: Compile-time property of the template parameter or target type. The
             // condition is constant for one instantiation but differs between the supported types, so it is
             // needed.
             // codeql::misra_deviation_next_line(DEV-006.07)
             (std::numeric_limits<T>::max_exponent == c_float_max_exponent) &&
             // MISRA rule: RULE-10-2-3 cpp/misra/unscoped-enum-without-fixed-underlying-type-used
             // MISRA justification: Both operands have the standard library's own enum type
             // std::float_denorm_style (has_denorm and denorm_present), so no conversion to a different type
             // takes place.
             // codeql::misra_deviation_next_line(DEV-019.02)
             (std::numeric_limits<T>::has_denorm == std::denorm_present);
    }
    else if constexpr (std::is_same_v<T, double>)
    {
      // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
      // MISRA justification: Compile-time property of the template parameter or target type. The condition is
      // constant for one instantiation but differs between the supported types, so it is needed.
      // codeql::misra_deviation_next_line(DEV-006.06)
      return std::numeric_limits<T>::is_iec559 && (std::numeric_limits<T>::radix == c_binary_radix) &&
             // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
             // MISRA justification: Compile-time property of the template parameter or target type. The
             // condition is constant for one instantiation but differs between the supported types, so it is
             // needed.
             // codeql::misra_deviation_next_line(DEV-006.05)
             (std::numeric_limits<T>::digits == c_double_precision) &&
             // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
             // MISRA justification: Compile-time property of the template parameter or target type. The
             // condition is constant for one instantiation but differs between the supported types, so it is
             // needed.
             // codeql::misra_deviation_next_line(DEV-006.04)
             (std::numeric_limits<T>::min_exponent == c_double_min_exponent) &&
             // MISRA rule: RULE-0-0-2 cpp/misra/invariant-condition
             // MISRA justification: Compile-time property of the template parameter or target type. The
             // condition is constant for one instantiation but differs between the supported types, so it is
             // needed.
             // codeql::misra_deviation_next_line(DEV-006.03)
             (std::numeric_limits<T>::max_exponent == c_double_max_exponent) &&
             // MISRA rule: RULE-10-2-3 cpp/misra/unscoped-enum-without-fixed-underlying-type-used
             // MISRA justification: Both operands have the standard library's own enum type
             // std::float_denorm_style (has_denorm and denorm_present), so no conversion to a different type
             // takes place.
             // codeql::misra_deviation_next_line(DEV-019.01)
             (std::numeric_limits<T>::has_denorm == std::denorm_present);
    }
    else
    {
      return false;
    }
  }
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_INTERNAL_VALUE_FLOATING_FORMAT_HPP_ */
