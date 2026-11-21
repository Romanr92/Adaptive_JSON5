#include "lexer/token_boundary.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/punctuation.hpp"
#include "lexer/whitespace.hpp"
#include "unicode/utf8.hpp"

#include <cstdint>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Test whether a decoded scalar separates tokens in the selected syntax mode.
     * @par Requirements
     * P028; J5-009; J5-010; J5-011; J5-012; J5-024.
     * @par ID SWU-LEXER-073
     */
    bool is_boundary_space(std::uint32_t scalar, syntax_mode mode) noexcept
    {
      const bool json_whitespace =
        (scalar == 0x20U) || (scalar == 0x09U) || (scalar == 0x0DU) || (scalar == 0x0AU); // ' ', '\t', '\r', '\n'
      return json_whitespace ||
             ((mode == syntax_mode::json5) && (is_json5_space(scalar) || (scalar == 0x2028U) || (scalar == 0x2029U)));
    }
  } // namespace

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
   * @par ID SWU-LEXER-048
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

    const std::uint32_t scalar = character->value;

    if (is_boundary_space(scalar, mode))
    {
      return status_result::success();
    }

    // A lexical boundary does not decide whether this punctuation fits the container.
    const auto punctuation = consume_punctuation(source);
    if (punctuation.has_value())
    {
      return status_result::success();
    }

    if (scalar == 0x2FU) // '/'
    {
      // UTF-8 lookahead already advanced past the opening slash.
      const auto next = lookahead.peek();
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.71)
      const bool comment_opener = next.has_value() && ((*next == '/') || (*next == '*'));

      if (comment_opener && (mode == syntax_mode::json5))
      {
        return status_result::success();
      }
    }

    return status_result::failure(error::syntax_at(character->byte_offset, syntax_error_reason::unexpected_character));
  }
} // namespace adaptive_json5
