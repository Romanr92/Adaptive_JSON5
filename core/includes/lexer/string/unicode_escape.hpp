#ifndef UNICODE_ESCAPE_HPP_
#define UNICODE_ESCAPE_HPP_

#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Decode one Unicode escape after its backslash.
   * @par Requirements
   * P017; J5-015; J5-018.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return One UTF-16 unit, successful nullopt before a non-u byte,
   * or a located syntax/encoding error.
   * @note Consumes u and exactly four ASCII hexadecimal digits.
   * Valid in both syntax modes; surrogate units are preserved unchanged.
   * No-match and failure leave source unchanged.
   * Missing digits report EOF; nonhex characters report their starting offset.
   * Nonmatching input is not validated. No allocation is performed.
   */
  [[nodiscard]] result<std::optional<char16_t>> consume_unicode_escape(cursor& source) noexcept;
} // namespace adaptive_json5

#endif /* UNICODE_ESCAPE_HPP_ */
