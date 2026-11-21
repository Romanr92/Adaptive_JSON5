#include "lexer/number/non_finite_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "input/cursor.hpp"
#include "lexer/number/number_token.hpp"
#include "unicode/unicode.hpp"
#include "unicode/utf8.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Recognize an exact Infinity or NaN numeric spelling.
   * @par Requirements
   * P027; J5-009; J5-021; J5-022; J5-041; J5-043; issue #125; issue #155.
   * @param source Cursor positioned at a possible non-finite value.
   * @param mode Selected input syntax.
   * @return Source metadata, successful nullopt before unrelated input or for an
   * unknown word, or an explicit located error.
   * @note Accepts one optional leading plus or minus.
   * After the sign, only uppercase I or N starts a recognition attempt.
   * Other starts return no-match without validating the remaining input.
   * A word that is not exactly Infinity or NaN, signed or unsigned (a spelling mismatch,
   * or an identifier continuation or backslash after it), returns no-match, so the scalar
   * lexer rejects the whole word, from the sign, as unknown_literal with a source_range,
   * like nul or trueish.
   * EOF inside the spelling returns no-match for an unsigned word and unexpected_end for
   * a signed one.
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
    // Code points of the two spellings; "NaN" uses the first c_nan_length entries.
    static constexpr std::array<std::uint32_t, 8U> c_infinity_spelling{0x49U, 0x6EU, 0x66U, 0x69U,
                                                                       0x6EU, 0x69U, 0x74U, 0x79U}; // "Infinity"
    static constexpr std::array<std::uint32_t, 8U> c_nan_spelling{0x4EU, 0x61U, 0x4EU};             // "NaN"
    constexpr std::size_t c_nan_length = 3U;

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

    // Only EOF inside a signed spelling stays a located failure; every other mismatch is an
    // unknown word that the scalar lexer reports whole, from the sign (issue #155).
    const bool signed_word = (candidate.offset() != start);

    // Choose the spelling by the first character's code point. An undecodable first byte is
    // neither 'I' nor 'N', so it is left to the other recognizers, as before.
    cursor probe = candidate;
    const auto first_decoded = decode_utf8(probe);
    const bool decodable = first_decoded.has_value() && first_decoded.value_if()->has_value();
    const std::uint32_t first_scalar = decodable ? (*first_decoded.value_if())->value : std::uint32_t{};
    const bool infinity = (first_scalar == c_infinity_spelling.front());
    if (!infinity && (first_scalar != c_nan_spelling.front()))
    {
      return non_finite_result::success(std::nullopt);
    }

    const auto& spelling = infinity ? c_infinity_spelling : c_nan_spelling;
    const number_kind kind = infinity ? number_kind::infinity : number_kind::nan;
    std::size_t remaining = infinity ? c_infinity_spelling.size() : c_nan_length;

    for (const std::uint32_t expected : spelling)
    {
      if (remaining == std::size_t{})
      {
        break;
      }

      --remaining;
      const auto decoded = decode_utf8(candidate);
      if (!decoded.has_value())
      {
        return non_finite_result::failure(*decoded.error_if());
      }

      const auto& character = *decoded.value_if();
      if (!character.has_value())
      {
        return signed_word
                 ? non_finite_result::failure(error::syntax_at(candidate.offset(), syntax_error_reason::unexpected_end))
                 : non_finite_result::success(std::nullopt);
      }

      if (character->value != expected)
      {
        return non_finite_result::success(std::nullopt);
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
      const std::uint32_t scalar = suffix->value;

      // Reject extended names and escaped suffixes as unknown words; a decimal point is not part of a word.
      if (is_identifier_part(scalar) || (scalar == 0x5CU)) // '\\'
      {
        return non_finite_result::success(std::nullopt);
      }

      if (scalar == 0x2EU) // '.'
      {
        return non_finite_result::failure(error::syntax_at(suffix->byte_offset, syntax_error_reason::unknown_literal));
      }
    }

    // Only a complete Infinity or NaN is a strict-JSON violation; other words are unknown in both modes.
    if (mode == syntax_mode::strict_json)
    {
      return non_finite_result::failure(error::syntax_at(start, syntax_error_reason::not_allowed_in_strict_json));
    }

    const std::size_t end = candidate.offset();
    if (end < start)
    {
      return non_finite_result::failure(error::internal_fault());
    }

    const number_token token{kind, start, end - start};
    auto completed = non_finite_result::success(std::optional<number_token>{token});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5
