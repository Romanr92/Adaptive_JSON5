// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/parser/parser_dispatch.hpp"

#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    result<parser_action> next_container_action(const parser_frame& frame, std::optional<char> current,
                                                std::size_t byte_offset, syntax_mode mode) noexcept
    {
      using action_result = result<parser_action>;

      if (!current.has_value())
      {
        return action_result::failure(error::at_offset(error_code::syntax, byte_offset));
      }

      const char byte = *current;

      switch (frame.state)
      {
      // P034: distinguish an empty array, a completed element and a consumed comma.
      case parser_state::array_first_or_end:
        if (byte == ']')
        {
          return action_result::success(parser_action::close_container);
        }
        break;

      case parser_state::array_value_after_comma:
        if ((byte == ']') && (mode == syntax_mode::json5))
        {
          return action_result::success(parser_action::close_container);
        }
        break;

      case parser_state::array_comma_or_end:
        if (byte == ',')
        {
          return action_result::success(parser_action::consume_comma);
        }

        if (byte == ']')
        {
          return action_result::success(parser_action::close_container);
        }

        return action_result::failure(error::at_offset(error_code::syntax, byte_offset));

      // P035: require each name, colon and value before allowing a member separator.
      case parser_state::object_first_name_or_end:
        if (byte == '}')
        {
          return action_result::success(parser_action::close_container);
        }

        return action_result::success(parser_action::read_name);

      case parser_state::object_name_after_comma:
        if (byte == '}')
        {
          if (mode == syntax_mode::json5)
          {
            return action_result::success(parser_action::close_container);
          }

          return action_result::failure(error::at_offset(error_code::syntax, byte_offset));
        }

        return action_result::success(parser_action::read_name);

      case parser_state::object_colon:
        if (byte == ':')
        {
          return action_result::success(parser_action::consume_colon);
        }

        return action_result::failure(error::at_offset(error_code::syntax, byte_offset));

      case parser_state::object_value:
        break;

      case parser_state::object_comma_or_end:
        if (byte == ',')
        {
          return action_result::success(parser_action::consume_comma);
        }

        if (byte == '}')
        {
          return action_result::success(parser_action::close_container);
        }

        return action_result::failure(error::at_offset(error_code::syntax, byte_offset));

      default:
        return action_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // P034/P035: separators cannot create array holes or replace member values.
      if ((byte == ',') || (byte == ':') || (byte == ']') || (byte == '}'))
      {
        return action_result::failure(error::at_offset(error_code::syntax, byte_offset));
      }

      return action_result::success(parser_action::read_value);
    }
  } // namespace detail
} // namespace adaptive_json5
