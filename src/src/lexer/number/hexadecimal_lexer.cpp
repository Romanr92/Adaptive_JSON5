// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/number/hexadecimal_lexer.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/detail/lexer/string/hex_escape.hpp"
#include "adaptive_json5/detail/unicode/unicode.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  result<std::optional<number_token>> consume_hexadecimal(cursor& source, syntax_mode mode) noexcept
  {
    using hexadecimal_result = result<std::optional<number_token>>;

    constexpr std::size_t c_ascii_byte_count = 1;
    constexpr std::size_t c_prefix_byte_count = 2;

    const std::size_t start = source.offset();

    // Keep optional sign consumption private until the complete operation succeeds.
    cursor candidate = source;
    const auto initial = candidate.peek();

    if (initial.has_value() && ((*initial == '+') || (*initial == '-')))
    {
      candidate.advance(c_ascii_byte_count);
    }

    const auto zero = candidate.peek();
    const auto marker = candidate.peek(c_ascii_byte_count);

    if (!zero.has_value() || (*zero != '0'))
    {
      return hexadecimal_result::success(std::nullopt);
    }

    if (!marker.has_value() || ((*marker != 'x') && (*marker != 'X')))
    {
      return hexadecimal_result::success(std::nullopt);
    }

    if (mode == syntax_mode::strict_json)
    {
      return hexadecimal_result::failure(error::at_offset(error_code::syntax, start));
    }

    candidate.advance(c_prefix_byte_count);
    bool has_digits = false;

    while (true)
    {
      // Decode through a copy so an admitted terminator remains unconsumed.
      cursor lookahead = candidate;
      const auto decoded = decode_utf8(lookahead);

      if (!decoded.has_value())
      {
        return hexadecimal_result::failure(*decoded.error_if());
      }

      const auto& character = *decoded.value_if();

      if (!character.has_value())
      {
        if (!has_digits)
        {
          return hexadecimal_result::failure(error::at_offset(error_code::syntax, candidate.offset()));
        }

        break;
      }

      const auto digit = hex_digit_value(character->value);

      if (digit.has_value())
      {
        // Recognize spelling without accumulating a potentially overflowing value.
        candidate = lookahead;
        has_digits = true;
      }
      else
      {
        if (!has_digits)
        {
          return hexadecimal_result::failure(error::at_offset(error_code::syntax, character->byte_offset));
        }

        const char32_t scalar = character->value;

        // These suffixes cannot turn a hexadecimal prefix into a valid number.
        if (is_identifier_part(scalar) || (scalar == U'\\') || (scalar == U'.'))
        {
          return hexadecimal_result::failure(error::at_offset(error_code::syntax, character->byte_offset));
        }

        break;
      }
    }

    const number_token token{number_kind::hexadecimal, start, candidate.offset() - start};
    auto completed = hexadecimal_result::success(std::optional<number_token>{token});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5
