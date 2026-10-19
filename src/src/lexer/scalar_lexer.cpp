// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/scalar_lexer.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/identifier/identifier_classification.hpp"
#include "adaptive_json5/detail/lexer/identifier/identifier_lexer.hpp"
#include "adaptive_json5/detail/lexer/number/number_lexer.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/lexer/scalar_token.hpp"
#include "adaptive_json5/detail/lexer/string/string_lexer.hpp"
#include "adaptive_json5/detail/lexer/string/string_token.hpp"
#include "adaptive_json5/detail/lexer/token_boundary.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    result<std::optional<scalar_token>> finish_scalar_token(cursor& source, cursor candidate, scalar_token token,
                                                            syntax_mode mode) noexcept
    {
      using scalar_result = result<std::optional<scalar_token>>;

      const auto boundary = check_unquoted_token_boundary(candidate, mode);
      if (!boundary.has_value())
      {
        return scalar_result::failure(*boundary.error_if());
      }

      auto completed = scalar_result::success(std::optional<scalar_token>{std::move(token)});

      source = candidate;

      return completed;
    }
  } // namespace detail

  result<std::optional<scalar_token>> consume_scalar(cursor& source, syntax_mode mode, const storage_callbacks& storage,
                                                     std::size_t max_string_units, std::size_t max_identifier_units,
                                                     std::size_t max_exponent_digits) noexcept
  {
    using scalar_result = result<std::optional<scalar_token>>;

    const std::size_t start = source.offset();
    if (!source.peek().has_value())
    {
      return scalar_result::success(std::nullopt);
    }

    // Leave structural tokens available to the container parser.
    cursor punctuation_source = source;
    const auto punctuation = consume_punctuation(punctuation_source);
    if (punctuation.has_value())
    {
      return scalar_result::success(std::nullopt);
    }

    cursor candidate = source;

    // Delay public success until the following boundary also validates.
    auto string = consume_string(candidate, mode, storage, max_string_units);
    if (!string.has_value())
    {
      return scalar_result::failure(*string.error_if());
    }

    if (string.value_if()->has_value())
    {
      scalar_token token{std::in_place_type<string_token>, std::move(**string.value_if())};
      return detail::finish_scalar_token(source, candidate, std::move(token), mode);
    }

    auto number = detail::consume_number_spelling(candidate, mode, max_exponent_digits);
    if (!number.has_value())
    {
      return scalar_result::failure(*number.error_if());
    }

    if (number.value_if()->has_value())
    {
      scalar_token token{std::in_place_type<number_token>, **number.value_if()};
      return detail::finish_scalar_token(source, candidate, std::move(token), mode);
    }

    auto identifier = consume_identifier(candidate, storage, max_identifier_units);
    if (!identifier.has_value())
    {
      return scalar_result::failure(*identifier.error_if());
    }

    if (!identifier.value_if()->has_value())
    {
      return scalar_result::failure(error::at_offset(error_code::syntax, start));
    }

    const auto& name = **identifier.value_if();
    const auto classification = classify_identifier(name, identifier_context::value, mode);
    if (!classification.has_value())
    {
      return scalar_result::failure(*classification.error_if());
    }

    literal_kind kind = literal_kind::null_literal;

    switch (*classification.value_if())
    {
    case identifier_classification::null_literal:
      kind = literal_kind::null_literal;
      break;
    case identifier_classification::true_literal:
      kind = literal_kind::true_literal;
      break;
    case identifier_classification::false_literal:
      kind = literal_kind::false_literal;
      break;

    default:
      return scalar_result::failure(error::at_offset(error_code::syntax, name.byte_offset));
    }

    const literal_token literal{kind, name.byte_offset, name.byte_length};
    scalar_token token{std::in_place_type<literal_token>, literal};

    return detail::finish_scalar_token(source, candidate, std::move(token), mode);
  }

  result<std::optional<scalar_token>> consume_scalar(cursor& source, syntax_mode mode, const storage_callbacks& storage,
                                                     std::size_t max_units) noexcept
  {
    return consume_scalar(source, mode, storage, max_units, max_units, std::numeric_limits<std::size_t>::max());
  }
} // namespace adaptive_json5
