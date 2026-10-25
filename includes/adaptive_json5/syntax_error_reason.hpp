#ifndef ADAPTIVE_JSON5_SYNTAX_ERROR_REASON_HPP_
#define ADAPTIVE_JSON5_SYNTAX_ERROR_REASON_HPP_

namespace adaptive_json5
{
  /**
   * @brief Describe why input violates the selected grammar.
   * @par Requirements
   * J5-043; J5-044; issue #96.
   * @par Reasons
   * - unexpected_end: Input ends inside a token or before a required value or delimiter.
   * - unexpected_character: A character cannot start or continue any token here.
   * - unexpected_comma: A comma appears where no separator is allowed, e.g. `[,1]` or `[1,,2]`.
   * - unexpected_colon: A colon appears outside a member, or a second time.
   * - unexpected_closing_bracket: `]` or `}` closes the wrong container or none.
   * - missing_colon: A member name is not followed by a colon.
   * - missing_comma: Two items follow each other without a separator.
   * - missing_value: A colon or separator is followed by a closing bracket.
   * - expected_member_name: An object position requires a member name, e.g. `{1:2}`.
   * - unterminated_string: A string reaches end of input without its closing quote.
   * - unterminated_comment: A block comment reaches end of input without its closer.
   * - line_terminator_in_string: A raw line terminator appears inside a string.
   * - control_character_in_string: Strict JSON forbids a raw U+0000-U+001F character.
   * - invalid_escape: A backslash sequence is malformed or not permitted.
   * - invalid_number: A number spelling is malformed, e.g. a leading zero or missing digits.
   * - invalid_identifier: A member name contains an illegal character or escape.
   * - unknown_literal: An unquoted word in value position is not null, true or false.
   * - not_allowed_in_strict_json: A JSON5-only feature was used in strict JSON mode.
   * - empty_input: No value appears before end of input.
   * - trailing_content: More content follows the complete root value.
   * @note The reason refines error_code::syntax; it never replaces the error code.
   * Callers may switch on it; they never need to parse formatted message text.
   */
  enum class syntax_error_reason
  {
    unexpected_end,
    unexpected_character,
    unexpected_comma,
    unexpected_colon,
    unexpected_closing_bracket,
    missing_colon,
    missing_comma,
    missing_value,
    expected_member_name,
    unterminated_string,
    unterminated_comment,
    line_terminator_in_string,
    control_character_in_string,
    invalid_escape,
    invalid_number,
    invalid_identifier,
    unknown_literal,
    not_allowed_in_strict_json,
    empty_input,
    trailing_content
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SYNTAX_ERROR_REASON_HPP_ */
