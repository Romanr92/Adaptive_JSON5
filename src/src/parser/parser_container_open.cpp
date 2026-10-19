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
    /**
     * @brief Prepare a frame and create one detached empty container.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-046; J5-048; J5-049.
     * @param destination Private document under construction.
     * @param frames Stack receiving the new open-container frame.
     * @param opening Recognized one-byte left bracket or left brace.
     * @param work Shared work budget for this parse operation.
     * @return New container index or an explicit failure.
     * @note The caller must reserve depth and value/member resources first.
     * On failure, the caller must release those reservations.
     * Depth remains reserved until closure or rollback.
     * Value/member reservations remain while their records exist.
     * Does not advance a cursor, attach the container or establish a root.
     * Failure preserves document records and existing frame contents.
     * Stack capacity and successfully charged work may change on failure.
     * Frame growth invalidates previously obtained frame pointers.
     * Record construction charges destination.size() plus one.
     * Frame pushing additionally follows parser_stack's charging rules.
     */
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
