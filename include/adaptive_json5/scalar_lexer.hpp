#ifndef SCALAR_LEXER_HPP_
#define SCALAR_LEXER_HPP_

#include "adaptive_json5/number_token.hpp"
#include "adaptive_json5/string_token.hpp"
#include "cursor.hpp"
#include "error.hpp"
#include "identifier_classification.hpp"
#include "identifier_lexer.hpp"
#include "number_lexer.hpp"
#include "options.hpp"
#include "punctuation.hpp"
#include "result.hpp"
#include "scalar_token.hpp"
#include "storage.hpp"
#include "string_lexer.hpp"
#include "token_boundary.hpp"

#include <cstddef>
#include <optional>
#include <utility>

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
    inline result<std::optional<scalar_token>> finish_scalar_token(cursor& source, cursor candidate, scalar_token token,
                                                                   syntax_mode mode) noexcept
    {
      using scalar_result = result<std::optional<scalar_token>>;

      const auto boundary = check_unquoted_token_boundary(candidate, mode);
      if (!boundary.has_value())
      {
        return scalar_result::failure(*boundary.error_if());
      }

      auto completed = scalar_result::success(std::optional<scalar_token>{std::move(token)});

      source = candidate;

      return completed;
    }
  } // namespace detail

  /**
   * @brief Recognize one string, number, null or Boolean value token.
   * @par Requirements
   * P028; J5-009; J5-023; J5-024; J5-041.
   * @param source Cursor at a possible scalar value, after caller-managed trivia.
   * @param mode Selected syntax.
   * @param storage Caller-owned fallible storage provider.
   * @param max_units Maximum decoded units for strings or temporary identifiers.
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
   * Container grammar, later trivia and complete document validation are separate.
   */
  inline result<std::optional<scalar_token>>
  consume_scalar(cursor& source, syntax_mode mode, const storage_callbacks& storage, std::size_t max_units) noexcept
  {
    using scalar_result = result<std::optional<scalar_token>>;

    const std::size_t start = source.offset();
    if (!source.peek().has_value())
    {
      return scalar_result::success(std::nullopt);
    }

    // Leave structural tokens available to the container parser.
    cursor punctuation_source = source;
    const auto punctuation = consume_punctuation(punctuation_source);
    if (punctuation.has_value())
    {
      return scalar_result::success(std::nullopt);
    }

    cursor candidate = source;

    // Delay public success until the following boundary also validates.
    auto string = consume_string(candidate, mode, storage, max_units);
    if (!string.has_value())
    {
      return scalar_result::failure(*string.error_if());
    }

    if (string.value_if()->has_value())
    {
      scalar_token token{std::in_place_type<string_token>, std::move(**string.value_if())};
      return detail::finish_scalar_token(source, candidate, std::move(token), mode);
    }

    auto number = detail::consume_number_spelling(candidate, mode);
    if (!number.has_value())
    {
      return scalar_result::failure(*number.error_if());
    }

    if (number.value_if()->has_value())
    {
      scalar_token token{std::in_place_type<number_token>, **number.value_if()};
      return detail::finish_scalar_token(source, candidate, std::move(token), mode);
    }

    auto identifier = consume_identifier(candidate, storage, max_units);
    if (!identifier.has_value())
    {
      return scalar_result::failure(*identifier.error_if());
    }

    if (!identifier.value_if()->has_value())
    {
      return scalar_result::failure(error::at_offset(error_code::syntax, start));
    }

    const auto& name = **identifier.value_if();
    const auto classification = classify_identifier(name, identifier_context::value, mode);
    if (!classification.has_value())
    {
      return scalar_result::failure(*classification.error_if());
    }

    literal_kind kind = literal_kind::null_literal;

    switch (*classification.value_if())
    {
    case identifier_classification::null_literal:
      kind = literal_kind::null_literal;
      break;
    case identifier_classification::true_literal:
      kind = literal_kind::true_literal;
      break;
    case identifier_classification::false_literal:
      kind = literal_kind::false_literal;
      break;

    default:
      return scalar_result::failure(error::at_offset(error_code::syntax, name.byte_offset));
    }

    const literal_token literal{kind, name.byte_offset, name.byte_length};
    scalar_token token{std::in_place_type<literal_token>, literal};

    return detail::finish_scalar_token(source, candidate, std::move(token), mode);
  }
} // namespace adaptive_json5

#endif /* SCALAR_LEXER_HPP_ */
