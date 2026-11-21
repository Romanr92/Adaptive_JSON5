#ifndef ADAPTIVE_JSON5_TEXT_POSITION_HPP_
#define ADAPTIVE_JSON5_TEXT_POSITION_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Describe a human-readable position in the original input.
   * @par Requirements
   * P039; J5-044.
   * @note Lines and columns are 1-based; byte offsets elsewhere remain 0-based.
   * Columns count Unicode code points; each invalid UTF-8 byte counts as one column.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.19)
  struct [[nodiscard]] text_position
  {
      std::size_t line{1};
      std::size_t column{1};
  };

  /**
   * @brief Convert a zero-based byte offset into a line and column.
   * @par Requirements
   * P039; J5-003; J5-044; J5-052.
   * @param input Original bytes the offset refers to.
   * @param byte_offset Zero-based offset; input.size denotes the end of input.
   * @return Position of the character at byte_offset, or wrong_type_or_access (invalid_argument) when it
   * exceeds input.size.
   * @note LF, CR, CRLF, U+2028 and U+2029 end a line; CRLF counts once.
   * An offset inside a multibyte character or inside CRLF reports where that sequence starts.
   * Scans the input once up to byte_offset; performs no allocation or locale-dependent operation.
   * @par ID SWU-INPUT-011
   */
  [[nodiscard]] result<text_position> locate(input_view input, std::size_t byte_offset) noexcept;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_TEXT_POSITION_HPP_ */
