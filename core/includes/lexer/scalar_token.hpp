#ifndef SCALAR_TOKEN_HPP_
#define SCALAR_TOKEN_HPP_

#include "lexer/number/number_token.hpp"
#include "lexer/string/string_token.hpp"

#include <cstddef>
#include <variant>

namespace adaptive_json5
{
  /**
   * @brief Identify an exact null or Boolean literal.
   * @par Requirements
   * P024; P028; J5-023.
   */
  enum class literal_kind
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
  struct [[nodiscard]] literal_token
  {
      literal_kind kind = literal_kind::null_literal;
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
} // namespace adaptive_json5

#endif /* SCALAR_TOKEN_HPP_ */
