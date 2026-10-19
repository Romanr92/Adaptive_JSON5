// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/parser/parser_container_open.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/detail/parser/parser_stack.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    result<value_index> open_container(document& destination, parser_stack& frames, const punctuation_token& opening,
                                       work_budget& work) noexcept
    {
      bool array = false;
      parser_frame frame{};

      // Only a recognized opening token may begin a container.
      if (opening.byte_length != 1)
      {
        return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      switch (opening.kind)
      {
      case punctuation_kind::left_bracket:
        array = true;
        frame.state = parser_state::array_first_or_end;
        break;

      case punctuation_kind::left_brace:
        frame.state = parser_state::object_first_name_or_end;
        break;

      default:
        return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      frame.opening_offset = opening.byte_offset;

      // Allow for creating the record and relocating existing records.
      const auto units = checked_add(destination.size(), 1);

      if (!units.has_value())
      {
        return result<value_index>::failure(*units.error_if());
      }

      const auto charged = work.charge(*units.value_if());

      if (!charged.has_value())
      {
        return result<value_index>::failure(*charged.error_if());
      }

      // Obtain bounded frame storage before creating the document value.
      const auto pushed = frames.push(std::move(frame), work);

      if (!pushed.has_value())
      {
        return result<value_index>::failure(*pushed.error_if());
      }

      const auto created = array ? destination.make_array() : destination.make_object();

      if (!created.has_value())
      {
        // A successful push guarantees a provisional frame exists to remove.
        frames.pop();
        return result<value_index>::failure(*created.error_if());
      }

      // No stack mutation occurred since push, so top addresses our new frame.
      frames.top()->container = *created.value_if();

      return result<value_index>::success(*created.value_if());
    }
  } // namespace detail
} // namespace adaptive_json5
