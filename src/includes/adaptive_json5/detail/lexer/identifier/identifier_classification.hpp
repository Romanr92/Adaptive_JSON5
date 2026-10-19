#ifndef IDENTIFIER_CLASSIFICATION_HPP_
#define IDENTIFIER_CLASSIFICATION_HPP_

#include "adaptive_json5/detail/lexer/identifier/identifier_token.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>
#include <string_view>

namespace adaptive_json5
{
  /**
   * @brief Identify the grammatical use of an identifier token.
   * @par Requirements
   * P024; J5-019; J5-023.
   */
  enum class identifier_context
  {
    member_name,
    value
  };

  /**
   * @brief Describe an identifier's meaning in the selected context.
   * @par Requirements
   * P024; J5-023.
   * @note unrecognized_value does not represent an accepted string value.
   * Numeric recognition and final rejection belong to the token dispatcher.
   */
  enum class identifier_classification
  {
    member_name,
    null_literal,
    true_literal,
    false_literal,
    unrecognized_value
  };

  namespace detail
  {
    /**
     * @brief Compare decoded content with an exact UTF-16 spelling.
     * @par Requirements
     * P024; J5-023.
     * @note Performs no allocation, normalization or case folding.
     * Checked-access failures are propagated explicitly.
     */
    result<bool> identifier_spelling_matches(const decoded_string& value, std::u16string_view spelling) noexcept;
  } // namespace detail

  /**
   * @brief Classify an identifier as a member name or primitive literal.
   * @par Requirements
   * P024; J5-019; J5-023.
   * @param token Complete token produced by identifier recognition.
   * @param context Whether a member name or value is expected.
   * @param mode Selected input syntax.
   * @return Classification or an explicit error.
   * @note Strict JSON rejects unquoted member names at the token start.
   * Value literals require exact lowercase spelling without escapes.
   * Other value spellings return unrecognized_value, not a string value.
   * Input, token ownership and cursor position are unchanged.
   * Performs no allocation.
   */
  result<identifier_classification> classify_identifier(const identifier_token& token, identifier_context context,
                                                        syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* IDENTIFIER_CLASSIFICATION_HPP_ */
