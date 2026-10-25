#ifndef SIMPLE_ESCAPE_HPP_
#define SIMPLE_ESCAPE_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Decode one simple escape after its backslash.
   * @par Requirements
   * P015; J5-015; J5-016.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return One UTF-16 unit, successful nullopt for another escape form,
   * or a located syntax error.
   * @note Success with a unit consumes one ASCII byte.
   * No-match and failure leave source unchanged.
   * Does not validate or consume bytes belonging to other escape forms.
   * A JSON5 zero escape fails if followed by any decimal digit.
   * EOF after an accepted zero still requires the enclosing string to close.
   * No allocation is performed.
   */
  [[nodiscard]] result<std::optional<char16_t>> consume_simple_escape(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* SIMPLE_ESCAPE_HPP_ */
