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
   * truncates the diagnostic list (ADR 0006 decision 12). max_regex_steps bounds the linear-time
   * pattern matcher (P049).
   * Construction performs no allocation.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.22)
  struct [[nodiscard]] validation_budget
  {
      std::size_t max_depth = resource_limits{}.max_depth; // Nested instance/schema evaluation depth.
      std::size_t max_reference_steps = 10'000;            // `$ref` traversals.
      std::size_t max_branch_evaluations = 100'000;        // anyOf, oneOf, allOf, not, if subschemas.
      std::size_t max_equality_steps = 100'000;            // Nodes visited by const, enum, uniqueItems.
      std::size_t max_errors = 100;                        // Diagnostics kept in collect-all mode.
      std::size_t max_work_steps = 1'000'000;              // Overall cap on all evaluation steps.
      std::size_t max_regex_steps = 1'000'000;             // Pattern matcher thread steps (P049).
  };

  /**
   * @brief Inclusive compilation limits.
   * @par Requirements
   * P043; J5-047; J5-049; ADR 0006 decision 5.
   * @note Exceeding a limit returns schema_error_code::resource_limit with the matching
   * schema_limit_reason. Compilation has no cancellation callback.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.21)
  struct [[nodiscard]] schema_compile_limits
  {
      std::size_t max_schema_nodes = 100'000;    // Compiled nodes across all registered resources.
      std::size_t max_compile_steps = 1'000'000; // Keyword checks, `$ref` resolution, cycle detection.
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_SCHEMA_BUDGET_HPP_ */
