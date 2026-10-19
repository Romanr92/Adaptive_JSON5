// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/non_escape.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
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
  result<std::optional<decoded_character>> consume_non_escape(cursor& source, syntax_mode mode) noexcept
  {
    using escape_result = result<std::optional<decoded_character>>;

    constexpr char32_t c_first_forbidden_digit = U'1';
    constexpr char32_t c_last_forbidden_digit = U'9';
    constexpr char32_t c_line_feed = U'\n';
    constexpr char32_t c_carriage_return = U'\r';
    constexpr char32_t c_line_separator = 0x2028;
    constexpr char32_t c_paragraph_separator = 0x2029;

    // Validate through a copy so unsuccessful recognition consumes nothing.
    cursor candidate = source;
    const auto decoded = decode_utf8(candidate);
    if (!decoded.has_value())
    {
      return escape_result::failure(*decoded.error_if());
    }

    const auto& character = *decoded.value_if();
    if (!character.has_value())
    {
      return escape_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    const char32_t scalar = character->value;

    // These spellings must never fall back to literal-character decoding.
    switch (scalar)
    {
    case U'\'':
    case U'"':
    case U'\\':
    case U'b':
    case U'f':
    case U'n':
    case U'r':
    case U't':
    case U'v':
    case U'0':
    case U'x':
    case U'u':
      return escape_result::success(std::nullopt);

    default:
      break;
    }

    // Line terminators belong to the continuation recognizer in P019.
    if ((scalar == c_line_feed) || (scalar == c_carriage_return) || (scalar == c_line_separator) ||
        (scalar == c_paragraph_separator))
    {
      return escape_result::success(std::nullopt);
    }

    if ((scalar >= c_first_forbidden_digit) && (scalar <= c_last_forbidden_digit))
    {
      return escape_result::failure(error::at_offset(error_code::syntax, character->byte_offset));
    }

    if (mode == syntax_mode::strict_json)
    {
      return escape_result::failure(error::at_offset(error_code::syntax, character->byte_offset));
    }

    source = candidate;
    return decoded;
  }
} // namespace adaptive_json5
