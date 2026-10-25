#ifndef SCALAR_LEXER_HPP_
#define SCALAR_LEXER_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/scalar_token.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Validate a scalar boundary and publish a complete token.
     * @par Requirements
     * P028; J5-024; J5-041.
     * @param source Caller cursor updated only after complete success.
     * @param candidate Private cursor immediately after the spelling.
     * @param token Recognized scalar, transferred into the returned result.
     * @param mode Selected syntax.
     * @note Failure releases any owned token storage without advancing source.
     * No warning observer is invoked.
     */
    [[nodiscard]] result<std::optional<scalar_token>>
    finish_scalar_token(cursor& source, cursor candidate, scalar_token token, syntax_mode mode) noexcept;
  } // namespace detail

  /**
   * @brief Recognize a primitive token with independent component limits.
   * @par Requirements
   * P028; P032; J5-009; J5-023; J5-024; J5-041; J5-047.
   * @param source Cursor at a possible scalar value, after caller-managed trivia.
   * @param mode Selected syntax.
   * @param storage Caller-owned fallible storage provider.
   * @param max_string_units Maximum decoded units in a string token.
   * @param max_identifier_units Maximum temporary decoded identifier units.
   * @param max_exponent_digits Maximum decimal exponent digits.
   * @return A scalar token, successful nullopt at EOF or structural punctuation,
   * or an explicit located error.
   * @note No-match and failure leave source unchanged.
   * Unknown or escaped value names fail instead of becoming string values.
   * Does not skip trivia or join spellings across trivia.
   * Number/literal results contain source metadata without owning input.
   * String results own decoded storage and retain their warning metadata.
   * No warning callbacks are exposed or invoked by this operation.
   * String storage and temporary identifier recognition may allocate.
   * The storage provider state must outlive returned owned string content.
   * Temporary identifier units do not consume the string-content limit.
   * Container grammar, later trivia and complete document validation are separate.
   */
  [[nodiscard]] result<std::optional<scalar_token>>
  consume_scalar(cursor& source, syntax_mode mode, const storage_callbacks& storage, std::size_t max_string_units,
                 std::size_t max_identifier_units, std::size_t max_exponent_digits) noexcept;

  /**
   * @brief Recognize a primitive using the original shared decoded-unit limit.
   * @par Requirements
   * P028; J5-009; J5-023; J5-024; J5-041.
   * @param source Cursor at a possible scalar value, after caller-managed trivia.
   * @param mode Selected syntax.
   * @param storage Caller-owned fallible storage provider.
   * @param max_units Maximum decoded units for strings or temporary identifiers.
   * @return The independently limited overload's recognition result.
   * @note Preserves the original four-argument call behavior.
   * Permits any exponent digit count representable by size_t.
   */
  [[nodiscard]] result<std::optional<scalar_token>>
  consume_scalar(cursor& source, syntax_mode mode, const storage_callbacks& storage, std::size_t max_units) noexcept;
} // namespace adaptive_json5

#endif /* SCALAR_LEXER_HPP_ */
