#include "lexer/string/non_escape.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "unicode/utf8.hpp"

#include <cstdint>
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
   * @par ID SWU-LEXER-032
   */
  result<std::optional<decoded_character>> consume_non_escape(cursor& source, syntax_mode mode) noexcept
  {
    using escape_result = result<std::optional<decoded_character>>;

    constexpr std::uint32_t c_first_forbidden_digit = 0x31U; // '1'
    constexpr std::uint32_t c_last_forbidden_digit = 0x39U;  // '9'
    constexpr std::uint32_t c_line_feed = 0x0AU;             // '\n'
    constexpr std::uint32_t c_carriage_return = 0x0DU;       // '\r'
    constexpr std::uint32_t c_line_separator = 0x2028;
    constexpr std::uint32_t c_paragraph_separator = 0x2029;

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
      return escape_result::failure(error::syntax_at(source.offset(), syntax_error_reason::unexpected_end));
    }

    const std::uint32_t scalar = character->value;

    // These spellings must never fall back to literal-character decoding.
    switch (scalar)
    {
    case 0x27U: // '\''
    case 0x22U: // '"'
    case 0x5CU: // '\\'
    case 0x62U: // 'b'
    case 0x66U: // 'f'
    case 0x6EU: // 'n'
    case 0x72U: // 'r'
    case 0x74U: // 't'
    case 0x76U: // 'v'
    case 0x30U: // '0'
    case 0x78U: // 'x'
    case 0x75U: // 'u'
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
      return escape_result::failure(error::syntax_at(character->byte_offset, syntax_error_reason::invalid_escape));
    }

    if (mode == syntax_mode::strict_json)
    {
      return escape_result::failure(
        error::syntax_at(character->byte_offset, syntax_error_reason::not_allowed_in_strict_json));
    }

    source = candidate;
    return decoded;
  }
} // namespace adaptive_json5
