#include "adaptive_json5/text_position.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace
  {
    constexpr char32_t c_line_feed = U'\n';
    constexpr char32_t c_carriage_return = U'\r';
    constexpr char32_t c_line_separator = U'\u2028';
    constexpr char32_t c_paragraph_separator = U'\u2029';
    constexpr std::size_t c_single_byte = 1;
    constexpr std::size_t c_first_column = 1;

    /**
     * @brief Report whether a decoded character ends a line.
     * @par Requirements
     * P039; J5-044.
     * @param value Decoded Unicode scalar.
     * @return True for LF, CR, U+2028 and U+2029.
     * @note CRLF is recognized by the caller so that it counts as one break.
     */
    [[nodiscard]] bool is_line_break(char32_t value) noexcept
    {
      return (value == c_line_feed) || (value == c_carriage_return) || (value == c_line_separator) ||
             (value == c_paragraph_separator);
    }
  } // namespace

  /**
   * @brief Walk whole characters up to the offset, counting lines and code points.
   * @par Requirements
   * P039; J5-003; J5-044; J5-052.
   */
  result<text_position> locate(input_view input, std::size_t byte_offset) noexcept
  {
    using position_result = result<text_position>;

    if (byte_offset > input.size)
    {
      return position_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    text_position position{};
    cursor source{input};

    while (source.offset() < byte_offset)
    {
      const std::size_t start = source.offset();

      // Decode on a copy so a CR's following byte can be inspected before committing.
      cursor attempt = source;
      const auto decoded = decode_utf8(attempt);

      std::size_t length = c_single_byte;
      char32_t value{};

      // Invalid bytes are deliberately not an error here: we may be locating that very encoding failure.
      if (decoded.has_value() && decoded.value_if()->has_value())
      {
        value = (*decoded.value_if())->value;
        length = (*decoded.value_if())->byte_length;
      }

      const bool crlf = ((value == c_carriage_return) && (attempt.peek() == '\n'));

      if (crlf)
      {
        length += c_single_byte;
      }

      // An offset inside a character of CRLF belongs to the position where that sequence starts.
      if ((byte_offset - start) < length)
      {
        break;
      }

      source.advance(length);

      if (is_line_break(value))
      {
        ++position.line;
        position.column = c_first_column;
      }
      else
      {
        ++position.column;
      }
    }

    return position_result::success(position);
  }
} // namespace adaptive_json5
