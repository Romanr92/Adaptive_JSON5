#ifndef ADAPTIVE_JSON5_INTEGER_CONVERSION_HPP_
#define ADAPTIVE_JSON5_INTEGER_CONVERSION_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/numeric_conversion.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace adaptive_json5
{
  /**
   * @brief Convert an exact number to a selected integer destination.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048; J5-052.
   * @tparam T Unqualified integral destination type other than bool.
   * @param number Borrowed number whose spelling remains unchanged.
   * @param options Work ceiling passed to the widest-type conversion.
   * @return Exact destination value or an explicit error.
   * @note Checks destination bounds before casting.
   * Negative zero converts to zero.
   * Propagates widest-type conversion failures unchanged.
   * Destination-bound failures return numeric_conversion with out_of_range.
   * Performs no additional spelling scan or allocation.
   */
  template <typename T>
  [[nodiscard]] result<T> to_integer(const owned_number& number,
                                     const numeric_conversion_options& options = {}) noexcept
  {
    static_assert(std::is_integral_v<T>, "The destination must be an integral type");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>, "Boolean conversion is not an integer conversion");
    static_assert(std::is_same_v<T, std::remove_cv_t<T>>, "The destination must not be const or volatile");

    using conversion_result = result<T>;

    if constexpr (std::is_signed_v<T>)
    {
      const auto converted = to_signed_integer(number, options);

      if (!converted.has_value())
      {
        return conversion_result::failure(*converted.error_if());
      }

      const auto value = *converted.value_if();

      constexpr auto c_minimum = static_cast<std::intmax_t>(std::numeric_limits<T>::min());
      constexpr auto c_maximum = static_cast<std::intmax_t>(std::numeric_limits<T>::max());

      if ((value < c_minimum) || (value > c_maximum))
      {
        return conversion_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      return conversion_result::success(static_cast<T>(value));
    }
    else
    {
      const auto converted = to_unsigned_integer(number, options);

      if (!converted.has_value())
      {
        return conversion_result::failure(*converted.error_if());
      }

      const auto value = *converted.value_if();

      constexpr auto c_maximum = static_cast<std::uintmax_t>(std::numeric_limits<T>::max());

      if (value > c_maximum)
      {
        return conversion_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      return conversion_result::success(static_cast<T>(value));
    }
  }

  /**
   * @brief Convert a numeric document value to a selected integer destination.
   * @par Requirements
   * P038; J5-003; J5-035; J5-039; J5-040.
   * @tparam T Unqualified integral destination type other than bool.
   * @param value Borrowed document value whose numeric payload is inspected.
   * @param options Work ceiling passed to numeric conversion.
   * @return Exact destination value or an explicit error.
   * @note Invalid views and non-number values return wrong_type_or_access.
   * Booleans and numeric-looking strings are never coerced.
   * Numeric conversion failures propagate unchanged.
   * The owning document must remain alive and unmodified during this call.
   * Does not retain the view or payload pointer after returning.
   * Performs no additional spelling scan or allocation.
   */
  template <typename T>
  [[nodiscard]] result<T> to_integer(const value_view& value, const numeric_conversion_options& options = {}) noexcept
  {
    using conversion_result = result<T>;

    const auto number = value.as_number();

    if (!number.has_value())
    {
      return conversion_result::failure(*number.error_if());
    }

    return to_integer<T>(**number.value_if(), options);
  }
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_INTEGER_CONVERSION_HPP_ */
