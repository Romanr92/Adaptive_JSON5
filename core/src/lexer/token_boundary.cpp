#include "lexer/token_boundary.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/punctuation.hpp"
#include "lexer/whitespace.hpp"
#include "unicode/utf8.hpp"

namespace adaptive_json5
{
  /**
   * @brief Check the immediate boundary after an unquoted token.
   * @par Requirements
   * P028; J5-009; J5-010; J5-011; J5-012; J5-024.
   * @param source Copy of the cursor immediately after the token spelling.
   * @param mode Selected input syntax.
   * @return Success for EOF, admitted whitespace, structural punctuation,
   * or a JSON5 comment opener; otherwise a located error.
   * @note Never changes the caller's cursor and requires no allocation.
   * Checks only the immediate boundary, not the remaining document.
   * Comment contents and later trivia require normal trivia processing.
   * Structural punctuation still requires grammatical context validation.
   * Intended for numbers, primitive literals and unquoted member names.
   */
  status_result check_unquoted_token_boundary(cursor source, syntax_mode mode) noexcept
  {
    cursor lookahead = source;
    const auto decoded = decode_utf8(lookahead);

    if (!decoded.has_value())
    {
      return status_result::failure(*decoded.error_if());
    }

    const auto& character = *decoded.value_if();
    if (!character.has_value())
    {
      return status_result::success();
    }

    const char32_t scalar = character->value;

    const bool json_whitespace = (scalar == U' ') || (scalar == U'\t') || (scalar == U'\r') || (scalar == U'\n');

    if (json_whitespace)
    {
      return status_result::success();
    }

    if (mode == syntax_mode::json5)
    {
      const bool additional_whitespace = is_json5_space(scalar) || (scalar == U'\u2028') || (scalar == U'\u2029');

      if (additional_whitespace)
      {
        return status_result::success();
      }
    }

    // A lexical boundary does not decide whether this punctuation fits the container.
    const auto punctuation = consume_punctuation(source);
    if (punctuation.has_value())
    {
      return status_result::success();
    }

    if (scalar == U'/')
    {
      // UTF-8 lookahead already advanced past the opening slash.
      const auto next = lookahead.peek();
      const bool comment_opener = next.has_value() && ((*next == '/') || (*next == '*'));

      if (comment_opener && (mode == syntax_mode::json5))
      {
        return status_result::success();
      }
    }

    return status_result::failure(error::syntax_at(character->byte_offset, syntax_error_reason::unexpected_character));
  }
} // namespace adaptive_json5
