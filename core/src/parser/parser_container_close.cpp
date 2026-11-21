#include "parser/parser_container_close.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "common/internal_link.hpp"
#include "lexer/punctuation.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_frame.hpp"
#include "parser/parser_stack.hpp"
#include "value/document_access.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Check that a closing token may close a container in the given grammar state.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-041; J5-043.
     * @param state Grammar state of the innermost open container.
     * @param closing Recognized right bracket or right brace.
     * @param mode Selected syntax mode.
     * @return Success, or a syntax error located at the closing token.
     * @note Performs no allocation and charges no work.
     * @par ID SWU-PARSER-003
     */
    status_result check_container_close(parser_state state, const punctuation_token& closing, syntax_mode mode) noexcept
    {
      bool array = false;
      bool trailing_comma = false;

      switch (state)
      {
      case parser_state::array_first_or_end:
      case parser_state::array_comma_or_end:
        array = true;
        break;

      case parser_state::array_value_after_comma:
        array = true;
        trailing_comma = true;
        break;

      case parser_state::object_first_name_or_end:
      case parser_state::object_comma_or_end:
        break;

      case parser_state::object_name_after_comma:
        trailing_comma = true;
        break;

      case parser_state::object_colon:
      case parser_state::object_value:
      default:
      {
        // A member awaiting its colon or value cannot close successfully.
        return status_result::failure(error::syntax_at(closing.byte_offset, (state == parser_state::object_colon)
                                                                              ? syntax_error_reason::missing_colon
                                                                              : syntax_error_reason::missing_value));
      }
      }

      const auto expected = array ? punctuation_kind::right_bracket : punctuation_kind::right_brace;

      if (closing.kind != expected)
      {
        return status_result::failure(
          error::syntax_at(closing.byte_offset, syntax_error_reason::unexpected_closing_bracket));
      }

      // P034/P035: only JSON5 permits closing after one completed item and its comma.
      if (trailing_comma && (mode != syntax_mode::json5))
      {
        return status_result::failure(
          error::syntax_at(closing.byte_offset, syntax_error_reason::not_allowed_in_strict_json));
      }

      return status_result::success();
    }

    /**
     * @brief Complete and remove the innermost open-container frame.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-041; J5-048; J5-049.
     * @param destination Document owning the frame's container.
     * @param frames Stack of valid open-container frames.
     * @param closing Recognized one-byte right bracket or right brace.
     * @param mode Selected syntax mode.
     * @param input_size Original input size in bytes.
     * @param work Shared work budget for this parse operation.
     * @return Completed container index or an explicit failure.
     * @note The caller must maintain valid frame/container correspondence.
     * Failure preserves the stack and existing source-span metadata.
     * Successful closure records the span and removes exactly one frame.
     * Does not advance a cursor, attach the value or establish a root.
     * The caller releases depth after success; value/member reservations remain.
     * Performs no allocation and charges one work unit after validation.
     * Successful work charges are not refunded after later failure.
     * @note A frame container the document cannot resolve reports internal_error (issue #125).
     * @par ID SWU-PARSER-004
     */
    result<value_index> close_container(document& destination, parser_stack& frames, const punctuation_token& closing,
                                        syntax_mode mode, std::size_t input_size, work_budget& work) noexcept
    {
      auto* frame = frames.top();

      if (frame == nullptr)
      {
        return result<value_index>::failure(error::internal_fault());
      }

      if ((closing.byte_length != 1) ||
          ((closing.kind != punctuation_kind::right_bracket) && (closing.kind != punctuation_kind::right_brace)))
      {
        return result<value_index>::failure(error::internal_fault());
      }

      // Validate offsets before adding the closing byte or subtracting the opening.
      if ((closing.byte_offset >= input_size) || (frame->opening_offset >= closing.byte_offset))
      {
        return result<value_index>::failure(error::internal_fault());
      }

      // The pre-pass uses the same check, so both passes report the same close errors.
      const auto closable = check_container_close(frame->state, closing, mode);

      if (!closable.has_value())
      {
        return result<value_index>::failure(*closable.error_if());
      }

      // Every closable object has already transferred its pending name.
      if (frame->pending_name.has_value())
      {
        return result<value_index>::failure(error::internal_fault());
      }

      const auto charged = work.charge(1);

      if (!charged.has_value())
      {
        return result<value_index>::failure(*charged.error_if());
      }

      const value_index completed = frame->container;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: closing.byte_offset is the offset of a byte inside the input, so adding one is at
      // most the input size.
      // codeql::misra_deviation_next_line(DEV-004.112)
      const std::size_t end_offset = closing.byte_offset + 1;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The container opened before its closing byte, so end_offset is above
      // opening_offset.
      // codeql::misra_deviation_next_line(DEV-004.111)
      const source_span span{frame->opening_offset, end_offset - frame->opening_offset};

      const auto recorded = detail::document_access::set_source_span(destination, completed, span, input_size);

      if (!recorded.has_value())
      {
        return result<value_index>::failure(as_internal_fault(*recorded.error_if()));
      }

      // No fallible operation remains after publishing the completed span.
      // MISRA rule: RULE-7-0-1 cpp/misra/no-conversion-from-bool
      // MISRA justification: The cast to void explicitly discards pop()'s result, as Rule 0.1.2 permits; it is
      // not a numeric conversion. A frame is known to exist here, so the result carries no information.
      // codeql::misra_deviation_next_line(DEV-014.02)
      static_cast<void>(frames.pop());

      return result<value_index>::success(completed);
    }
  } // namespace detail
} // namespace adaptive_json5
