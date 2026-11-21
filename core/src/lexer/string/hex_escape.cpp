#include "lexer/string/hex_escape.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Convert an ASCII hexadecimal character to its numeric value.
   * @par Requirements
   * P016; J5-016.
   * @return A value from zero through fifteen, or nullopt for nonhex input.
   * @note Classification is independent of locale.
   * @par ID SWU-LEXER-029
   */
  std::optional<std::uint16_t> hex_digit_value(std::uint32_t character) noexcept
  {
    constexpr std::uint32_t c_decimal_min = 0x30U; // '0'
    constexpr std::uint32_t c_decimal_max = 0x39U; // '9'
    constexpr std::uint32_t c_lower_min = 0x61U;   // 'a'
    constexpr std::uint32_t c_lower_max = 0x66U;   // 'f'
    constexpr std::uint32_t c_upper_min = 0x41U;   // 'A'
    constexpr std::uint32_t c_upper_max = 0x46U;   // 'F'
    // Subtracting these bases maps a..f and A..F to 10..15 (0x61 - 0x57 == 10).
    constexpr std::uint32_t c_lower_base = 0x57U;
    constexpr std::uint32_t c_upper_base = 0x37U;

    if ((character >= c_decimal_min) && (character <= c_decimal_max))
    {
      return static_cast<std::uint16_t>(character - c_decimal_min);
    }

    if ((character >= c_lower_min) && (character <= c_lower_max))
    {
      return static_cast<std::uint16_t>(character - c_lower_base);
    }

    if ((character >= c_upper_min) && (character <= c_upper_max))
    {
      return static_cast<std::uint16_t>(character - c_upper_base);
    }

    return std::nullopt;
  }

  /**
   * @brief Decode a JSON5 hexadecimal escape after its backslash.
   * @par Requirements
   * P016; J5-016.
   * @param source Cursor immediately after an escape-introducing backslash.
   * @return A UTF-16 unit, successful nullopt before a non-x byte,
   * or a located syntax/encoding error.
   * @note A match consumes x and exactly two ASCII hexadecimal digits.
   * No-match and failure leave source unchanged.
   * Strict JSON rejects x at its original offset.
   * Missing digits report EOF; nonhex characters report their starting offset.
   * Nonmatching input is not validated. No allocation is performed.
   * @par ID SWU-LEXER-030
   */
  result<std::optional<std::uint16_t>> consume_hex_escape(cursor& source, syntax_mode mode) noexcept
  {
    using escape_result = result<std::optional<std::uint16_t>>;

    constexpr char c_hex_introducer = 'x';
    constexpr std::size_t c_introducer_byte_count = 1;
    constexpr std::size_t c_hex_digit_count = 2;
    constexpr std::uint32_t c_bits_per_hex_digit = 4;

    const auto current = source.peek();
    if (!current.has_value())
    {
      return escape_result::failure(error::syntax_at(source.offset(), syntax_error_reason::unexpected_end));
    }

    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Tests whether the byte equals one specific character. An equality comparison between
    // two character operands does not use the numeric value; the tool reports the implicit promotion of both
    // operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-
    // compatible execution character set, where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.41)
    if (*current != c_hex_introducer)
    {
      return escape_result::success(std::nullopt);
    }

    if (mode == syntax_mode::strict_json)
    {
      return escape_result::failure(error::syntax_at(source.offset(), syntax_error_reason::not_allowed_in_strict_json));
    }

    // Keep the original position until the entire escape is validated.
    cursor candidate = source;
    candidate.advance(c_introducer_byte_count);
    std::uint16_t unit{};

    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
    // bound that the body does not modify, and changed only in the increment expression. The counter is
    // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
    // codeql::misra_deviation_next_line(DEV-017.22)
    for (std::size_t index{}; index < c_hex_digit_count; ++index)
    {
      const auto decoded = decode_utf8(candidate);
      if (!decoded.has_value())
      {
        return escape_result::failure(*decoded.error_if());
      }

      const auto& character = *decoded.value_if();
      if (!character.has_value())
      {
        return escape_result::failure(error::syntax_at(candidate.offset(), syntax_error_reason::unexpected_end));
      }

      const auto digit = hex_digit_value(character->value);
      if (!digit.has_value())
      {
        return escape_result::failure(error::syntax_at(character->byte_offset, syntax_error_reason::invalid_escape));
      }

      // Two hexadecimal digits fit completely within one UTF-16 unit.
      unit = static_cast<std::uint16_t>((static_cast<std::uint32_t>(unit) << c_bits_per_hex_digit) |
                                        static_cast<std::uint32_t>(*digit));
    }

    source = candidate;
    return escape_result::success(std::optional<std::uint16_t>{unit});
  }
} // namespace adaptive_json5
