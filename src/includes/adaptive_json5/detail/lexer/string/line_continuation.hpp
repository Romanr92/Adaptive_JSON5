#ifndef LINE_CONTINUATION_HPP_
#define LINE_CONTINUATION_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/line_terminator.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

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
  result<std::optional<line_terminator_span>> consume_line_continuation(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* LINE_CONTINUATION_HPP_ */
