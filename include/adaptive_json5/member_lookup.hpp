#ifndef MEMBER_LOOKUP_HPP_
#define MEMBER_LOOKUP_HPP_

#include "adaptive_json5/container_links.hpp"
#include "decoded_name_equal.hpp"
#include "decoded_string.hpp"
#include "error.hpp"
#include "result.hpp"
#include "value_index.hpp"
#include "value_iterator.hpp"
#include "value_record.hpp"
#include "value_storage.hpp"
#include "value_view.hpp"

#include <optional>
#include <variant>

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

  namespace detail
  {
    /**
     * @brief Find whether an object contains one unambiguous decoded name.
     * @par Requirements
     * P030; J5-031; J5-033; J5-039; J5-040.
     * @param storage Storage belonging to the inspected document.
     * @param object Object index belonging to storage.
     * @param name Exact decoded name to find.
     * @return Explicit lookup outcome or an access/comparison failure.
     * @note Rejects arrays and scalar values, including empty arrays.
     * Preserves every member and never selects among duplicate matches.
     * Allocates nothing and does not modify the document or supplied name.
     * Requires relationships maintained by validated document operations.
     * Work is linear in visited members plus examined name units.
     * P031 adds configurable work accounting.
     */
    inline result<member_lookup> lookup_member(const value_storage& storage, value_index object,
                                               const decoded_string& name) noexcept
    {
      const auto resolved = storage.at(object);
      if (!resolved.has_value())
      {
        return result<member_lookup>::failure(*resolved.error_if());
      }

      // An empty array must not be mistaken for an object with no matches.
      if (!std::holds_alternative<object_data>((*resolved.value_if())->payload))
      {
        return result<member_lookup>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      auto iterator = value_iterator::create(storage, object);

      if (!iterator.has_value())
      {
        return result<member_lookup>::failure(*iterator.error_if());
      }

      std::optional<value_view> match{};

      while (true)
      {
        const auto next = iterator.value_if()->next();

        if (!next.has_value())
        {
          return result<member_lookup>::failure(*next.error_if());
        }

        const auto& child = *next.value_if();

        if (!child.has_value())
        {
          break;
        }

        const auto member_name = child->member_name();

        if (!member_name.has_value())
        {
          return result<member_lookup>::failure(*member_name.error_if());
        }

        const auto equal = decoded_name_equal(**member_name.value_if(), name);

        if (!equal.has_value())
        {
          return result<member_lookup>::failure(*equal.error_if());
        }

        if (!*equal.value_if())
        {
          continue;
        }

        // A second match makes selecting either value ambiguous.
        if (match.has_value())
        {
          return result<member_lookup>::success(member_lookup{member_lookup_state::ambiguous, std::nullopt});
        }

        match.emplace(*child);
      }

      if (!match.has_value())
      {
        return result<member_lookup>::success(member_lookup{member_lookup_state::absent, std::nullopt});
      }

      return result<member_lookup>::success(member_lookup{member_lookup_state::unique, match});
    }
  } // namespace detail
} // namespace adaptive_json5

#endif /* MEMBER_LOOKUP_HPP_ */
