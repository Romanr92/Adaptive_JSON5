#ifndef MEMBER_LOOKUP_HPP_
#define MEMBER_LOOKUP_HPP_

#include "adaptive_json5/value/value_view.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Distinguish absent, unique and ambiguous decoded-name matches.
   * @par Requirements
   * P030; J5-031; J5-039.
   */
  enum class member_lookup_state
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
  struct member_lookup
  {
      member_lookup_state state = member_lookup_state::absent;
      std::optional<value_view> value{};
  };

} // namespace adaptive_json5
#endif
