#include "lexer/string/unicode_escape.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/string/hex_escape.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Decode one Unicode escape after its backslash.
   * @par Requirements
   * P017; J5-015; J5-018.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return One UTF-16 unit, successful nullopt before a non-u byte,
   * or a located syntax/encoding error.
   * @note Consumes u and exactly four ASCII hexadecimal digits.
   * Valid in both syntax modes; surrogate units are preserved unchanged.
   * No-match and failure leave source unchanged.
   * Missing digits report EOF; nonhex characters report their starting offset.
   * Nonmatching input is not validated. No allocation is performed.
   */
  result<std::optional<char16_t>> consume_unicode_escape(cursor& source) noexcept
  {
    using escape_result = result<std::optional<char16_t>>;

    constexpr char c_unicode_introducer = 'u';
    constexpr std::size_t c_introducer_byte_count = 1;
    constexpr std::size_t c_unicode_digit_count = 4;
    constexpr unsigned int c_bits_per_hex_digit = 4;

    const auto current = source.peek();
    if (!current.has_value())
    {
      return escape_result::failure(error::syntax_at(source.offset(), syntax_error_reason::unexpected_end));
    }

    if (*current != c_unicode_introducer)
    {
      return escape_result::success(std::nullopt);
    }

    // Keep original position until all four digits are validated.
    cursor candidate = source;
    candidate.advance(c_introducer_byte_count);
    char16_t unit{};

    for (std::size_t index{}; index < c_unicode_digit_count; ++index)
    {
      const auto decoded = decode_utf8(candidate);
      if (!decoded.has_value())
      {
        return escape_result::failure(*decoded.error_if());
      }

      const auto& character = *decoded.value_if();
      if (!character.has_value())
      {
        return escape_result::failure(error::syntax_at(candidate.offset(), syntax_error_reason::unexpected_end));
      }

      const auto digit = hex_digit_value(character->value);
      if (!digit.has_value())
      {
        return escape_result::failure(error::syntax_at(character->byte_offset, syntax_error_reason::invalid_escape));
      }

      // Four hexadecimal digits represent exactly one UTF-16 unit.
      unit = static_cast<char16_t>((unit << c_bits_per_hex_digit) | *digit);
    }

    source = candidate;
    return escape_result::success(std::optional<char16_t>{unit});
  }
} // namespace adaptive_json5
