#include "parser/parser_attachment.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_frame.hpp"
#include "parser/parser_work.hpp"
#include "value/document_access.hpp"

#include <cstddef>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Attach a completed value and retain its pending member-name span.
     * @par Requirements
     * P033; P034; P035; P036; J5-013; J5-030; J5-031; J5-032;
     * J5-041; J5-048; J5-049; J5-051.
     * @param destination Document owning both frame.container and child.
     * @param frame Parent frame currently expecting a value.
     * @param child Detached completed value belonging to destination.
     * @param work Shared work budget for the parse operation.
     * @param input_size Original input size used to validate member-name spans.
     * @return Success or an explicit state, work, range or attachment failure.
     * @note Performs no allocation.
     * Failure preserves relationships, frame state, pending name and name offsets.
     * Successful work charges are not refunded after attachment failure.
     * Charges c_attachment_work before relationship inspection. The engine attaches a
     * container only when it closes, so frame.container is still detached and the
     * cycle check inspects that one record.
     * Success transfers an object's pending name and records its original span.
     * Does not establish a root, change the cursor or reserve resources.
     * The caller handles node/member/string accounting and duplicate policy.
     * The parser attaches without ancestor traversal; frame.container must be detached, which holds because containers
     * attach only when they close.
     */
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

      // The open parent is detached, so the ancestor walk inspects only that record.
      const auto charged = work.charge(c_attachment_work);

      if (!charged.has_value())
      {
        return status_result::failure(*charged.error_if());
      }

      // Preserve source spelling coordinates before clearing the pending name.
      const source_span name_span{frame.name_offset, frame.name_length};

      // P034: append elements in source order; P035: retain every member with its decoded name.
      // Open containers stay detached until they close, so the loop-free attach applies.
      const auto attached = object_member ? document_access::attach_parsed_member(destination, frame.container,
                                                                                  std::move(*frame.pending_name), child,
                                                                                  name_span, input_size)
                                          : document_access::attach_parsed_element(destination, frame.container, child);

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
