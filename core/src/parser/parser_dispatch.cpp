#include "parser/parser_dispatch.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "parser/parser_frame.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Name an unexpected structural byte, or fall back to the state's own reason.
     * @par Requirements
     * J5-043; issue #96.
     * @param byte Byte found where the grammar required something else.
     * @param otherwise Reason used when byte is not structural punctuation.
     * @return The punctuation-specific reason or otherwise.
     * @note Performs no allocation.
     * @par ID SWU-PARSER-006
     */
    syntax_error_reason separator_reason(char byte, syntax_error_reason otherwise) noexcept
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
      // character value. The tool reports the integral promotion of the condition and labels. The parser
      // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
      // character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.79)
      switch (byte)
      {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
      // character value. The tool reports the integral promotion of the condition and labels. The parser
      // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
      // character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.78)
      case ',':
        return syntax_error_reason::unexpected_comma;
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
      // character value. The tool reports the integral promotion of the condition and labels. The parser
      // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
      // character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.77)
      case ':':
        return syntax_error_reason::unexpected_colon;
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
      // character value. The tool reports the integral promotion of the condition and labels. The parser
      // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
      // character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.76)
      case ']':
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
      // character value. The tool reports the integral promotion of the condition and labels. The parser
      // compares UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution
      // character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.75)
      case '}':
        return syntax_error_reason::unexpected_closing_bracket;
      default:
        return otherwise;
      }
    }
  } // namespace

  namespace detail
  {
    namespace
    {
      /**
       * @brief Select a member name or accept the closing brace after a JSON5 trailing comma.
       * @par Requirements
       * P033; P034; P035; J5-013; J5-027.
       * @par ID SWU-PARSER-050
       */
      result<parser_action> select_name_after_comma(char byte, syntax_mode mode, std::size_t byte_offset) noexcept
      {
        using action_result = result<parser_action>;
        if (byte == '}')
        {
          if (mode == syntax_mode::json5)
          {
            return action_result::success(parser_action::close_container);
          }

          return action_result::failure(error::syntax_at(byte_offset, syntax_error_reason::not_allowed_in_strict_json));
        }

        return action_result::success(parser_action::read_name);
      }

      /**
       * @brief Select a comma or the matching closer after a completed container entry.
       * @par Requirements
       * P033; P034; P035; J5-013; J5-027.
       * @par ID SWU-PARSER-051
       */
      result<parser_action> select_separator(char byte, char closing, std::size_t byte_offset) noexcept
      {
        using action_result = result<parser_action>;
        if (byte == ',')
        {
          return action_result::success(parser_action::consume_comma);
        }

        if (byte == closing)
        {
          return action_result::success(parser_action::close_container);
        }

        return action_result::failure(
          error::syntax_at(byte_offset, separator_reason(byte, syntax_error_reason::missing_comma)));
      }

      /**
       * @brief Reject misplaced punctuation or request a value from the lexer.
       * @par Requirements
       * P033; P034; P035; J5-013; J5-027.
       * @par ID SWU-PARSER-052
       */
      result<parser_action> select_value_action(parser_state state, char byte, std::size_t byte_offset) noexcept
      {
        using action_result = result<parser_action>;
        // P034/P035: separators cannot create array holes or replace member values.
        if ((byte == ',') || (byte == ':') || (byte == ']') || (byte == '}'))
        {
          auto reason = separator_reason(byte, syntax_error_reason::unexpected_character);

          // A member colon promises a value, so any punctuation here means the value is missing.
          if (state == parser_state::object_value)
          {
            reason = syntax_error_reason::missing_value;
          }
          // JSON5 accepted this trailing comma above; only strict mode reaches here with it.
          else if ((state == parser_state::array_value_after_comma) && (byte == ']'))
          {
            reason = syntax_error_reason::not_allowed_in_strict_json;
          }
          else
          {
            // Other states keep the separator-based reason.
          }

          return action_result::failure(error::syntax_at(byte_offset, reason));
        }

        return action_result::success(parser_action::read_value);
      }
    } // namespace

    /**
     * @brief Select the next action for the innermost open container.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027.
     * @param frame Valid innermost open-container frame.
     * @param current Current byte after caller-managed trivia, or nullopt at EOF.
     * @param byte_offset Original offset of current, or the EOF offset.
     * @param mode Selected syntax mode.
     * @return Next action or a located syntax error.
     * @note Performs no allocation, input consumption or state mutation.
     * Lexers validate value and name spellings after dispatch.
     * EOF inside an open container is always a syntax error.
     * Post-comma states are entered only after a completed element or member.
     * @par ID SWU-PARSER-007
     */
    result<parser_action> next_container_action(const parser_frame& frame, std::optional<char> current,
                                                std::size_t byte_offset, syntax_mode mode) noexcept
    {
      using action_result = result<parser_action>;

      if (!current.has_value())
      {
        return action_result::failure(error::syntax_at(byte_offset, syntax_error_reason::unexpected_end));
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
        return select_separator(byte, ']', byte_offset);

      // P035: require each name, colon and value before allowing a member separator.
      case parser_state::object_first_name_or_end:
        if (byte == '}')
        {
          return action_result::success(parser_action::close_container);
        }

        return action_result::success(parser_action::read_name);

      case parser_state::object_name_after_comma:
        return select_name_after_comma(byte, mode, byte_offset);

      case parser_state::object_colon:
        if (byte == ':')
        {
          return action_result::success(parser_action::consume_colon);
        }

        return action_result::failure(error::syntax_at(byte_offset, syntax_error_reason::missing_colon));

      case parser_state::object_value:
        break;

      case parser_state::object_comma_or_end:
        return select_separator(byte, '}', byte_offset);

      default:
        return action_result::failure(error::internal_fault());
      }

      return select_value_action(frame.state, byte, byte_offset);
    }
  } // namespace detail
} // namespace adaptive_json5
