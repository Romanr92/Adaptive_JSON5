// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/comment.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/line_terminator.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Consume a // comment if one begins at the current position.
   * @par Requirements
   * P009; J5-012.
   * @param source Cursor over borrowed UTF-8 input.
   * @return A comment span, successful std::nullopt when no comment starts
   * here, or an explicit encoding error.
   * @note Leaves the terminating line break unread.
   * No match or encoding failure leaves source unchanged.
   * Requires no allocation and throws no exceptions.
   */
  result<std::optional<comment_span>> consume_line_comment(cursor& source) noexcept
  {
    using comment_result = result<std::optional<comment_span>>;

    constexpr char c_slash = '/';
    constexpr std::size_t c_opener_byte_count = 2;

    const auto first = source.peek();
    const auto second = source.peek(1);

    if ((!first.has_value()) || (!second.has_value()) || (*first != c_slash) || (*second != c_slash))
    {
      return comment_result::success(std::nullopt);
    }

    const std::size_t start = source.offset();

    // Copy the cursor not the input bytes and work on this copy so a failure can't advance the caller's cursor.
    cursor candidate = source;
    candidate.advance(c_opener_byte_count);

    while (!candidate.is_at_end())
    {

      cursor lookahead = candidate;
      const auto terminator = consume_line_terminator(lookahead);

      if (!terminator.has_value())
      {
        return comment_result::failure(*terminator.error_if());
      }

      if (terminator.value_if()->has_value())
      {
        break;
      }

      // Validate ignored contents so malformed UTF-8 cannot pass unnoticed.
      const auto decoded = decode_utf8(candidate);

      if (!decoded.has_value())
      {
        return comment_result::failure(*decoded.error_if());
      }
    }

    // Position counts bytes, so this also works for multibyte characters.
    const std::size_t length = candidate.offset() - start;
    source = candidate;

    return comment_result::success(std::optional<comment_span>{comment_span{start, length}});
  }

  /**
   * @brief Consume a block comment through its first closing delimiter.
   * @par Requirements
   * P010; J5-012.
   * @param source Cursor over borrowed UTF-8 input.
   * @return A comment span, successful std::nullopt when no block comment
   * begins here, or an explicit encoding or syntax error.
   * @note Includes both delimiters and does not nest comments.
   * An unterminated comment reports a syntax error at its opening slash.
   * No match or failure leaves source unchanged.
   * Requires no allocation and throws no exceptions.
   * This recognizer does not enforce strict JSON mode.
   */
  result<std::optional<comment_span>> consume_block_comment(cursor& source) noexcept
  {
    using comment_result = result<std::optional<comment_span>>;

    constexpr char c_slash = '/';
    constexpr char c_asterisk = '*';
    constexpr std::size_t c_delimiter_byte_count = 2;

    const auto first = source.peek();
    const auto second = source.peek(1);

    // Keep line comments and unrelated slash-prefixed input for other recognizers.
    if ((!first.has_value()) || (!second.has_value()) || (*first != c_slash) || (*second != c_asterisk))
    {
      return comment_result::success(std::nullopt);
    }

    const std::size_t start = source.offset();

    // An incomplete or malformed comment must not publish partial advancement.
    cursor candidate = source;
    candidate.advance(c_delimiter_byte_count);

    while (!candidate.is_at_end())
    {
      const auto current = candidate.peek();
      const auto following = candidate.peek(1);

      // Only the first closer matters: embedded openers do not create nesting
      if (current.has_value() && following.has_value() && (*current == c_asterisk) && (*following == c_slash))
      {
        candidate.advance(c_delimiter_byte_count);

        const std::size_t length = candidate.offset() - start;
        source = candidate;

        return comment_result::success(std::optional<comment_span>{comment_span{start, length}});
      }

      // Validate all contents, including text that the parser will discard
      const auto decoded = decode_utf8(candidate);

      if (!decoded.has_value())
      {
        return comment_result::failure(*decoded.error_if());
      }
    }
    // Point to the opener so the caller can locate the unterminated comment
    return comment_result::failure(error::at_offset(error_code::syntax, start));
  }
} // namespace adaptive_json5
