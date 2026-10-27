#ifndef IDENTIFIER_LEXER_HPP_
#define IDENTIFIER_LEXER_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/identifier/identifier_token.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Validate one identifier and count its units without allocating.
   * @par Requirements
   * P022; P023; J5-019; J5-020; J5-041; J5-046; J5-047.
   * @param source Cursor positioned at a possible identifier start.
   * @param max_units Maximum decoded UTF-16 unit count.
   * @return The measurement, successful nullopt for no-match,
   * or the same located failure consume_identifier reports for the same input.
   * @note No-match and failure leave source unchanged.
   * Performs no allocation (ADR 0003, decisions 2 and 7).
   * Recognition is independent of key/value context and syntax mode.
   */
  [[nodiscard]] result<std::optional<identifier_measure>> measure_identifier(cursor& source,
                                                                             std::size_t max_units) noexcept;

  /**
   * @brief Consume an identifier with ordinary UTF-8 characters or Unicode escapes.
   * @par Requirements
   * P022; P023; J5-019; J5-020; J5-041; J5-046.
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
   * Counts units first, then makes one allocation of exactly that size (ADR 0003, decision 2).
   * The storage provider state must outlive a returned token.
   */
  [[nodiscard]] result<std::optional<identifier_token>>
  consume_identifier(cursor& source, const storage_callbacks& storage, std::size_t max_units) noexcept;
} // namespace adaptive_json5

#endif /* IDENTIFIER_LEXER_HPP_ */
