#include "adaptive_json5/text_position.hpp"

#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace adaptive_json5
{
  namespace
  {
    constexpr std::uint32_t c_line_feed = 0x0AU;             // '\n'
    constexpr std::uint32_t c_carriage_return = 0x0DU;       // '\r'
    constexpr std::uint32_t c_line_separator = 0x2028U;      // '\u2028'
    constexpr std::uint32_t c_paragraph_separator = 0x2029U; // '\u2029'
    constexpr std::size_t c_single_byte = 1;
    constexpr std::size_t c_first_column = 1;
    constexpr std::size_t c_crlf_byte_count = 2;

    /**
     * @brief Report whether a decoded character ends a line.
     * @par Requirements
     * P039; J5-044.
     * @param value Decoded Unicode scalar.
     * @return True for LF, CR, U+2028 and U+2029.
     * @note CRLF is recognized by the caller so that it counts as one break.
     * @par ID SWU-INPUT-010
     */
    [[nodiscard]] bool is_line_break(std::uint32_t value) noexcept
    {
      return (value == c_line_feed) || (value == c_carriage_return) || (value == c_line_separator) ||
             (value == c_paragraph_separator);
    }
  } // namespace

  /**
   * @brief Walk whole characters up to the offset, counting lines and code points.
   * @par Requirements
   * P039; J5-003; J5-044; J5-052.
   * @par ID SWU-INPUT-011
   */
  result<text_position> locate(input_view input, std::size_t byte_offset) noexcept
  {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t step = 1U;

    using position_result = result<text_position>;

    if (byte_offset > input.size)
    {
      return position_result::failure(error::access_failure(access_error_reason::invalid_argument));
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
      std::uint32_t value{};

      // Invalid bytes are deliberately not an error here: we may be locating that very encoding failure.
      if (decoded.has_value() && decoded.value_if()->has_value())
      {
        value = (*decoded.value_if())->value;
        length = (*decoded.value_if())->byte_length;
      }

      const bool crlf = ((value == c_carriage_return) && (attempt.peek() == '\n'));

      if (crlf)
      {
        length = c_crlf_byte_count;
      }

      // An offset inside a character or CRLF belongs to the position where that sequence starts.
      if ((byte_offset - start) < length)
      {
        break;
      }

      source.advance(length);

      if (is_line_break(value))
      {
        if (step > (maximum - position.line))
        {
          return position_result::failure(error::access_failure(access_error_reason::invalid_argument));
        }
        position.line = position.line + step;
        position.column = c_first_column;
      }
      else
      {
        if (step > (maximum - position.column))
        {
          return position_result::failure(error::access_failure(access_error_reason::invalid_argument));
        }
        position.column = position.column + step;
      }
    }

    return position_result::success(position);
  }
} // namespace adaptive_json5
