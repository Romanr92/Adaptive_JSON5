#include "lexer/line_terminator.hpp"

#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Consume one line-terminator sequence if present.
   * @par Requirements
   * P007; J5-011; J5-017; J5-044.
   * @param source Cursor over borrowed UTF-8 input.
   * @return A terminator span, successful std::nullopt when none is
   * present, or an explicit encoding error.
   * @note EOF, nonterminators and encoding failures leave source unchanged.
   * Recognizes LF, CR, CRLF, U+2028 and U+2029 without allocation.
   */
  result<std::optional<line_terminator_span>> consume_line_terminator(cursor& source) noexcept
  {
    using terminator_result = result<std::optional<line_terminator_span>>;

    constexpr char32_t c_line_feed = 0x000A;
    constexpr char32_t c_carriage_return = 0x000D;
    constexpr char32_t c_line_separator = 0x2028;
    constexpr char32_t c_paragraph_separator = 0x2029;

    constexpr char c_line_feed_byte = '\n';
    constexpr std::size_t c_line_feed_byte_count = 1;

    const std::size_t start = source.offset();

    // Decode through a copy so unsuccesful recognition consumes nothing.
    cursor candidate = source;
    const auto decoded = decode_utf8(candidate);

    if (!decoded.has_value())
    {
      // Preserve the decoder's encoding category and original byte offset.
      return terminator_result::failure(*decoded.error_if());
    }

    const auto& character = *decoded.value_if();

    if (!character.has_value())
    {
      return terminator_result::success(std::nullopt);
    }

    const char32_t scalar = character->value;

    if ((scalar != c_line_feed) && (scalar != c_carriage_return) && (scalar != c_line_separator) &&
        (scalar != c_paragraph_separator))
    {
      // The decoded character belongs to the next token or another trivia rule.
      // Discard the temporary advancement and leave source unchanged.
      return terminator_result::success(std::nullopt);
    }

    if (scalar == c_carriage_return)
    {
      // CR optionally absorbs an immediately following ASCII LF.
      // Inspect only that byte; other following input remains for the next operation.
      const auto following = candidate.peek();

      if ((following.has_value()) && (*following == c_line_feed_byte))
      {
        candidate.advance(c_line_feed_byte_count);
      }
    }

    // Offsets measure original bytes, so Unicode separators span three bytes
    // and CRLF spans two bytes without counting as two terminator sequences.
    const std::size_t length = candidate.offset() - start;

    // Recognition succeded: comit the complete sequence in one operation.
    source = candidate;

    return terminator_result::success(std::optional<line_terminator_span>{line_terminator_span{start, length}});
  }
} // namespace adaptive_json5
