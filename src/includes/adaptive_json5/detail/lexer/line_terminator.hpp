#ifndef LINE_TERMINATOR_HPP_
#define LINE_TERMINATOR_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Original byte span of one line-terminator sequence.
   * @par Requirements
   * P007; J5-011; J5-017; J5-044.
   * @note CRLF occupies two bytes but represents one sequence.
   */
  struct line_terminator_span
  {
      std::size_t byte_offset;
      std::size_t byte_length;
  };

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
  result<std::optional<line_terminator_span>> consume_line_terminator(cursor& source) noexcept;
} // namespace adaptive_json5

#endif /* LINE_TERMINATOR_HPP_ */
