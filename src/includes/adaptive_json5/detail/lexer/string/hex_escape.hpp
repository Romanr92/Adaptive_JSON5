#ifndef HEX_ESCAPE_HPP_
#define HEX_ESCAPE_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Convert an ASCII hexadecimal character to its numeric value.
   * @par Requirements
   * P016; J5-016.
   * @return A value from zero through fifteen, or nullopt for nonhex input.
   * @note Classification is independent of locale.
   */
  [[nodiscard]] std::optional<char16_t> hex_digit_value(char32_t character) noexcept;

  /**
   * @brief Decode a JSON5 hexadecimal escape after its backslash.
   * @par Requirements
   * P016; J5-016.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return A UTF-16 unit, successful nullopt before a non-x byte,
   * or a located syntax/encoding error.
   * @note A match consumes x and exactly two ASCII hexadecimal digits.
   * No-match and failure leave source unchanged.
   * Strict JSON rejects x at its original offset.
   * Missing digits report EOF; nonhex characters report their starting offset.
   * Nonmatching input is not validated. No allocation is performed.
   */
  [[nodiscard]] result<std::optional<char16_t>> consume_hex_escape(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* HEX_ESCAPE_HPP_ */
