#ifndef ADAPTIVE_JSON5_DETAIL_FLOATING_SPECIAL_HPP_
#define ADAPTIVE_JSON5_DETAIL_FLOATING_SPECIAL_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "value/floating_format.hpp"
#include "value/number_analysis.hpp"

#include <limits>
#include <optional>
#include <type_traits>

namespace adaptive_json5::detail
{
  /**
   * @brief Handle exact zero and explicit non-finite floating values.
   * @par Requirements
   * P038; J5-003; J5-037; J5-038.
   * @tparam T Destination type: float or double.
   * @param analysis Properties obtained from successful number analysis.
   * @param options Explicit non-finite handling policy.
   * @return A destination value, successful nullopt for finite nonzero input,
   * or an explicit error.
   * @note Unsupported destination formats return unsupported_destination.
   * Exact zero preserves its sign independently of underflow policy.
   * Explicit infinity/NaN follow the non_finite policy.
   * NaN payload and sign preservation are not promised.
   * Performs constant bounded work without scanning or allocation.
   * The enclosing operation accounts for analysis before calling this helper.
   */
  template <typename T>
  [[nodiscard]] result<std::optional<T>> convert_special_floating(const number_analysis& analysis,
                                                                  const floating_conversion_options& options) noexcept
  {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "The destination must be float or double");

    using special_result = result<std::optional<T>>;

    if constexpr (!supports_floating_conversion<T>())
    {
      return special_result::failure(error::numeric_failure(numeric_conversion_reason::unsupported_destination));
    }
    else
    {
      if ((analysis.kind == number_kind::decimal) || (analysis.kind == number_kind::hexadecimal))
      {
        if (!analysis.is_zero)
        {
          return special_result::success(std::nullopt);
        }

        constexpr T c_zero{};

        const T zero = analysis.negative ? -c_zero : c_zero;

        return special_result::success(std::optional<T>{zero});
      }

      if (options.non_finite != floating_non_finite_policy::allow)
      {
        return special_result::failure(error::numeric_failure(numeric_conversion_reason::non_finite));
      }

      if (analysis.kind == number_kind::infinity)
      {
        if constexpr (std::numeric_limits<T>::has_infinity)
        {
          constexpr T c_infinity = std::numeric_limits<T>::infinity();

          const T infinity = analysis.negative ? -c_infinity : c_infinity;

          return special_result::success(std::optional<T>{infinity});
        }
        else
        {
          return special_result::failure(error::numeric_failure(numeric_conversion_reason::non_finite));
        }
      }

      if (analysis.kind == number_kind::nan)
      {
        if constexpr (std::numeric_limits<T>::has_quiet_NaN)
        {
          return special_result::success(std::optional<T>{std::numeric_limits<T>::quiet_NaN()});
        }
        else
        {
          return special_result::failure(error::numeric_failure(numeric_conversion_reason::non_finite));
        }
      }

      return special_result::failure(error::without_location(error_code::wrong_type_or_access));
    }
  }
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_DETAIL_FLOATING_SPECIAL_HPP_ */
