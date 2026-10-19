// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/string_escape.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/string/hex_escape.hpp"
#include "adaptive_json5/detail/lexer/string/line_continuation.hpp"
#include "adaptive_json5/detail/lexer/string/non_escape.hpp"
#include "adaptive_json5/detail/lexer/string/simple_escape.hpp"
#include "adaptive_json5/detail/lexer/string/unicode_escape.hpp"
#include "adaptive_json5/detail/unicode/utf16.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    result<std::optional<char16_t>> consume_unit_escape(cursor& source, syntax_mode mode) noexcept
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
