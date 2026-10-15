#ifndef LINE_CONTINUATION_HPP_
#define LINE_CONTINUATION_HPP_

#include "cursor.hpp"
#include "error.hpp"
#include "line_terminator.hpp"
#include "options.hpp"
#include "result.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Consume a JSON5 line continuation after its backslash.
   * @par Requirements
   * P019; J5-017.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return The consumed terminator span, successful nullopt before another
   * character, or a located syntax/encoding error.
   * @note JSON5 accepts LF, CR, CRLF, U+2028 and U+2029.
   * Strict JSON rejects a recognized continuation without consuming it.
   * The returned span excludes the already-consumed backslash.
   * A matched continuation contributes no decoded string units.
   * EOF after the backslash reports syntax at the current end offset.
   * Failure and no-match leave source unchanged.
   * No allocation is performed.
   */
  inline result<std::optional<line_terminator_span>> consume_line_continuation(cursor& source,
                                                                               syntax_mode mode) noexcept
  {
    using continuation_result = result<std::optional<line_terminator_span>>;

    if (!source.peek().has_value())
    {
      return continuation_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    // Apply the syntax policy before committing the recognized terminator.
    cursor candidate = source;
    const auto terminator = consume_line_terminator(candidate);
    if (!terminator.has_value())
    {
      return continuation_result::failure(*terminator.error_if());
    }

    if (!terminator.value_if()->has_value())
    {
      return continuation_result::success(std::nullopt);
    }

    if (mode == syntax_mode::strict_json)
    {
      return continuation_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    source = candidate;
    return terminator;
  }
} // namespace adaptive_json5

#endif /* LINE_CONTINUATION_HPP_ */
