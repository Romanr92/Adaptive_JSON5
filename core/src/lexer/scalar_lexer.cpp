#include "lexer/scalar_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/identifier/identifier_lexer.hpp"
#include "lexer/number/number_lexer.hpp"
#include "lexer/number/number_token.hpp"
#include "lexer/punctuation.hpp"
#include "lexer/scalar_token.hpp"
#include "lexer/string/string_lexer.hpp"
#include "lexer/string/string_token.hpp"
#include "lexer/token_boundary.hpp"
#include "unicode/unicode.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
  using adaptive_json5::literal_kind;

  /**
   * @brief Pair one value-literal spelling with its token kind.
   * @par Requirements
   * P028; J5-023; J5-046.
   */
  struct literal_spelling
  {
      std::string_view text;
      literal_kind kind;
  };

  constexpr literal_spelling c_literal_spelling[] = {
    {"null", literal_kind::null_literal}, {"true", literal_kind::true_literal}, {"false", literal_kind::false_literal}};

  /**
   * @brief Report whether the character after a spelling could extend an identifier.
   * @par Requirements
   * P028; J5-020; J5-046.
   * @param after Copy of the cursor immediately after the matched spelling.
   * @return True for a backslash, an identifier-part character or malformed UTF-8.
   * @note Performs no allocation. True means "not provably a literal", so the caller
   * defers to the identifier path, which reports today's exact error.
   */
  bool may_continue_identifier(adaptive_json5::cursor after) noexcept
  {
    const auto next = after.peek();
    if (!next.has_value())
    {
      return false;
    }

    // An escape could decode to an identifier part, as in true\u0041.
    if (*next == '\\')
    {
      return true;
    }

    const auto decoded = adaptive_json5::decode_utf8(after);
    // Let the identifier path report malformed UTF-8 with its usual location.
    if (!decoded.has_value())
    {
      return true;
    }

    const auto& character = *decoded.value_if();
    return (character.has_value() && adaptive_json5::is_identifier_part(character->value));
  }

  /**
   * @brief Recognize null, true or false by comparing source bytes.
   * @par Requirements
   * P028; J5-023; J5-046.
   * @param source Cursor at a possible value literal; not advanced.
   * @return The literal token when the bytes spell a complete literal, otherwise nullopt.
   * @note Never decodes into storage and never calls storage callbacks (ADR 0003, decision 3).
   */
  std::optional<adaptive_json5::literal_token> match_literal_bytes(const adaptive_json5::cursor& source) noexcept
  {
    for (const literal_spelling& literal : c_literal_spelling)
    {
      bool matches = true;
      for (std::size_t index{}; index < literal.text.size(); ++index)
      {
        const auto byte = source.peek(index);
        if ((!byte.has_value()) || (*byte != literal.text[index]))
        {
          matches = false;
          break;
        }
      }

      if (!matches)
      {
        continue;
      }

      adaptive_json5::cursor after = source;
      after.advance(literal.text.size());

      // Trueish is one unknown identifier, not true followed by junk.
      if (may_continue_identifier(after))
      {
        return std::nullopt;
      }

      return adaptive_json5::literal_token{literal.kind, source.offset(), literal.text.size()};
    }

    return std::nullopt;
  }
} // namespace

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
    result<std::optional<scalar_token>> finish_scalar_token(cursor& source, cursor candidate, scalar_token token,
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
   * @brief Recognize a primitive token with independent component limits.
   * @par Requirements
   * P028; P032; J5-009; J5-023; J5-024; J5-041; J5-046; J5-047.
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
   * Literals never allocate; strings and the rejection of other identifiers may allocate.
   * The storage provider state must outlive returned owned string content.
   * Temporary identifier units do not consume the string-content limit.
   * Container grammar, later trivia and complete document validation are separate.
   */
  result<std::optional<scalar_token>> consume_scalar(cursor& source, syntax_mode mode, const storage_callbacks& storage,
                                                     std::size_t max_string_units, std::size_t max_identifier_units,
                                                     std::size_t max_exponent_digits) noexcept
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
    auto string = consume_string(candidate, mode, storage, max_string_units);
    if (!string.has_value())
    {
      return scalar_result::failure(*string.error_if());
    }

    if (string.value_if()->has_value())
    {
      scalar_token token{std::in_place_type<string_token>, std::move(**string.value_if())};
      return detail::finish_scalar_token(source, candidate, std::move(token), mode);
    }

    auto number = detail::consume_number_spelling(candidate, mode, max_exponent_digits);
    if (!number.has_value())
    {
      return scalar_result::failure(*number.error_if());
    }

    if (number.value_if()->has_value())
    {
      scalar_token token{std::in_place_type<number_token>, **number.value_if()};
      return detail::finish_scalar_token(source, candidate, std::move(token), mode);
    }

    // Literals need no decoded copy, so recognize them before the allocating identifier path.
    const auto literal = match_literal_bytes(candidate);
    if (literal.has_value())
    {
      candidate.advance(literal->byte_length);
      scalar_token token{std::in_place_type<literal_token>, *literal};

      return detail::finish_scalar_token(source, candidate, std::move(token), mode);
    }

    // Only rejection remains: reuse the identifier lexer so errors match the original path.
    auto identifier = consume_identifier(candidate, storage, max_identifier_units);
    if (!identifier.has_value())
    {
      return scalar_result::failure(*identifier.error_if());
    }

    if (!identifier.value_if()->has_value())
    {
      return scalar_result::failure(error::syntax_at(start, syntax_error_reason::unexpected_character));
    }

    // Unescaped literal spellings were matched above, so any identifier here is unknown.
    return scalar_result::failure(
      error::syntax_at((**identifier.value_if()).byte_offset, syntax_error_reason::unknown_literal));
  }

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
  result<std::optional<scalar_token>> consume_scalar(cursor& source, syntax_mode mode, const storage_callbacks& storage,
                                                     std::size_t max_units) noexcept
  {
    return consume_scalar(source, mode, storage, max_units, max_units, std::numeric_limits<std::size_t>::max());
  }
} // namespace adaptive_json5
