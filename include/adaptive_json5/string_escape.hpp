#ifndef STRING_ESCAPE_HPP_
#define STRING_ESCAPE_HPP_

#include "cursor.hpp"
#include "error.hpp"
#include "hex_escape.hpp"
#include "line_continuation.hpp"
#include "non_escape.hpp"
#include "options.hpp"
#include "result.hpp"
#include "simple_escape.hpp"
#include "unicode_escape.hpp"
#include "utf16.hpp"

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
     */
    inline result<std::optional<char16_t>> consume_unit_escape(cursor& source, syntax_mode mode) noexcept
    {
      const auto current = source.peek();

      if (current.has_value() && (*current == 'x'))
      {
        return consume_hex_escape(source, mode);
      }

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
   */
  inline result<encoded_utf16> consume_string_escape(cursor& source, syntax_mode mode) noexcept
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
      return result<encoded_utf16>::success(encoded_utf16{});
    }

    const auto character = consume_non_escape(candidate, mode);
    if (!character.has_value())
    {
      return result<encoded_utf16>::failure(*character.error_if());
    }

    if (!character.value_if()->has_value())
    {
      return result<encoded_utf16>::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    // Non-escape source characters are scalars and may require a surrogate pair.
    const auto encoded = encode_utf16((**character.value_if()).value);
    if (!encoded.has_value())
    {
      return result<encoded_utf16>::failure(error::at_offset(encoded.error_if()->code, source.offset()));
    }

    source = candidate;
    return encoded;
  }
} // namespace adaptive_json5

#endif /* STRING_ESCAPE_HPP_ */
