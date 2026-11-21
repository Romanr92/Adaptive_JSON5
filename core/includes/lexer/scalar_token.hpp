#ifndef ADAPTIVE_JSON5_INTERNAL_LEXER_SCALAR_TOKEN_HPP_
#define ADAPTIVE_JSON5_INTERNAL_LEXER_SCALAR_TOKEN_HPP_

#include "lexer/number/number_token.hpp"
#include "lexer/string/string_token.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>

namespace adaptive_json5
{
  /**
   * @brief Identify an exact null or Boolean literal.
   * @par Requirements
   * P024; P028; J5-023.
   */
  enum class literal_kind : std::uint8_t
  {
    null_literal,
    true_literal,
    false_literal
  };

  /**
   * @brief Describe a recognized primitive literal in original input bytes.
   * @par Requirements
   * P028; J5-023; J5-024; J5-041.
   * @note Contains source metadata only and requires no allocation.
   * The spelling must be exact, lowercase and unescaped.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.01)
  struct [[nodiscard]] literal_token
  {
      literal_kind kind = literal_kind::null_literal;
      // MISRA rule: RULE-0-2-1 cpp/misra/unused-limited-visibility-variable
      // MISRA justification: Part of the span result returned to callers, so the result describes the complete
      // recognized range. Unit tests read the member to verify the range; production callers that only need
      // success ignore it.
      // codeql::misra_deviation_next_line(DEV-008.05)
      std::size_t byte_offset{};
      std::size_t byte_length{};
  };

  /**
   * @brief Hold one recognized string, number or primitive literal token.
   * @par Requirements
   * P028; J5-023; J5-024.
   * @note Exactly one alternative is active.
   * String tokens own decoded storage; their provider state must outlive them.
   * Number and literal tokens contain source metadata without owning input.
   * This is a lexical token type, not the P030 owned value/container model.
   * Copying is unavailable because string_token contains move-only ownership.
   * Transfer tokens by move and inspect alternatives through std::get_if.
   */
  using scalar_token = std::variant<string_token, number_token, literal_token>;

  /**
   * @brief Describe one recognized string, number or literal without owning anything.
   * @par Requirements
   * P028; J5-023; J5-024; J5-046; J5-047.
   * @note Exactly one alternative is active.
   * Number and literal tokens already own nothing, so they are reused unchanged;
   * a string is described by its measurement instead of its decoded units.
   * Copyable, because no alternative owns storage.
   */
  using scalar_measure = std::variant<string_measure, number_token, literal_token>;

} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_INTERNAL_LEXER_SCALAR_TOKEN_HPP_ */
