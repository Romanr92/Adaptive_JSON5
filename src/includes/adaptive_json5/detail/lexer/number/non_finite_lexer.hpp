#ifndef NON_FINITE_LEXER_HPP_
#define NON_FINITE_LEXER_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Recognize an exact Infinity or NaN numeric spelling.
   * @par Requirements
   * P027; J5-009; J5-021; J5-022; J5-041.
   * @param source Cursor positioned at a possible non-finite value.
   * @param mode Selected input syntax.
   * @return Source metadata, successful nullopt before unrelated input,
   * or an explicit located error.
   * @note Accepts one optional leading plus or minus.
   * After the sign, only uppercase I or N starts a recognition attempt.
   * Other starts return no-match without validating the remaining input.
   * Strict JSON rejects an attempted non-finite token at its original start.
   * JSON5 spelling mismatches report the offending character or EOF.
   * Identifier continuations, backslashes and decimal points after the name fail.
   * Malformed UTF-8 encountered during recognition reports an encoding error.
   * No-match and failure leave source unchanged.
   * Does not skip trivia, allocate, convert or own source text.
   * Use in value context; member names use identifier recognition instead.
   * Other suffix/context validation remains P028 work.
   */
  [[nodiscard]] result<std::optional<number_token>> consume_non_finite(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* NON_FINITE_LEXER_HPP_ */
