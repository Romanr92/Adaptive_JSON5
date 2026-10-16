#ifndef NON_FINITE_LEXER_HPP_
#define NON_FINITE_LEXER_HPP_

#include "cursor.hpp"
#include "error.hpp"
#include "number_token.hpp"
#include "options.hpp"
#include "result.hpp"
#include "unicode.hpp"
#include "utf8.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace adaptive_json5
{
  /**
   * @brief Recognize an exact Infinity or NaN numeric spelling.
   * @par Requirements
   * P027; J5-009; J5-021; J5-022; J5-041.
   * @param source Cursor positioned at a possible non-finite value.
   * @param mode Selected input syntax.
   * @return Source metadata, successful nullopt before unrelated input,
   * or an explicit located error.
   * @note Accepts one optional leading plus or minus.
   * After the sign, only uppercase I or N starts a recognition attempt.
   * Other starts return no-match without validating the remaining input.
   * Strict JSON rejects an attempted non-finite token at its original start.
   * JSON5 spelling mismatches report the offending character or EOF.
   * Identifier continuations, backslashes and decimal points after the name fail.
   * Malformed UTF-8 encountered during recognition reports an encoding error.
   * No-match and failure leave source unchanged.
   * Does not skip trivia, allocate, convert or own source text.
   * Use in value context; member names use identifier recognition instead.
   * Other suffix/context validation remains P028 work.
   */
  inline result<std::optional<number_token>> consume_non_finite(cursor& source, syntax_mode mode) noexcept
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

    if (mode == syntax_mode::strict_json)
    {
      return non_finite_result::failure(error::at_offset(error_code::syntax, start));
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
        return non_finite_result::failure(error::at_offset(error_code::syntax, candidate.offset()));
      }

      if (character->value != static_cast<char32_t>(expected))
      {
        return non_finite_result::failure(error::at_offset(error_code::syntax, character->byte_offset));
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

      // Reject extended names, escaped suffixes and numeric-looking continuations.
      if (is_identifier_part(scalar) || (scalar == U'\\') || (scalar == U'.'))
      {
        return non_finite_result::failure(error::at_offset(error_code::syntax, suffix->byte_offset));
      }
    }

    const number_token token{kind, start, candidate.offset() - start};
    auto completed = non_finite_result::success(std::optional<number_token>{token});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5

#endif /* NON_FINITE_LEXER_HPP_ */
