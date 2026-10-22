#include "adaptive_json5/detail/lexer/string/string_warning.hpp"

#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"
#include "adaptive_json5/options.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Describe a warning for a raw JSON5 string separator, if present.
   * @par Requirements
   * P020; J5-017.
   * @param piece An original source piece returned by consume_string_piece.
   * @return Warning for a raw JSON5 U+2028/U+2029 character, otherwise nullopt.
   * @note Does not change content, cursor state or parsing success.
   * Call only for source pieces, never reconstructed decoded escape values.
   * Strict JSON receives no JSON5 separator advisory.
   * No allocation or callback is performed.
   */
  std::optional<string_warning> raw_separator_warning_for(const string_piece& piece, syntax_mode mode) noexcept
  {
    constexpr char32_t c_line_separator = 0x2028;
    constexpr char32_t c_paragraph_separator = 0x2029;

    if ((mode != syntax_mode::json5) || (piece.kind != string_piece_kind::character))
    {
      return std::nullopt;
    }

    string_warning_code code;

    if (piece.scalar == c_line_separator)
    {
      code = string_warning_code::raw_line_separator;
    }
    else if (piece.scalar == c_paragraph_separator)
    {
      code = string_warning_code::raw_paragraph_separator;
    }
    else
    {
      return std::nullopt;
    }

    return string_warning{code, piece.byte_offset, piece.byte_length};
  }
} // namespace adaptive_json5
