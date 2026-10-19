// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/trivia.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/comment.hpp"
#include "adaptive_json5/detail/lexer/whitespace.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  status_result skip_strict_json_whitespace(cursor& source) noexcept
  {
    constexpr char32_t c_space = 0x0020;
    constexpr char32_t c_horizontal_tab = 0x0009;
    constexpr char32_t c_carriage_return = 0x000D;
    constexpr char32_t c_line_feed = 0x000A;

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

      const char32_t value = character->value;

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

  result<trivia_span> skip_trivia(cursor& source, syntax_mode mode) noexcept
  {
    using trivia_result = result<trivia_span>;

    constexpr char c_slash = '/';
    constexpr char c_asterisk = '*';

    const std::size_t start = source.offset();

    // Whitespace and comments from one run whose advancement succeeds togather.
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

      // Leave a lone slash or another tojen for the token recognizer.
      if ((!first.has_value()) || (!second.has_value()) || (*first != c_slash))
      {
        break;
      }

      const bool is_line_comment = (*second == c_slash);
      const bool is_block_comment = (*second == c_asterisk);

      if ((!is_line_comment) && (!is_block_comment))
      {
        break;
      }

      // Strict mode must reject comments instead of silently skipping them.
      if (mode == syntax_mode::strict_json)
      {
        return trivia_result::failure(error::at_offset(error_code::syntax, candidate.offset()));
      }

      const auto comment = is_line_comment ? consume_line_comment(candidate) : consume_block_comment(candidate);

      if (!comment.has_value())
      {
        return trivia_result::failure(*comment.error_if());
      }

      // A confirmed comment opener guarantees progress on success.
      // Repeat because another whitespace run or comment may follow.
    }

    const std::size_t length = candidate.offset() - start;
    source = candidate;

    return trivia_result::success(trivia_span{start, length});
  }
} // namespace adaptive_json5
