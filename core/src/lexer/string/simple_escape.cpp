#include "lexer/string/simple_escape.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Decode one simple escape after its backslash.
   * @par Requirements
   * P015; J5-015; J5-016.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return One UTF-16 unit, successful nullopt for another escape form,
   * or a located syntax error.
   * @note Success with a unit consumes one ASCII byte.
   * No-match and failure leave source unchanged.
   * Does not validate or consume bytes belonging to other escape forms.
   * A JSON5 zero escape fails if followed by any decimal digit.
   * EOF after an accepted zero still requires the enclosing string to close.
   * No allocation is performed.
   * @par ID SWU-LEXER-033
   */
  result<std::optional<std::uint16_t>> consume_simple_escape(cursor& source, syntax_mode mode) noexcept
  {
    using escape_result = result<std::optional<std::uint16_t>>;

    constexpr std::size_t c_escape_byte_count = 1;
    constexpr char c_decimal_digits_min = '0';
    constexpr char c_decimal_digits_max = '9';

    const auto current = source.peek();
    if (!current.has_value())
    {
      return escape_result::failure(error::syntax_at(source.offset(), syntax_error_reason::unexpected_end));
    }

    // The NUL escape (backslash zero) keeps this initial value; every other case assigns its unit.
    std::uint16_t unit{};
    bool json5_only = false;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.60)
    switch (*current)
    {
    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.59)
    case '"':
      unit = 0x22U; // '"'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.58)
    case '\'':
      unit = 0x27U; // '\''
      json5_only = true;
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.57)
    case '\\':
      unit = 0x5CU; // '\\'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.56)
    case '/':
      unit = 0x2FU; // '/'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.55)
    case 'b':
      unit = 0x08U; // '\b'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.54)
    case 'f':
      unit = 0x0CU; // '\f'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.53)
    case 'n':
      unit = 0x0AU; // '\n'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.52)
    case 'r':
      unit = 0x0DU; // '\r'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.51)
    case 't':
      unit = 0x09U; // '\t'
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.50)
    case 'v':
      unit = 0x0BU; // '\v'
      json5_only = true;
      break;

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Selects on the exact character with character case labels; no arithmetic uses the
    // character value. The tool reports the integral promotion of the condition and labels. The parser compares
    // UTF-8 input bytes with character literals, so it relies on an ASCII-compatible execution character set,
    // where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.49)
    case '0':
      json5_only = true;
      break;

    default:
      // Other escape recognizers must see their original input.
      return escape_result::success(std::nullopt);
    }

    if (json5_only && (mode == syntax_mode::strict_json))
    {
      return escape_result::failure(error::syntax_at(source.offset(), syntax_error_reason::not_allowed_in_strict_json));
    }

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Tests whether the byte equals one specific character. An equality comparison between
    // two character operands does not use the numeric value; the tool reports the implicit promotion of both
    // operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-
    // compatible execution character set, where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.48)
    if (*current == c_decimal_digits_min)
    {
      const auto following = source.peek(c_escape_byte_count);

      // Reject the entire zero escape instead of accepting an octal prefix.
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.47)
      if (following.has_value() && (*following >= c_decimal_digits_min) && (*following <= c_decimal_digits_max))
      {
        return escape_result::failure(
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: The two escape bytes were read from the input, so offset + c_escape_byte_count
          // is at most the input size.
          // codeql::misra_deviation_next_line(DEV-004.113)
          error::syntax_at(source.offset() + c_escape_byte_count, syntax_error_reason::invalid_escape));
      }
    }

    source.advance(c_escape_byte_count);
    return escape_result::success(std::optional<std::uint16_t>{unit});
  }
} // namespace adaptive_json5
