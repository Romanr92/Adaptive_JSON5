// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/parser/parser_container_close.hpp"

#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/detail/parser/parser_stack.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    result<value_index> close_container(document& destination, parser_stack& frames, const punctuation_token& closing,
                                        syntax_mode mode, std::size_t input_size, work_budget& work) noexcept
    {
      auto* frame = frames.top();

      if (frame == nullptr)
      {
        return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      if ((closing.byte_length != 1) ||
          ((closing.kind != punctuation_kind::right_bracket) && (closing.kind != punctuation_kind::right_brace)))
      {
        return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // Validate offsets before adding the closing byte or subtracting the opening.
      if ((closing.byte_offset >= input_size) || (frame->opening_offset >= closing.byte_offset))
      {
        return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      bool array = false;
      bool trailing_comma = false;

      switch (frame->state)
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

      default:
        // A member awaiting its colon or value cannot close successfully.
        return result<value_index>::failure(error::at_offset(error_code::syntax, closing.byte_offset));
      }

      const auto expected = array ? punctuation_kind::right_bracket : punctuation_kind::right_brace;

      // P034/P035: only JSON5 permits closing after one completed item and its comma.
      if ((closing.kind != expected) || (trailing_comma && (mode != syntax_mode::json5)))
      {
        return result<value_index>::failure(error::at_offset(error_code::syntax, closing.byte_offset));
      }

      // Every closable object has already transferred its pending name.
      if (frame->pending_name.has_value())
      {
        return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      const auto charged = work.charge(1);

      if (!charged.has_value())
      {
        return result<value_index>::failure(*charged.error_if());
      }

      const value_index completed = frame->container;
      const std::size_t end_offset = closing.byte_offset + 1;
      const source_span span{frame->opening_offset, end_offset - frame->opening_offset};

      const auto recorded = destination.set_source_span(completed, span, input_size);

      if (!recorded.has_value())
      {
        return result<value_index>::failure(*recorded.error_if());
      }

      // No fallible operation remains after publishing the completed span.
      frames.pop();

      return result<value_index>::success(completed);
    }
  } // namespace detail
} // namespace adaptive_json5
