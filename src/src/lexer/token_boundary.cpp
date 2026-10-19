// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/token_boundary.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/lexer/whitespace.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

namespace adaptive_json5
{
  status_result check_unquoted_token_boundary(cursor source, syntax_mode mode) noexcept
  {
    cursor lookahead = source;
    const auto decoded = decode_utf8(lookahead);

    if (!decoded.has_value())
    {
      return status_result::failure(*decoded.error_if());
    }

    const auto& character = *decoded.value_if();
    if (!character.has_value())
    {
      return status_result::success();
    }

    const char32_t scalar = character->value;

    const bool json_whitespace = (scalar == U' ') || (scalar == U'\t') || (scalar == U'\r') || (scalar == U'\n');

    if (json_whitespace)
    {
      return status_result::success();
    }

    if (mode == syntax_mode::json5)
    {
      const bool additional_whitespace = is_json5_space(scalar) || (scalar == U'\u2028') || (scalar == U'\u2029');

      if (additional_whitespace)
      {
        return status_result::success();
      }
    }

    // A lexical boundary does not decide whether this punctuation fits the container.
    const auto punctuation = consume_punctuation(source);
    if (punctuation.has_value())
    {
      return status_result::success();
    }

    if (scalar == U'/')
    {
      // UTF-8 lookahead already advanced past the opening slash.
      const auto next = lookahead.peek();
      const bool comment_opener = next.has_value() && ((*next == '/') || (*next == '*'));

      if (comment_opener && (mode == syntax_mode::json5))
      {
        return status_result::success();
      }
    }

    return status_result::failure(error::at_offset(error_code::syntax, character->byte_offset));
  }
} // namespace adaptive_json5
