#include "lexer/trivia.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/comment.hpp"
#include "lexer/whitespace.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Consume whitespace admitted by strict JSON.
   * @par Requirements
   * P011; J5-010.
   * @param source Cursor positioned before possible whitespace.
   * @return Success, or an explicit encoding error.
   * @note Admits only space, horizontal tab, carriage return and line feed.
   * Stops before the first other valid character.
   * Encoding failure leaves source unchanged.
   * Requires no allocation and throws no exceptions.
   * @par ID SWU-LEXER-049
   */
  status_result skip_strict_json_whitespace(cursor& source) noexcept
  {
    constexpr std::uint32_t c_space = 0x0020;
    constexpr std::uint32_t c_horizontal_tab = 0x0009;
    constexpr std::uint32_t c_carriage_return = 0x000D;
    constexpr std::uint32_t c_line_feed = 0x000A;

    // A later encoding failure must not publish partial consumption.
    cursor candidate = source;

    while (!candidate.is_at_end())
    {
      // Preserve the next token's first character when whitespace ends.
      cursor lookahead = candidate;
      const auto decoded = decode_utf8(lookahead);

      if (!decoded.has_value())
      {
        return status_result::failure(*decoded.error_if());
      }

      const auto& character = *decoded.value_if();

      if (!character.has_value())
      {
        break;
      }

      const std::uint32_t value = character->value;

      // JSON5's additional whitespace must remain inadmissable in strict JSON.
      if ((value != c_space) && (value != c_horizontal_tab) && (value != c_carriage_return) && (value != c_line_feed))
      {
        break;
      }

      candidate = lookahead;
    }

    source = candidate;
    return status_result::success();
  }

  /**
   * @brief Consume consecutive whitespace and comments admitted by the mode.
   * @par Requirements
   * P011; J5-010; J5-012; J5-024.
   * @param source Cursor positioned between tokens.
   * @param mode Selected input language.
   * @return The consumed trivia span, or an explicit encoding or syntax error.
   * @note Strict JSON rejects comment openers at their opening slash.
   * Stops before the first nontrivia character.
   * Failure leaves source unchanged, including preceding trivia in this run.
   * Requires no allocation and throws no exceptions.
   * Call between tokens, never inside a token spelling.
   * @par ID SWU-LEXER-050
   */
  result<trivia_span> skip_trivia(cursor& source, syntax_mode mode) noexcept
  {
    using trivia_result = result<trivia_span>;

    constexpr char c_slash = '/';
    constexpr char c_asterisk = '*';

    const std::size_t start = source.offset();

    // Whitespace and comments from one run whose advancement succeeds together.
    cursor candidate = source;

    while (true)
    {
      if (mode == syntax_mode::json5)
      {
        const auto whitespace = skip_json5_whitespace(candidate);

        if (!whitespace.has_value())
        {
          return trivia_result::failure(*whitespace.error_if());
        }
      }
      else
      {
        const auto whitespace = skip_strict_json_whitespace(candidate);

        if (!whitespace.has_value())
        {
          return trivia_result::failure(*whitespace.error_if());
        }
      }

      const auto first = candidate.peek();
      const auto second = candidate.peek(1);

      // Leave a lone slash or another token for the token recognizer.
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.70)
      if ((!first.has_value()) || (!second.has_value()) || (*first != c_slash))
      {
        break;
      }

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.69)
      const bool is_line_comment = (*second == c_slash);
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.68)
      const bool is_block_comment = (*second == c_asterisk);

      if ((!is_line_comment) && (!is_block_comment))
      {
        break;
      }

      // Strict mode must reject comments instead of silently skipping them.
      if (mode == syntax_mode::strict_json)
      {
        return trivia_result::failure(
          error::syntax_at(candidate.offset(), syntax_error_reason::not_allowed_in_strict_json));
      }

      const auto comment = is_line_comment ? consume_line_comment(candidate) : consume_block_comment(candidate);

      if (!comment.has_value())
      {
        return trivia_result::failure(*comment.error_if());
      }

      // A confirmed comment opener guarantees progress on success.
      // Repeat because another whitespace run or comment may follow.
    }

    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: A cursor only moves forward, so the later offset is never below the earlier offset
    // it started from, and the difference is the number of bytes consumed.
    // codeql::misra_deviation_next_line(DEV-004.115)
    const std::size_t length = candidate.offset() - start;
    source = candidate;

    return trivia_result::success(trivia_span{start, length});
  }
} // namespace adaptive_json5
