// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/number/number_lexer.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/decimal_lexer.hpp"
#include "adaptive_json5/detail/lexer/number/hexadecimal_lexer.hpp"
#include "adaptive_json5/detail/lexer/number/non_finite_lexer.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/detail/lexer/token_boundary.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <limits>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    result<std::optional<number_token>> consume_number_spelling(cursor& source, syntax_mode mode,
                                                                std::size_t max_exponent_digits) noexcept
    {
      auto hexadecimal = consume_hexadecimal(source, mode);
      if (!hexadecimal.has_value())
      {
        return hexadecimal;
      }

      if (hexadecimal.value_if()->has_value())
      {
        return hexadecimal;
      }

      auto non_finite = consume_non_finite(source, mode);
      if (!non_finite.has_value())
      {
        return non_finite;
      }

      if (non_finite.value_if()->has_value())
      {
        return non_finite;
      }

      return consume_decimal(source, mode, max_exponent_digits);
    }
  } // namespace detail

  result<std::optional<number_token>> consume_number(cursor& source, syntax_mode mode,
                                                     std::size_t max_exponent_digits) noexcept
  {
    using number_result = result<std::optional<number_token>>;

    // Include boundary validation in the same private cursor transaction.
    cursor candidate = source;
    auto recognized = detail::consume_number_spelling(candidate, mode, max_exponent_digits);

    if (!recognized.has_value())
    {
      return recognized;
    }

    if (!recognized.value_if()->has_value())
    {
      return recognized;
    }

    const auto boundary = check_unquoted_token_boundary(candidate, mode);
    if (!boundary.has_value())
    {
      return number_result::failure(*boundary.error_if());
    }

    source = candidate;
    return recognized;
  }
} // namespace adaptive_json5
