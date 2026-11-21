#ifndef ADAPTIVE_JSON5_VALUE_MEMBER_LOOKUP_HPP_
#define ADAPTIVE_JSON5_VALUE_MEMBER_LOOKUP_HPP_

#include "adaptive_json5/value/value_view.hpp"

#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Distinguish absent, unique and ambiguous decoded-name matches.
   * @par Requirements
   * P030; J5-031; J5-039.
   */
  enum class member_lookup_state : std::uint8_t
  {
    absent,
    unique,
    ambiguous
  };

  /**
   * @brief Describe the outcome of inspecting an object's matching members.
   * @par Requirements
   * P030; J5-031; J5-039; J5-040.
   * @note Lookup supplies a value only for the unique state.
   * The returned view borrows its originating document.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.04)
  struct [[nodiscard]] member_lookup
  {
      // MISRA rule: RULE-0-2-1 cpp/misra/unused-limited-visibility-variable
      // MISRA justification: Part of the span result returned to callers, so the result describes the complete
      // recognized range. Unit tests read the member to verify the range; production callers that only need
      // success ignore it.
      // codeql::misra_deviation_next_line(DEV-008.13)
      member_lookup_state state = member_lookup_state::absent;
      std::optional<value_view> value{};
  };

} // namespace adaptive_json5
#endif
