#include "lexer/punctuation.hpp"

#include "input/cursor.hpp"

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
   * @par ID SWU-LEXER-021
   */
  std::optional<punctuation_token> consume_punctuation(cursor& source) noexcept
  {
    constexpr std::size_t c_punctuation_byte_count = 1;

    const auto current = source.peek();

    if (!current.has_value())
    {
      return std::nullopt;
    }

    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.23)
    punctuation_kind kind{};

    // An explicit list prevents admitting operators or other punctuation.
    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.25)
    switch (*current)
    {
    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.24)
    case '{':
      kind = punctuation_kind::left_brace;
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.23)
    case '}':
      kind = punctuation_kind::right_brace;
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.22)
    case '[':
      kind = punctuation_kind::left_bracket;
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.21)
    case ']':
      kind = punctuation_kind::right_bracket;
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.20)
    case ':':
      kind = punctuation_kind::colon;
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.19)
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
