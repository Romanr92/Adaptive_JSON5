// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/string_warning.hpp"

#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"
#include "adaptive_json5/options.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
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
