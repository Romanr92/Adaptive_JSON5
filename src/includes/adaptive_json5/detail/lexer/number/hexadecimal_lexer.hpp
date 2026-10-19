#ifndef HEXADECIMAL_LEXER_HPP_
#define HEXADECIMAL_LEXER_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Recognize a hexadecimal numeric spelling without conversion.
   * @par Requirements
   * P026; J5-009; J5-021; J5-022; J5-041.
   * @param source Cursor positioned at a possible hexadecimal number.
   * @param mode Selected input syntax.
   * @return Source metadata, successful nullopt before another spelling,
   * or an explicit located error.
   * @note No-match and failure leave source unchanged.
   * Accepts one optional sign, 0x or 0X, and at least one hex digit.
   * Strict JSON rejects a recognized hexadecimal prefix at token start.
   * Missing digits report the next character's offset or EOF.
   * Identifier continuations, backslashes and decimal points after digits fail.
   * Malformed UTF-8 encountered during scanning reports an encoding error.
   * Other suffix/context validation remains P028 work.
   * Does not skip trivia, allocate, convert or own source text.
   */
  result<std::optional<number_token>> consume_hexadecimal(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* HEXADECIMAL_LEXER_HPP_ */
