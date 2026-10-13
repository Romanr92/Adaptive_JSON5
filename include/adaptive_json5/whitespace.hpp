#ifndef WHITESPACE_HPP_
#define WHITESPACE_HPP_

#include "cursor.hpp"
#include "line_terminator.hpp"
#include "result.hpp"
#include "unicode.hpp"
#include "unicode_tables.hpp"
#include "utf8.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Original byte span and line count of consumed whitespace.
   * @par Requirements
   * P008; J5-011; J5-025.
   * @note CRLF contributes one line break. An empty run has zero
   * byte length and zero line breaks.
   */
  struct whitespace_span
  {
      std::size_t byte_offset;
      std::size_t byte_length;
      std::size_t line_break_count;
  };

  /**
   * @brief Recognize JSON5 space characters other than line terminators.
   * @par Requirements
   * P008; J5-011; J5-025.
   * @param value Decoded Unicode scalar.
   * @return True for a named JSON5 space or admitted space separator.
   * @note Preserves Unicode 3.0 whitespace memberships alongside
   * the selected Unicode 17.0.0 category data.
   */
  inline constexpr bool is_json5_space(char32_t value) noexcept
  {
    constexpr char32_t c_horizontal_tab = 0x0009;
    constexpr char32_t c_vertical_tab = 0x000B;
    constexpr char32_t c_form_feed = 0x000C;
    constexpr char32_t c_space = 0x0020;
    constexpr char32_t c_no_break_space = 0x00A0;
    constexpr char32_t c_byte_order_mark = 0xFEFF;

    // These characters are explicitly named by the JSON5 whitespace grammar.
    if ((value == c_horizontal_tab) || (value == c_vertical_tab) || (value == c_form_feed) || (value == c_space) ||
        (value == c_no_break_space) || (value == c_byte_order_mark))
    {
      return true;
    }

    // Recognize space separators from the selected modern Unicode data.
    if (is_unicode_space_separator(value))
    {
      return true;
    }

    // Preserve historical whitespace memberships whoose categories changed.
    // For example, U+200B was Zs in Unicode 3.0 but is Cf in modern data.
    return detail::contains_unicode(value, detail::c_unicode_3_space_separator_ranges);
  }

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
  inline result<whitespace_span> skip_json5_whitespace(cursor& source) noexcept
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

#endif /* WHITESPACE_HPP_ */
