#ifndef CONTAINER_LINKS_HPP_
#define CONTAINER_LINKS_HPP_

#include "value/record_link.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Describe an ordered chain of direct child values.
   * @par Requirements
   * P030; J5-030; J5-031.
   * @note Owns no child records or allocations.
   * first/last are untagged private record links (ADR 0007 §4).
   * Empty chains have count zero and unset first/last links.
   * Nonempty chains have set first/last links.
   * Each child record supplies the next-sibling link.
   * The last child's next-sibling link is unset.
   * count includes direct children only, not nested descendants.
   * Internal document construction enforces bounds, membership and acyclic links.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.55)
  struct [[nodiscard]] container_links
  {
      detail::record_link first{};
      detail::record_link last{};
      std::size_t count{};
  };

  /**
   * @brief Describe an array's elements in source order.
   * @par Requirements
   * P030; J5-028; J5-030.
   * @note Repeated values remain separate element records.
   * Child ownership belongs to the document.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.54)
  struct [[nodiscard]] array_data
  {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.18)
      container_links elements{};
  };

  /**
   * @brief Describe an object's members in source order.
   * @par Requirements
   * P030; J5-028; J5-031.
   * @note Every member remains observable, including repeated decoded names.
   * Each direct child record will retain its owned member name.
   * These links do not perform lookup or apply duplicate-name policy.
   * Child ownership belongs to the document.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.53)
  struct [[nodiscard]] object_data
  {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.17)
      container_links members{};
  };
} // namespace adaptive_json5

#endif /* CONTAINER_LINKS_HPP_ */
