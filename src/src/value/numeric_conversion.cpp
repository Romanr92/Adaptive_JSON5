#include "adaptive_json5/value/numeric_conversion.hpp"

#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/value/integer_magnitude.hpp"
#include "adaptive_json5/detail/value/number_analysis.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstdint>
#include <limits>

namespace adaptive_json5
{
  /**
   * @brief Convert an exact numeric value to the widest unsigned integer type.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048; J5-052.
   * @param number Borrowed validated number.
   * @param options Work ceiling for this operation.
   * @return Exact unsigned value or an explicit error.
   * @note Negative zero is accepted. Negative nonzero values are rejected.
   * Performs no allocation and never modifies the stored spelling.
   */
  result<std::uintmax_t> to_unsigned_integer(const owned_number& number,
                                             const numeric_conversion_options& options) noexcept
  {
    using conversion_result = result<std::uintmax_t>;

    constexpr auto c_maximum_magnitude = std::numeric_limits<std::uintmax_t>::max();

    work_budget work{options.max_work_units};

    const auto converted = detail::read_integer_magnitude(number, c_maximum_magnitude, work);

    if (!converted.has_value())
    {
      return conversion_result::failure(*converted.error_if());
    }

    const auto& integer = *converted.value_if();

    // Unsigned zero can represent negative zero's value without wrapping.
    if (integer.negative && (integer.magnitude != 0))
    {
      return conversion_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
    }

    return conversion_result::success(integer.magnitude);
  }

  /**
   * @brief Convert an exact number while safely handling the signed minimum.
   * @par Requirements
   * P038; J5-003; J5-035; J5-036; J5-038; J5-048; J5-052.
   * @param number Borrowed validated number.
   * @param options Work ceiling for this operation.
   * @return Exact signed value or an explicit error.
   * @note Checks magnitude before signed conversion or negation.
   * Performs no allocation and preserves the stored spelling.
   */
  result<std::intmax_t> to_signed_integer(const owned_number& number,
                                          const numeric_conversion_options& options) noexcept
  {
    using conversion_result = result<std::intmax_t>;

    constexpr auto c_signed_minimum = std::numeric_limits<std::intmax_t>::min();
    constexpr auto c_signed_maximum = std::numeric_limits<std::intmax_t>::max();

    constexpr std::intmax_t c_one = 1;
    constexpr std::uintmax_t c_unsigned_one = 1;

    constexpr auto c_positive_limit = static_cast<std::uintmax_t>(c_signed_maximum);

    // Move inside the signed range before negation; finish in unsigned arithmetic.
    constexpr auto c_negative_limit = static_cast<std::uintmax_t>(-(c_signed_minimum + c_one)) + c_unsigned_one;

    work_budget work{options.max_work_units};

    const auto converted = detail::read_integer_magnitude(number, c_negative_limit, work);

    if (!converted.has_value())
    {
      return conversion_result::failure(*converted.error_if());
    }

    const auto& integer = *converted.value_if();

    if (!integer.negative)
    {
      if (integer.magnitude > c_positive_limit)
      {
        return conversion_result::failure(error::numeric_failure(numeric_conversion_reason::out_of_range));
      }

      return conversion_result::success(static_cast<std::intmax_t>(integer.magnitude));
    }

    // Return the minimum directly rather than converting its unsigned magnitude.
    if (integer.magnitude == c_negative_limit)
    {
      return conversion_result::success(c_signed_minimum);
    }

    const auto positive_magnitude = static_cast<std::intmax_t>(integer.magnitude);

    return conversion_result::success(-positive_magnitude);
  }

  /**
   * @brief Query mathematical integrality through bounded exact analysis.
   * @par Requirements
   * P038; J5-003; J5-036; J5-038; J5-048; J5-052.
   * @param number Borrowed validated number.
   * @param options Work ceiling for this query.
   * @return Exact integrality or an explicit operational/access failure.
   * @note No conversion to an integer or floating-point destination occurs.
   */
  result<bool> is_integral_number(const owned_number& number, const numeric_conversion_options& options) noexcept
  {
    using query_result = result<bool>;

    work_budget work{options.max_work_units};

    const auto analysed = detail::analyse_number(number, work);

    if (!analysed.has_value())
    {
      return query_result::failure(*analysed.error_if());
    }

    return query_result::success(analysed.value_if()->is_integral);
  }

  /**
   * @brief Query exact integrality after checked numeric document access.
   * @par Requirements
   * P038; J5-003; J5-036; J5-039; J5-040.
   * @param value Borrowed document value.
   * @param options Work ceiling passed to the owned-number query.
   * @return Exact integrality or an explicit access/analysis failure.
   * @note Delegation starts one numeric-analysis budget and retains no pointers.
   */
  result<bool> is_integral_number(const value_view& value, const numeric_conversion_options& options) noexcept
  {
    using query_result = result<bool>;

    const auto number = value.as_number();

    if (!number.has_value())
    {
      return query_result::failure(*number.error_if());
    }

    return is_integral_number(**number.value_if(), options);
  }
} // namespace adaptive_json5
