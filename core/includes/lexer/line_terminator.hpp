#ifndef LINE_TERMINATOR_HPP_
#define LINE_TERMINATOR_HPP_

#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Original byte span of one line-terminator sequence.
   * @par Requirements
   * P007; J5-011; J5-017; J5-044.
   * @note CRLF occupies two bytes but represents one sequence.
   */
  struct [[nodiscard]] line_terminator_span
  {
      // MISRA rule: RULE-0-2-1 cpp/misra/unused-limited-visibility-variable
      // MISRA justification: Part of the span result returned to callers, so the result describes the complete
      // recognized range. Unit tests read the member to verify the range; production callers that only need
      // success ignore it.
      // codeql::misra_deviation_next_line(DEV-008.04)
      std::size_t byte_offset;
      // MISRA rule: RULE-0-2-1 cpp/misra/unused-limited-visibility-variable
      // MISRA justification: Part of the span result returned to callers, so the result describes the complete
      // recognized range. Unit tests read the member to verify the range; production callers that only need
      // success ignore it.
      // codeql::misra_deviation_next_line(DEV-008.03)
      std::size_t byte_length;
  };

  /**
   * @brief Consume one line-terminator sequence if present.
   * @par Requirements
   * P007; J5-011; J5-017; J5-044.
   * @param source Cursor over borrowed UTF-8 input.
   * @return A terminator span, successful std::nullopt when none is
   * present, or an explicit encoding error.
   * @note EOF, nonterminators and encoding failures leave source unchanged.
   * Recognizes LF, CR, CRLF, U+2028 and U+2029 without allocation.
   * @par ID SWU-LEXER-010
   */
  [[nodiscard]] result<std::optional<line_terminator_span>> consume_line_terminator(cursor& source) noexcept;
} // namespace adaptive_json5

#endif /* LINE_TERMINATOR_HPP_ */
