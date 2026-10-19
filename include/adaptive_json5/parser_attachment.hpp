#ifndef PARSER_ATTACHMENT_HPP_
#define PARSER_ATTACHMENT_HPP_

#include "checked_size.hpp"
#include "document.hpp"
#include "error.hpp"
#include "parser_frame.hpp"
#include "result.hpp"
#include "value_index.hpp"
#include "work_budget.hpp"

#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Attach a completed value to its waiting parent frame.
     * @par Requirements
     * P033; J5-013; J5-030; J5-031; J5-048; J5-049; J5-051.
     * @param destination Document owning both frame.container and child.
     * @param frame Parent frame currently expecting a value.
     * @param child Detached completed value belonging to destination.
     * @param work Shared work budget for the parse operation.
     * @return Success or an explicit state, work or attachment failure.
     * @note Performs no allocation.
     * Failure preserves document relationships, frame state and pending name.
     * Successful work charges are not refunded after attachment failure.
     * Charges destination.size() plus one before relationship inspection.
     * Success transfers an object member's pending name and expects a separator.
     * Does not establish a root, change the cursor or reserve resources.
     * The caller handles node/member/string accounting and duplicate policy.
     */
    inline status_result attach_parsed_value(document& destination, parser_frame& frame, value_index child,
                                             work_budget& work) noexcept
    {
      bool object_member = false;

      switch (frame.state)
      {
      case parser_state::array_first_or_end:
      case parser_state::array_value_after_comma:
        // An array element must not accidentally discard a pending member name.
        if (frame.pending_name.has_value())
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }
        break;

      case parser_state::object_value:
        // An empty decoded name is valid; an absent name is an internal state error.
        if (!frame.pending_name.has_value())
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        object_member = true;
        break;

      default:
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // Document attachment may inspect the parent's complete ancestor chain.
      const auto units = checked_add(destination.size(), 1);

      if (!units.has_value())
      {
        return status_result::failure(*units.error_if());
      }

      const auto charged = work.charge(*units.value_if());

      if (!charged.has_value())
      {
        return status_result::failure(*charged.error_if());
      }

      const auto attached = object_member
                              ? destination.append_member(frame.container, std::move(*frame.pending_name), child)
                              : destination.append_element(frame.container, child);

      if (!attached.has_value())
      {
        return status_result::failure(*attached.error_if());
      }

      // Publish the grammar transition only after the relationship exists.
      if (object_member)
      {
        frame.pending_name.reset();
        frame.name_offset = 0;
        frame.name_length = 0;
        frame.state = parser_state::object_comma_or_end;
      }
      else
      {
        frame.state = parser_state::array_comma_or_end;
      }

      return status_result::success();
    }
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_ATTACHMENT_HPP_ */
