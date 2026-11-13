#ifndef CHECKED_SIZE_HPP_
#define CHECKED_SIZE_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <limits>

namespace adaptive_json5
{
  /**
   * @brief Add storage sizes without unsigned overflow.
   * @par Requirements
   * P003; J5-046; J5-051; J5-101; issue #125.
   * @param left First operand.
   * @param right Second operand.
   * @param reason Limit whose quantity the caller computes, e.g. owned_bytes for a block
   * size or work for work units.
   * @return Exact sum, or resource_limit carrying reason when std::size_t cannot
   * represent it.
   * @note Failure reporting requires no allocation or input location.
   * @par ID SWU-COMMON-001
   */
  [[nodiscard]] constexpr result<std::size_t> checked_add(std::size_t left, std::size_t right,
                                                          resource_limit_reason reason) noexcept
  {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();

    // Check remaining capacity before adding; unsigned addition could wrap
    if (right > maximum - left)
    {
      return result<std::size_t>::failure(error::limit_without_location(reason));
    }

    return result<std::size_t>::success(left + right);
  }

  /**
   * @brief Multiply storage sizes without unsigned overflow.
   * @par Requirements
   * P003; J5-046; J5-051; J5-101; issue #125.
   * @param left First operand.
   * @param right Second operand.
   * @param reason Limit whose quantity the caller computes, e.g. owned_bytes for a block
   * size or work for work units.
   * @return Exact product, or resource_limit carrying reason when std::size_t cannot
   * represent it.
   * @note Failure reporting requires no allocation or input location.
   * @par ID SWU-COMMON-002
   */
  [[nodiscard]] constexpr result<std::size_t> checked_multiply(std::size_t left, std::size_t right,
                                                               resource_limit_reason reason) noexcept
  {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();

    // Short-circuit division by zero, then check before multiplication can wrap.
    if ((left != 0) && (right > (maximum / left)))
    {
      return result<std::size_t>::failure(error::limit_without_location(reason));
    }

    return result<std::size_t>::success(left * right);
  }
} // namespace adaptive_json5
#endif /* CHECKED_SIZE_HPP_ */
