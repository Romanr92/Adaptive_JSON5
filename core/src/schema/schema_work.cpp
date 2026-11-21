#include "schema/schema_work.hpp"

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_budget.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/validation_options.hpp"

#include <cstddef>

namespace adaptive_json5::detail
{
  /**
   * @brief Start a run with no steps charged.
   * @par Requirements
   * P043; J5-065.
   * @par ID SWU-SCHEMA-016
   */
  schema_work::schema_work(const validation_budget& budget, const cancellation_callback& cancellation) noexcept
      : m_budget(budget), m_cancellation(cancellation)
  {
  }

  /**
   * @brief Charge one evaluation step.
   * @par Requirements
   * P043; J5-049; J5-065; ADR 0006 decisions 5 and 7.
   * @par ID SWU-SCHEMA-017
   */
  result<void, schema_error> schema_work::step() noexcept
  {
    if ((m_cancellation.poll != nullptr) && m_cancellation.poll(m_cancellation.context))
    {
      return result<void, schema_error>::failure(schema_error::of(schema_error_code::cancelled));
    }

    if (m_work_steps >= m_budget.max_work_steps)
    {
      return result<void, schema_error>::failure(schema_error::limit(schema_limit_reason::max_work_steps));
    }

    ++m_work_steps;
    return result<void, schema_error>::success();
  }

  /**
   * @brief Return the number of steps charged so far.
   * @par Requirements
   * P043; J5-065.
   * @par ID SWU-SCHEMA-018
   */
  std::size_t schema_work::work_steps() const noexcept
  {
    return m_work_steps;
  }

  /**
   * @brief Return the budget this run is checked against.
   * @par Requirements
   * P043; J5-065.
   * @par ID SWU-SCHEMA-019
   */
  const validation_budget& schema_work::budget() const noexcept
  {
    return m_budget;
  }
} // namespace adaptive_json5::detail
