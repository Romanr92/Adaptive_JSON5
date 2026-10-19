// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/unicode_escape.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/string/hex_escape.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
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
      return escape_result::failure(error::at_offset(error_code::syntax, source.offset()));
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
        return escape_result::failure(error::at_offset(error_code::syntax, candidate.offset()));
      }

      const auto digit = hex_digit_value(character->value);
      if (!digit.has_value())
      {
        return escape_result::failure(error::at_offset(error_code::syntax, character->byte_offset));
      }

      // Four hexadecimal digits represent exactly one UTF-16 unit.
      unit = static_cast<char16_t>((unit << c_bits_per_hex_digit) | *digit);
    }

    source = candidate;
    return escape_result::success(std::optional<char16_t>{unit});
  }
} // namespace adaptive_json5
