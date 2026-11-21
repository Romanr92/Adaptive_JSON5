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
  class [[nodiscard]] schema_work
  {
    private:
      validation_budget m_budget;
      cancellation_callback m_cancellation;
      std::size_t m_work_steps{};

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
