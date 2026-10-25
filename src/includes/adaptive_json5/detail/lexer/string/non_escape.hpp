#ifndef NON_ESCAPE_HPP_
#define NON_ESCAPE_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Consume a JSON5 non-escape character after its backslash.
   * @par Requirements
   * P018; J5-016.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return The unchanged scalar and its source span, successful nullopt
   * for a special escape or line terminator, or a located error.
   * @note JSON5 accepts permitted non-escape characters; strict JSON rejects them.
   * Digits 1 through 9 produce syntax errors.
   * Special escape spellings remain for their owning decoders.
   * Success consumes one complete UTF-8 character.
   * Failure and no-match leave source unchanged.
   * The returned span excludes the already-consumed backslash.
   * No allocation is performed.
   */
  [[nodiscard]] result<std::optional<decoded_character>> consume_non_escape(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* NON_ESCAPE_HPP_ */
