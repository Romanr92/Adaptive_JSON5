#ifndef STRING_ESCAPE_HPP_
#define STRING_ESCAPE_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/string/hex_escape.hpp"
#include "adaptive_json5/detail/lexer/string/line_continuation.hpp"
#include "adaptive_json5/detail/lexer/string/non_escape.hpp"
#include "adaptive_json5/detail/lexer/string/simple_escape.hpp"
#include "adaptive_json5/detail/lexer/string/unicode_escape.hpp"
#include "adaptive_json5/detail/unicode/utf16.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Dispatch escape forms that produce exactly one UTF-16 unit.
     * @par Requirements
     * P021; J5-015; J5-016; J5-018.
     * @note x and u always go to their owning decoders.
     * Their failures must never fall back to non-escape characters.
     */
    result<std::optional<char16_t>> consume_unit_escape(cursor& source, syntax_mode mode) noexcept;
  } // namespace detail

  /**
   * @brief Decode one complete escape after its backslash.
   * @par Requirements
   * P021; J5-015; J5-016; J5-017; J5-018.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return Zero, one or two decoded UTF-16 units, or a located error.
   * @note Zero units represent a consumed line continuation.
   * Escaped surrogate units are preserved without scalar conversion.
   * Success consumes the complete escape suffix.
   * Failure leaves source unchanged.
   * No allocation is performed.
   */
  result<encoded_utf16> consume_string_escape(cursor& source, syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* STRING_ESCAPE_HPP_ */
