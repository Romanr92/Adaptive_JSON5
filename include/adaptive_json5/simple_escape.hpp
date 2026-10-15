#ifndef SIMPLE_ESCAPE_HPP_
#define SIMPLE_ESCAPE_HPP_

#include "cursor.hpp"
#include "error.hpp"
#include "options.hpp"
#include "result.hpp"

#include <cstddef>
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
   */
  inline result<std::optional<char16_t>> consume_simple_escape(cursor& source, syntax_mode mode) noexcept
  {
    using escape_result = result<std::optional<char16_t>>;

    constexpr std::size_t c_escape_byte_count = 1;
    constexpr char c_decimal_digits_min = '0';
    constexpr char c_decimal_digits_max = '9';

    const auto current = source.peek();
    if (!current.has_value())
    {
      return escape_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    char16_t unit{};
    bool json5_only = false;

    switch (*current)
    {
    case '"':
      unit = u'"';
      break;

    case '\'':
      unit = u'\'';
      json5_only = true;
      break;

    case '\\':
      unit = u'\\';
      break;

    case '/':
      unit = u'/';
      break;

    case 'b':
      unit = u'\b';
      break;

    case 'f':
      unit = u'\f';
      break;

    case 'n':
      unit = u'\n';
      break;

    case 'r':
      unit = u'\r';
      break;

    case 't':
      unit = u'\t';
      break;

    case 'v':
      unit = u'\v';
      json5_only = true;
      break;

    case '0':
      unit = char16_t{};
      json5_only = true;
      break;

    default:
      // Other escape recognizers must see their original input.
      return escape_result::success(std::nullopt);
    }

    if (json5_only && (mode == syntax_mode::strict_json))
    {
      return escape_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    if (*current == c_decimal_digits_min)
    {
      const auto following = source.peek(c_escape_byte_count);

      // Reject the entire zero escape instead of accepting an octal prefix.
      if (following.has_value() && (*following >= c_decimal_digits_min) && (*following <= c_decimal_digits_max))
      {
        return escape_result::failure(error::at_offset(error_code::syntax, source.offset() + c_escape_byte_count));
      }
    }

    source.advance(c_escape_byte_count);
    return escape_result::success(std::optional<char16_t>{unit});
  }
} // namespace adaptive_json5

#endif /* SIMPLE_ESCAPE_HPP_ */
