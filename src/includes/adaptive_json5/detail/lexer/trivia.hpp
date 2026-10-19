#ifndef ADAPTIVE_JSON5_TRIVIA_HPP_
#define ADAPTIVE_JSON5_TRIVIA_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/comment.hpp"
#include "adaptive_json5/detail/lexer/whitespace.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Original byte range consumed as trivia.
   * @par Requirements
   * P011; J5-012; J5-024.
   * @note Describes borrowed input without owning its bytes.
   * An empty run has zero byte length.
   */
  struct trivia_span
  {
      std::size_t byte_offset;
      std::size_t byte_length;
  };

  /**
   * @brief Consume whitespace admitted by strict JSON.
   * @par Requirements
   * P011; J5-010.
   * @param source Cursor positioned before possible whitespace.
   * @return Success, or an explicit encoding error.
   * @note Admits only space, horizontal tab, carriage return and line feed.
   * Stops before the first other valid character.
   * Encoding failure leaves source unchanged.
   * Requires no allocation and throws no exceptions.
   */
  status_result skip_strict_json_whitespace(cursor& source) noexcept;

  /**
   * @brief Consume consecutive whitespace and comments admitted by the mode.
   * @par Requirements
   * P011; J5-010; J5-012; J5-024.
   * @param source Cursor positioned between tokens.
   * @param mode Selected input language.
   * @return The consumed trivia span, or an explicit encoding or syntax error.
   * @note Strict JSON rejects comment openers at their opening slash.
   * Stops before the first nontrivia character.
   * Failure leaves source unchanged, including preceding trivia in this run.
   * Requires no allocation and throws no exceptions.
   * Call between tokens, never inside a token spelling.
   */
  result<trivia_span> skip_trivia(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_TRIVIA_HPP_ */
