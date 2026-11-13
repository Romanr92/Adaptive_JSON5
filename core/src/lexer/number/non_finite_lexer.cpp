#include "lexer/number/non_finite_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "input/cursor.hpp"
#include "lexer/number/number_token.hpp"
#include "unicode/unicode.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace adaptive_json5
{
  /**
   * @brief Recognize an exact Infinity or NaN numeric spelling.
   * @par Requirements
   * P027; J5-009; J5-021; J5-022; J5-041; J5-043; issue #125.
   * @param source Cursor positioned at a possible non-finite value.
   * @param mode Selected input syntax.
   * @return Source metadata, successful nullopt before unrelated input or for an
   * unsigned unknown word, or an explicit located error.
   * @note Accepts one optional leading plus or minus.
   * After the sign, only uppercase I or N starts a recognition attempt.
   * Other starts return no-match without validating the remaining input.
   * An unsigned word that is not exactly Infinity or NaN (a spelling mismatch, EOF inside
   * the spelling, or an identifier continuation or backslash after it) returns no-match,
   * so the scalar lexer rejects the whole word as unknown_literal with a source_range,
   * like nul or trueish.
   * A signed word reports unknown_literal at the offending character, or unexpected_end
   * at EOF, without a range.
   * A decimal point after the name fails with unknown_literal at the point.
   * Strict JSON rejects only a complete Infinity or NaN, at its original start; other
   * words are unknown in both modes.
   * Malformed UTF-8 encountered during recognition reports an encoding error.
   * No-match and failure leave source unchanged.
   * Does not skip trivia, allocate, convert or own source text.
   * Use in value context; member names use identifier recognition instead.
   * Other suffix/context validation remains P028 work.
   * @par ID SWU-LEXER-018
   */
  result<std::optional<number_token>> consume_non_finite(cursor& source, syntax_mode mode) noexcept
  {
    using non_finite_result = result<std::optional<number_token>>;

    constexpr std::size_t c_ascii_byte_count = 1;
    constexpr std::string_view c_infinity_spelling{"Infinity"};
    constexpr std::string_view c_nan_spelling{"NaN"};

    const std::size_t start = source.offset();

    // Inspect the sign privately so other recognizers retain the complete input.
    cursor candidate = source;
    const auto initial = candidate.peek();
    if (initial.has_value() && ((*initial == '-') || (*initial == '+')))
    {
      candidate.advance(c_ascii_byte_count);
    }

    const auto first = candidate.peek();
    if (!first.has_value())
    {
      return non_finite_result::success(std::nullopt);
    }

    // After the sign: an unsigned word that is not exactly Infinity or NaN is an unknown word.
    const bool signed_word = (candidate.offset() != start);

    // Unsigned mismatches return no match, so the scalar lexer rejects the whole word with its
    // source_range, as for nul or trueish. A signed word keeps the located failure.
    const auto reject = [signed_word](const error& located) noexcept
    {
      return signed_word ? non_finite_result::failure(located) : non_finite_result::success(std::nullopt);
    };

    std::string_view spelling{};
    number_kind kind = number_kind::infinity;

    if (*first == 'I')
    {
      spelling = c_infinity_spelling;
    }
    else if (*first == 'N')
    {
      spelling = c_nan_spelling;
      kind = number_kind::nan;
    }
    else
    {
      return non_finite_result::success(std::nullopt);
    }

    for (const char expected : spelling)
    {
      const auto decoded = decode_utf8(candidate);
      if (!decoded.has_value())
      {
        return non_finite_result::failure(*decoded.error_if());
      }

      const auto& character = *decoded.value_if();
      if (!character.has_value())
      {
        return reject(error::syntax_at(candidate.offset(), syntax_error_reason::unexpected_end));
      }

      if (character->value != static_cast<char32_t>(expected))
      {
        return reject(error::syntax_at(character->byte_offset, syntax_error_reason::unknown_literal));
      }
    }

    // Inspect the following scalar without consuming a valid terminator.
    cursor lookahead = candidate;
    const auto following = decode_utf8(lookahead);
    if (!following.has_value())
    {
      return non_finite_result::failure(*following.error_if());
    }

    const auto& suffix = *following.value_if();
    if (suffix.has_value())
    {
      const char32_t scalar = suffix->value;

      // Reject extended names and escaped suffixes as unknown words; a decimal point is not part of a word.
      if (is_identifier_part(scalar) || (scalar == U'\\'))
      {
        return reject(error::syntax_at(suffix->byte_offset, syntax_error_reason::unknown_literal));
      }

      if (scalar == U'.')
      {
        return non_finite_result::failure(error::syntax_at(suffix->byte_offset, syntax_error_reason::unknown_literal));
      }
    }

    // Only a complete Infinity or NaN is a strict-JSON violation; other words are unknown in both modes.
    if (mode == syntax_mode::strict_json)
    {
      return non_finite_result::failure(error::syntax_at(start, syntax_error_reason::not_allowed_in_strict_json));
    }

    const number_token token{kind, start, candidate.offset() - start};
    auto completed = non_finite_result::success(std::optional<number_token>{token});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5
