// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/punctuation.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Consume one structural punctuation character if present.
   * @par Requirements
   * P012; J5-013.
   * @param source Cursor positioned outside strings and comments.
   * @return A token, or std::nullopt at EOF or before nonpunctuation input.
   * @note No match leaves source unchanged.
   * Each recognized punctuator occupies exactly one byte.
   * Does not skip trivia or validate nonpunctuation input.
   * Requires no allocation and throws no exceptions.
   */
  std::optional<punctuation_token> consume_punctuation(cursor& source) noexcept
  {
    constexpr std::size_t c_punctuation_byte_count = 1;

    const auto current = source.peek();

    if (!current.has_value())
    {
      return std::nullopt;
    }

    punctuation_kind kind;

    // An explicit list prevents admitting operators or other punctuation.
    switch (*current)
    {
    case '{':
      kind = punctuation_kind::left_brace;
      break;

    case '}':
      kind = punctuation_kind::right_brace;
      break;

    case '[':
      kind = punctuation_kind::left_bracket;
      break;

    case ']':
      kind = punctuation_kind::right_bracket;
      break;

    case ':':
      kind = punctuation_kind::colon;
      break;

    case ',':
      kind = punctuation_kind::comma;
      break;

    default:
      // Other recognizers must see the original byte without losing input.
      return std::nullopt;
    }

    // Capture the original position before consuming the recognized character.
    const std::size_t start = source.offset();
    source.advance(c_punctuation_byte_count);

    return punctuation_token{kind, start, c_punctuation_byte_count};
  }
} // namespace adaptive_json5
