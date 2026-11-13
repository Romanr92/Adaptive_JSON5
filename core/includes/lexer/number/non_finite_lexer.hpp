#ifndef NON_FINITE_LEXER_HPP_
#define NON_FINITE_LEXER_HPP_

#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/number/number_token.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Recognize an exact Infinity or NaN numeric spelling.
   * @par Requirements
   * P027; J5-009; J5-021; J5-022; J5-041; J5-043; issue #125.
   * @param source Cursor positioned at a possible non-finite value.
   * @param mode Selected input syntax.
   * @return Source metadata, successful nullopt before unrelated input or for an
   * unsigned unknown word, or an explicit located error.
   * @note Accepts one optional leading plus or minus.
   * After the sign, only uppercase I or N starts a recognition attempt.
   * Other starts return no-match without validating the remaining input.
   * An unsigned word that is not exactly Infinity or NaN (a spelling mismatch, EOF inside
   * the spelling, or an identifier continuation or backslash after it) returns no-match,
   * so the scalar lexer rejects the whole word as unknown_literal with a source_range,
   * like nul or trueish.
   * A signed word reports unknown_literal at the offending character, or unexpected_end
   * at EOF, without a range.
   * A decimal point after the name fails with unknown_literal at the point.
   * Strict JSON rejects only a complete Infinity or NaN, at its original start; other
   * words are unknown in both modes.
   * Malformed UTF-8 encountered during recognition reports an encoding error.
   * No-match and failure leave source unchanged.
   * Does not skip trivia, allocate, convert or own source text.
   * Use in value context; member names use identifier recognition instead.
   * Other suffix/context validation remains P028 work.
   * @par ID SWU-LEXER-018
   */
  [[nodiscard]] result<std::optional<number_token>> consume_non_finite(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* NON_FINITE_LEXER_HPP_ */
