#ifndef IDENTIFIER_LEXER_HPP_
#define IDENTIFIER_LEXER_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/identifier/identifier_token.hpp"
#include "adaptive_json5/detail/lexer/string/unicode_escape.hpp"
#include "adaptive_json5/detail/memory/decoded_string_builder.hpp"
#include "adaptive_json5/detail/unicode/unicode.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Consume an identifier with ordinary UTF-8 characters or Unicode escapes.
   * @par Requirements
   * P022; P023; J5-019; J5-020; J5-041.
   * @param source Cursor positioned at a possible identifier start.
   * @param storage Caller-provided storage for decoded UTF-16 units.
   * @param max_units Maximum decoded UTF-16 unit count.
   * @return An owned token, successful nullopt for no-match,
   * or an explicit located error.
   * @note Only backslash-u followed by four hexadecimal digits is an escape.
   * Decoded escapes must satisfy the current start/part rule.
   * Illegal escaped characters and unsupported escape forms report the backslash offset.
   * Malformed Unicode escapes retain the decoder's error location.
   * No-match and failure leave source unchanged.
   * Recognition is independent of key/value context and syntax mode.
   * Strict JSON member-name rejection belongs to the context-aware caller.
   * The storage provider state must outlive a returned token.
   */
  result<std::optional<identifier_token>> consume_identifier(cursor& source, const storage_callbacks& storage,
                                                             std::size_t max_units) noexcept;
} // namespace adaptive_json5

#endif /* IDENTIFIER_LEXER_HPP_ */
