#ifndef CHECKED_SIZE_HPP_
#define CHECKED_SIZE_HPP_

#include "error.hpp"
#include "result.hpp"

#include <cstddef>
#include <limits>

namespace adaptive_json5
{
  /**
   * @brief Add storage sizes without unsigned overflow.
   * @par Requirements
   * P003; J5-046; J5-051; J5-101.
   * @return Exact sum, or resource_limit when unrepresentable.
   * @note Failure reporting requires no allocation or input location.
   */
  constexpr result<std::size_t> checked_add(std::size_t left, std::size_t right) noexcept
  {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();

    // Check remaining capacity before adding; unsigned addition could wrap
    if (right > maximum - left)
    {
      return result<std::size_t>::failure(error::without_location(error_code::resource_limit));
    }

    return result<std::size_t>::success(left + right);
  }

  /**
   * @brief Multiply storage sizes without unsigned overflow.
   * @par Requirements
   * P003; J5-046; J5-051; J5-101.
   * @return Exact product, or resource_limit when unrepresentable.
   * @note Failure reporting requires no allocation or input location.
   */
  constexpr result<std::size_t> checked_multiply(std::size_t left, std::size_t right) noexcept
  {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();

    // Short-circuit division by zero, then check before multiplication can wrap.
    if ((left != 0) && (right > (maximum / left)))
    {
      return result<std::size_t>::failure(error::without_location(error_code::resource_limit));
    }

    return result<std::size_t>::success(left * right);
  }
} // namespace adaptive_json5
#endif /* CHECKED_SIZE_HPP_ */
