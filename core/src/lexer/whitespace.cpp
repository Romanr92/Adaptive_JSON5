#include "lexer/whitespace.hpp"

#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/line_terminator.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Consume consecutive JSON5 spaces and line-terminator sequences.
   * @par Requirements
   * P008; J5-011; J5-025.
   * @param source Cursor positioned before possible whitespace.
   * @return The consumed byte span and number of line breaks,
   * or an explicit encoding error.
   * @note Stops before the first valid nonwhitespace character.
   * Encoding failure leaves source unchanged, including any preceding
   * whitespace from this attempted run. Requires no allocation.
   * This operation implements JSON5 whitespace, not strict JSON policy.
   */
  result<whitespace_span> skip_json5_whitespace(cursor& source) noexcept
  {
    using whitespace_result = result<whitespace_span>;

    const std::size_t start = source.offset();
    std::size_t line_break_count = 0;

    // Work on a copy so an encoding failure cannot partially consume the run.
    cursor candidate = source;

    while (!candidate.is_at_end())
    {
      // Reuse P007 so CRLF remains one sequence and Unicode separators
      // retain their original three-byte spans.
      const auto terminator = consume_line_terminator(candidate);

      if (!terminator.has_value())
      {
        return whitespace_result::failure(*terminator.error_if());
      }

      if (terminator.value_if()->has_value())
      {
        // The recognizer already advanced candidate over the complete sequence.
        ++line_break_count;
        continue;
      }

      // Decode through another copy: a valid nonspace must remain unread.
      cursor next = candidate;
      const auto decoded = decode_utf8(next);

      if (!decoded.has_value())
      {
        return whitespace_result::failure(*decoded.error_if());
      }

      const auto& character = *decoded.value_if();

      if (!character.has_value())
      {
        // No more input remains to classify.
        break;
      }

      if (!is_json5_space(character->value))
      {
        // Keep candidate before this character for the next token operation.
        break;
      }

      // The scalar is whitespace; accept it's validated byte advancement.
      candidate = next;
    }

    // Cursor offsets count original bytes, including multibyte whitespace.
    const std::size_t length = candidate.offset() - start;

    // The entire run succeded, so publish it's advancement togather.
    source = candidate;

    return whitespace_result::success(whitespace_span{start, length, line_break_count});
  }
} // namespace adaptive_json5
