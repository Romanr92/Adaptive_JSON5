#include "memory/work_budget.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Begin an operation with no charged work.
   * @par Requirements
   * P031; J5-049.
   * @param max_work_units Inclusive maximum cumulative work units.
   * @par ID SWU-MEMORY-071
   */
  work_budget::work_budget(std::size_t max_work_units) noexcept : m_limit(max_work_units) {}

  /**
   * @brief Reserve work before executing its associated operation.
   * @par Requirements
   * P031; J5-048; J5-049.
   * @param units Work units to charge.
   * @return Success, or resource_limit (work) without a location and without changing
   * consumed work.
   * @note Exactly exhausting the remaining budget succeeds.
   * Zero units succeed even when the budget is exhausted.
   * @par ID SWU-MEMORY-072
   */
  status_result work_budget::charge(std::size_t units) noexcept
  {
    // Subtraction checks available work without overflowing the total.
    if ((m_consumed > m_limit) || (units > (m_limit - m_consumed)))
    {
      return status_result::failure(error::limit_without_location(resource_limit_reason::work));
    }

    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: The check above returns when units exceeds the remaining budget, so m_consumed stays
    // at most m_limit.
    // codeql::misra_deviation_next_line(DEV-004.130)
    m_consumed += units;
    return status_result::success();
  }

  /**
   * @brief Return the cumulative successfully charged work.
   * @par Requirements
   * P031; J5-049.
   * @par ID SWU-MEMORY-073
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
   * @par ID SWU-MEMORY-074
   */
  std::size_t work_budget::remaining() const noexcept
  {
    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: charge() keeps m_consumed at most m_limit, so the difference cannot wrap.
    // codeql::misra_deviation_next_line(DEV-004.129)
    return m_limit - m_consumed;
  }
} // namespace adaptive_json5
