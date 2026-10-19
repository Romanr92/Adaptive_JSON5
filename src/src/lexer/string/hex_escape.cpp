// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/hex_escape.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  std::optional<char16_t> hex_digit_value(char32_t character) noexcept
  {
    constexpr char32_t c_decimal_min = U'0';
    constexpr char32_t c_decimal_max = U'9';
    constexpr char32_t c_lower_min = U'a';
    constexpr char32_t c_lower_max = U'f';
    constexpr char32_t c_upper_min = U'A';
    constexpr char32_t c_upper_max = U'F';
    constexpr char32_t c_letter_value_offset = 10;

    if ((character >= c_decimal_min) && (character <= c_decimal_max))
    {
      return static_cast<char16_t>(character - c_decimal_min);
    }

    if ((character >= c_lower_min) && (character <= c_lower_max))
    {
      return static_cast<char16_t>(character - c_lower_min + c_letter_value_offset);
    }

    if ((character >= c_upper_min) && (character <= c_upper_max))
    {
      return static_cast<char16_t>(character - c_upper_min + c_letter_value_offset);
    }

    return std::nullopt;
  }

  result<std::optional<char16_t>> consume_hex_escape(cursor& source, syntax_mode mode) noexcept
  {
    using escape_result = result<std::optional<char16_t>>;

    constexpr char c_hex_introducer = 'x';
    constexpr std::size_t c_introducer_byte_count = 1;
    constexpr std::size_t c_hex_digit_count = 2;
    constexpr unsigned int c_bits_per_hex_digit = 4;

    const auto current = source.peek();
    if (!current.has_value())
    {
      return escape_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    if (*current != c_hex_introducer)
    {
      return escape_result::success(std::nullopt);
    }

    if (mode == syntax_mode::strict_json)
    {
      return escape_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    // Keep the original position until the entire escape is validated.
    cursor candidate = source;
    candidate.advance(c_introducer_byte_count);
    char16_t unit{};

    for (std::size_t index{}; index < c_hex_digit_count; ++index)
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

      // Two hexadecimal digits fit completely within one UTF-16 unit.
      unit = static_cast<char16_t>((unit << c_bits_per_hex_digit) | *digit);
    }

    source = candidate;
    return escape_result::success(std::optional<char16_t>{unit});
  }
} // namespace adaptive_json5
