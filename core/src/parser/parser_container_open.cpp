#include "parser/parser_container_open.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "lexer/punctuation.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_frame.hpp"
#include "parser/parser_stack.hpp"
#include "parser/parser_work.hpp"
#include "value/document_access.hpp"

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
     * Record construction charges c_record_construction_work.
     * Frame pushing additionally follows parser_stack's charging rules.
     * @par ID SWU-PARSER-005
     */
    result<value_index> open_container(document& destination, parser_stack& frames, const punctuation_token& opening,
                                       work_budget& work) noexcept
    {
      bool array = false;
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.22)
      parser_frame frame{};

      // Only a recognized opening token may begin a container.
      if (opening.byte_length != 1)
      {
        return result<value_index>::failure(error::internal_fault());
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

      case punctuation_kind::right_brace:
      case punctuation_kind::right_bracket:
      case punctuation_kind::colon:
      case punctuation_kind::comma:
      default:
        return result<value_index>::failure(error::internal_fault());
      }

      frame.opening_offset = opening.byte_offset;

      // Charge record construction before any frame or record storage changes.
      const auto charged = work.charge(c_record_construction_work);

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

      const auto created =
        array ? detail::document_access::make_array(destination) : detail::document_access::make_object(destination);

      if (!created.has_value())
      {
        // A successful push guarantees a provisional frame exists to remove.
        // MISRA rule: RULE-7-0-1 cpp/misra/no-conversion-from-bool
        // MISRA justification: The cast to void explicitly discards pop()'s result, as Rule 0.1.2 permits; it
        // is not a numeric conversion. A frame is known to exist here, so the result carries no information.
        // codeql::misra_deviation_next_line(DEV-014.01)
        static_cast<void>(frames.pop());
        return result<value_index>::failure(*created.error_if());
      }

      // No stack mutation occurred since push, so top addresses our new frame.
      frames.top()->container = *created.value_if();

      return result<value_index>::success(*created.value_if());
    }
  } // namespace detail
} // namespace adaptive_json5
