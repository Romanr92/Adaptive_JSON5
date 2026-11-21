#ifndef ADAPTIVE_JSON5_SCHEMA_SCHEMA_BUDGET_HPP_
#define ADAPTIVE_JSON5_SCHEMA_SCHEMA_BUDGET_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Inclusive per-validation evaluation limits.
   * @par Requirements
   * P043; J5-049; J5-065; ADR 0006 decision 5.
   * @note Exceeding a limit returns schema_error_code::resource_limit with the matching
   * schema_limit_reason. max_errors is the exception: reaching it in collect-all mode only
   * truncates the diagnostic list (ADR 0006 decision 12). The regex budget is added with P048.
   * Construction performs no allocation.
   */
  struct [[nodiscard]] validation_budget
  {
      std::size_t max_depth = resource_limits{}.max_depth; // Nested instance/schema evaluation depth.
      std::size_t max_reference_steps = 10'000;            // `$ref` traversals.
      std::size_t max_branch_evaluations = 100'000;        // anyOf, oneOf, allOf, not, if subschemas.
      std::size_t max_equality_steps = 100'000;            // Nodes visited by const, enum, uniqueItems.
      std::size_t max_errors = 100;                        // Diagnostics kept in collect-all mode.
      std::size_t max_work_steps = 1'000'000;              // Overall cap on all evaluation steps.
  };

  /**
   * @brief Inclusive compilation limits.
   * @par Requirements
   * P043; J5-047; J5-049; ADR 0006 decision 5.
   * @note Exceeding a limit returns schema_error_code::resource_limit with the matching
   * schema_limit_reason. Compilation has no cancellation callback.
   */
  struct [[nodiscard]] schema_compile_limits
  {
      std::size_t max_schema_nodes = 100'000;    // Compiled nodes across all registered resources.
      std::size_t max_compile_steps = 1'000'000; // Keyword checks, `$ref` resolution, cycle detection.
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_SCHEMA_BUDGET_HPP_ */
