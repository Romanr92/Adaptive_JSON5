// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/parser/parser_attachment.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    status_result attach_parsed_value(document& destination, parser_frame& frame, value_index child, work_budget& work,
                                      std::size_t input_size) noexcept
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

      // Preserve source spelling coordinates before clearing the pending name.
      const source_span name_span{frame.name_offset, frame.name_length};

      // P034: append elements in source order; P035: retain every member with its decoded name.
      const auto attached = object_member ? destination.append_member(frame.container, std::move(*frame.pending_name),
                                                                      child, name_span, input_size)
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
