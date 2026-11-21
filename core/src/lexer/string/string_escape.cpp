#include "lexer/string/string_escape.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/string/hex_escape.hpp"
#include "lexer/string/line_continuation.hpp"
#include "lexer/string/non_escape.hpp"
#include "lexer/string/simple_escape.hpp"
#include "lexer/string/unicode_escape.hpp"
#include "unicode/utf16.hpp"

#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Dispatch escape forms that produce exactly one UTF-16 unit.
     * @par Requirements
     * P021; J5-015; J5-016; J5-018.
     * @note x and u always go to their owning decoders.
     * Their failures must never fall back to non-escape characters.
     * @par ID SWU-LEXER-036
     */
    result<std::optional<std::uint16_t>> consume_unit_escape(cursor& source, syntax_mode mode) noexcept
    {
      const auto current = source.peek();

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.39)
      if (current.has_value() && (*current == 'x'))
      {
        return consume_hex_escape(source, mode);
      }

      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Tests whether the byte equals one specific character. An equality comparison
      // between two character operands does not use the numeric value; the tool reports the implicit promotion
      // of both operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on
      // an ASCII-compatible execution character set, where these characters are contiguous ranges.
      // codeql::misra_deviation_next_line(DEV-015.38)
      if (current.has_value() && (*current == 'u'))
      {
        return consume_unicode_escape(source);
      }

      return consume_simple_escape(source, mode);
    }
  } // namespace detail

  /**
   * @brief Decode one complete escape after its backslash.
   * @par Requirements
   * P021; J5-015; J5-016; J5-017; J5-018.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return Zero, one or two decoded UTF-16 units, or a located error.
   * @note Zero units represent a consumed line continuation.
   * Escaped surrogate units are preserved without scalar conversion.
   * Success consumes the complete escape suffix.
   * Failure leaves source unchanged.
   * No allocation is performed.
   * @par ID SWU-LEXER-037
   */
  result<encoded_utf16> consume_string_escape(cursor& source, syntax_mode mode) noexcept
  {
    // Keep the complete escape transactional across all recognizers.
    cursor candidate = source;

    const auto unit = detail::consume_unit_escape(candidate, mode);
    if (!unit.has_value())
    {
      return result<encoded_utf16>::failure(*unit.error_if());
    }

    if (unit.value_if()->has_value())
    {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.21)
      encoded_utf16 output{};
      output.units[encoded_utf16::c_first_unit_index] = **unit.value_if();
      output.size = encoded_utf16::c_single_unit_count;

      source = candidate;
      return result<encoded_utf16>::success(output);
    }

    const auto continuation = consume_line_continuation(candidate, mode);
    if (!continuation.has_value())
    {
      return result<encoded_utf16>::failure(*continuation.error_if());
    }

    if (continuation.value_if()->has_value())
    {
      // A complete continuation contributes no units to the string.
      source = candidate;
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.20)
      return result<encoded_utf16>::success(encoded_utf16{});
    }

    const auto character = consume_non_escape(candidate, mode);
    if (!character.has_value())
    {
      return result<encoded_utf16>::failure(*character.error_if());
    }

    if (!character.value_if()->has_value())
    {
      return result<encoded_utf16>::failure(error::syntax_at(source.offset(), syntax_error_reason::invalid_escape));
    }

    // Non-escape source characters are scalars and may require a surrogate pair.
    const auto encoded = encode_utf16((**character.value_if()).value);
    if (!encoded.has_value())
    {
      // Keep the encoder's code and reason, located at the source character.
      return result<encoded_utf16>::failure(encoded.error_if()->relocated(source.offset()));
    }

    source = candidate;
    return encoded;
  }
} // namespace adaptive_json5
