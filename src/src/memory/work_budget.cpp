// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/memory/work_budget.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  work_budget::work_budget(std::size_t max_work_units) noexcept : m_limit(max_work_units) {}

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

  std::size_t work_budget::consumed() const noexcept
  {
    return m_consumed;
  }

  std::size_t work_budget::remaining() const noexcept
  {
    return m_limit - m_consumed;
  }
} // namespace adaptive_json5
