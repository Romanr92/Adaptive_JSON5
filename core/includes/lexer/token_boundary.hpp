#ifndef TOKEN_BOUNDARY_HPP_
#define TOKEN_BOUNDARY_HPP_

#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"

namespace adaptive_json5
{
  /**
   * @brief Check the immediate boundary after an unquoted token.
   * @par Requirements
   * P028; J5-009; J5-010; J5-011; J5-012; J5-024.
   * @param source Copy of the cursor immediately after the token spelling.
   * @param mode Selected input syntax.
   * @return Success for EOF, admitted whitespace, structural punctuation,
   * or a JSON5 comment opener; otherwise a located error.
   * @note Never changes the caller's cursor and requires no allocation.
   * Checks only the immediate boundary, not the remaining document.
   * Comment contents and later trivia require normal trivia processing.
   * Structural punctuation still requires grammatical context validation.
   * Intended for numbers, primitive literals and unquoted member names.
   */
  [[nodiscard]] status_result check_unquoted_token_boundary(cursor source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* TOKEN_BOUNDARY_HPP_ */
