#ifndef STRING_WARNING_HPP_
#define STRING_WARNING_HPP_

#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
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
  std::optional<string_warning> raw_separator_warning_for(const string_piece& piece, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* STRING_WARNING_HPP_ */
