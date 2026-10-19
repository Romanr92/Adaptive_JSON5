// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/memory/work_budget.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Begin an operation with no charged work.
   * @par Requirements
   * P031; J5-049.
   * @param max_work_units Inclusive maximum cumulative work units.
   */
  work_budget::work_budget(std::size_t max_work_units) noexcept : m_limit(max_work_units) {}

  /**
   * @brief Reserve work before executing its associated operation.
   * @par Requirements
   * P031; J5-048; J5-049.
   * @param units Work units to charge.
   * @return Success or resource_limit without changing consumed work.
   * @note Exactly exhausting the remaining budget succeeds.
   * Zero units succeed even when the budget is exhausted.
   */
  status_result work_budget::charge(std::size_t units) noexcept
  {
    // Subtraction checks available work without overflowing the total.
    if ((m_consumed > m_limit) || (units > (m_limit - m_consumed)))
    {
      return status_result::failure(error::without_location(error_code::resource_limit));
    }

    m_consumed += units;
    return status_result::success();
  }

  /**
   * @brief Return the cumulative successfully charged work.
   * @par Requirements
   * P031; J5-049.
   */
  std::size_t work_budget::consumed() const noexcept
  {
    return m_consumed;
  }

  /**
   * @brief Return the work units still available.
   * @par Requirements
   * P031; J5-049.
   * @note Successful charges preserve consumed <= limit.
   */
  std::size_t work_budget::remaining() const noexcept
  {
    return m_limit - m_consumed;
  }
} // namespace adaptive_json5
