// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/line_continuation.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/line_terminator.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <optional>

namespace adaptive_json5
{
  result<std::optional<line_terminator_span>> consume_line_continuation(cursor& source, syntax_mode mode) noexcept
  {
    using continuation_result = result<std::optional<line_terminator_span>>;

    if (!source.peek().has_value())
    {
      return continuation_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    // Apply the syntax policy before committing the recognized terminator.
    cursor candidate = source;
    const auto terminator = consume_line_terminator(candidate);
    if (!terminator.has_value())
    {
      return continuation_result::failure(*terminator.error_if());
    }

    if (!terminator.value_if()->has_value())
    {
      return continuation_result::success(std::nullopt);
    }

    if (mode == syntax_mode::strict_json)
    {
      return continuation_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    source = candidate;
    return terminator;
  }
} // namespace adaptive_json5
