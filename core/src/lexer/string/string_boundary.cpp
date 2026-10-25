#include "lexer/string/string_boundary.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Consume a permitted opening quote if present.
   * @par Requirements
   * P014; J5-014.
   * @return The delimiter, successful nullopt before nonquote input or EOF,
   * or syntax error for a single quote in strict JSON.
   * @note Failure and no-match leave the cursor unchanged.
   * Does not skip trivia or validate nonquote input.
   */
  result<std::optional<string_quote>> consume_string_opening(cursor& source, syntax_mode mode) noexcept
  {
    using opening_result = result<std::optional<string_quote>>;
    constexpr std::size_t c_quote_byte_count = 1;

    const auto current = source.peek();
    if (!current.has_value())
    {
      return opening_result::success(std::nullopt);
    }

    string_quote quote;
    if (*current == '"')
    {
      quote = string_quote::double_quote;
    }
    else if (*current == '\'')
    {
      if (mode == syntax_mode::strict_json)
      {
        return opening_result::failure(error::at_offset(error_code::syntax, source.offset()));
      }

      quote = string_quote::single;
    }
    else
    {
      return opening_result::success(std::nullopt);
    }

    source.advance(c_quote_byte_count);
    return opening_result::success(std::optional<string_quote>{quote});
  }

  /**
   * @brief Consume one ordinary character, closing quote or escape introducer.
   * @par Requirements
   * P014; J5-014.
   * @param quote Delimiter returned by consume_string_opening in the same mode.
   * @return One source piece, an encoding error, or a located syntax error.
   * EOF reports syntax at the current end offset.
   * @note Call only inside an open string, outside an escape sequence.
   * Success consumes exactly one source character; failure consumes nothing.
   * escape_start consumes the backslash only: the caller must decode its
   * following escape before calling this function again.
   * Raw U+2028/U+2029 are characters; warning handling belongs to P020.
   * Whole-string rollback belongs to the later composing lexer.
   */
  result<string_piece> consume_string_piece(cursor& source, string_quote quote, syntax_mode mode) noexcept
  {
    constexpr char32_t c_reverse_solidus = U'\\';
    constexpr char32_t c_carriage_return = U'\r';
    constexpr char32_t c_line_feed = U'\n';
    constexpr char32_t c_control_max = 0x1F;

    // Validate through a copy so a rejected character consumes no input.
    cursor candidate = source;
    const auto decoded = decode_utf8(candidate);
    if (!decoded.has_value())
    {
      return result<string_piece>::failure(*decoded.error_if());
    }

    const auto& character = *decoded.value_if();
    if (!character.has_value())
    {
      return result<string_piece>::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    const char32_t scalar = character->value;
    string_piece_kind kind = string_piece_kind::character;

    if (scalar == static_cast<char32_t>(quote))
    {
      kind = string_piece_kind::closing_quote;
    }
    else if (scalar == c_reverse_solidus)
    {
      kind = string_piece_kind::escape_start;
    }
    else if ((scalar == c_carriage_return) || (scalar == c_line_feed) ||
             ((mode == syntax_mode::strict_json) && (scalar <= c_control_max)))
    {
      return result<string_piece>::failure(error::at_offset(error_code::syntax, character->byte_offset));
    }

    source = candidate;
    return result<string_piece>::success(string_piece{kind, scalar, character->byte_offset, character->byte_length});
  }
} // namespace adaptive_json5
