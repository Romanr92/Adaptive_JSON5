#ifndef ADAPTIVE_JSON5_INTERNAL_MEMORY_WORK_BUDGET_HPP_
#define ADAPTIVE_JSON5_INTERNAL_MEMORY_WORK_BUDGET_HPP_

#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Bound cumulative charged work for one operation.
   * @par Requirements
   * P031; J5-047; J5-048; J5-049.
   * @note Charge before performing the corresponding work.
   * Failed charges leave accounting unchanged.
   * Successful charges are not refunded after later operation failures.
   * Zero permits only zero-cost charges; no unlimited sentinel is used.
   * Performs no allocation and requires no clock or operating-system service.
   * Operation-specific code must document its charging rules.
   * This object requires external synchronization if shared concurrently.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.37)
  class [[nodiscard]] work_budget
  {
    private:
      std::size_t m_limit;
      std::size_t m_consumed{};

    public:
      /**
       * @brief Begin an operation with no charged work.
       * @par Requirements
       * P031; J5-049.
       * @param max_work_units Inclusive maximum cumulative work units.
       * @par ID SWU-MEMORY-071
       */
      explicit work_budget(std::size_t max_work_units) noexcept;

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
      [[nodiscard]] status_result charge(std::size_t units) noexcept;

      /**
       * @brief Return the cumulative successfully charged work.
       * @par Requirements
       * P031; J5-049.
       * @par ID SWU-MEMORY-073
       */
      [[nodiscard]] std::size_t consumed() const noexcept;

      /**
       * @brief Return the work units still available.
       * @par Requirements
       * P031; J5-049.
       * @note Successful charges preserve consumed <= limit.
       * @par ID SWU-MEMORY-074
       */
      [[nodiscard]] std::size_t remaining() const noexcept;
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_INTERNAL_MEMORY_WORK_BUDGET_HPP_ */
