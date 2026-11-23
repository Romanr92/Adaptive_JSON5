#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_WORK_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_WORK_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_budget.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/validation_options.hpp"

#include <cstddef>

namespace adaptive_json5::detail
{
  /**
   * @brief Step accounting and cancellation polling of one validation run.
   * @par Requirements
   * P043; J5-049; J5-065; ADR 0006 decisions 5 and 7.
   * @note One object per validation; it is not shared between threads. Holds copies of the
   * budget and callback, so the caller's options may go away after construction.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.6)
  class [[nodiscard]] schema_work
  {
    private:
      validation_budget m_budget;
      cancellation_callback m_cancellation;
      std::size_t m_work_steps{};
      std::size_t m_equality_steps{};
      std::size_t m_reference_steps{};
      std::size_t m_branch_evaluations{};
      std::size_t m_regex_steps{};

      /**
       * @brief Charge one work step, then one unit of a category counter.
       * @par Requirements
       * P045; P046; J5-065; ADR 0006 decision 5.
       * @return The step() failure, or resource_limit (reason) when the category cap is
       * exceeded. A failed category charge keeps the work step charged.
       * @par ID SWU-SCHEMA-123
       */
      [[nodiscard]] result<void, schema_error> charge(std::size_t& counter, std::size_t limit,
                                                      schema_limit_reason reason) noexcept;

    public:
      /**
       * @brief Start a run with no steps charged.
       * @par Requirements
       * P043; J5-065.
       * @par ID SWU-SCHEMA-016
       */
      schema_work(const validation_budget& budget, const cancellation_callback& cancellation) noexcept;

      /**
       * @brief Charge one evaluation step.
       * @par Requirements
       * P043; J5-049; J5-065; ADR 0006 decisions 5 and 7.
       * @return cancelled when the callback asks to stop (polled first, once per step);
       * resource_limit (max_work_steps) when the step would exceed the inclusive cap. A failed
       * step is not counted.
       * @par ID SWU-SCHEMA-017
       */
      [[nodiscard]] result<void, schema_error> step() noexcept;

      /**
       * @brief Charge one node visited by deep equality (const, enum, uniqueItems).
       * @par Requirements
       * P045; J5-061; J5-065.
       * @return As step(), or resource_limit (max_equality_steps).
       * @par ID SWU-SCHEMA-124
       */
      [[nodiscard]] result<void, schema_error> equality_step() noexcept;

      /**
       * @brief Charge one `$ref` traversal.
       * @par Requirements
       * P046; J5-056; J5-065.
       * @return As step(), or resource_limit (max_reference_steps).
       * @par ID SWU-SCHEMA-125
       */
      [[nodiscard]] result<void, schema_error> reference_step() noexcept;

      /**
       * @brief Charge one subschema evaluation under anyOf, oneOf, allOf, not or if.
       * @par Requirements
       * P046; J5-065.
       * @return As step(), or resource_limit (max_branch_evaluations).
       * @par ID SWU-SCHEMA-126
       */
      [[nodiscard]] result<void, schema_error> branch_step() noexcept;

      /**
       * @brief Charge units pattern-matcher thread steps (no work step, no cancellation poll).
       * @par Requirements
       * P049; J5-049; J5-065.
       * @return success, or resource_limit (max_regex_steps) when the inclusive cap would be exceeded.
       * @par ID SWU-SCHEMA-231
       */
      [[nodiscard]] result<void, schema_error> regex_steps(std::size_t units) noexcept;

      /**
       * @brief Return the number of steps charged so far.
       * @par Requirements
       * P043; J5-065.
       * @par ID SWU-SCHEMA-018
       */
      [[nodiscard]] std::size_t work_steps() const noexcept;

      /**
       * @brief Return the budget this run is checked against.
       * @par Requirements
       * P043; J5-065.
       * @par ID SWU-SCHEMA-019
       */
      [[nodiscard]] const validation_budget& budget() const noexcept;
  };
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_WORK_HPP_ */
