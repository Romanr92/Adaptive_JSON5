// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/member_name_lexer.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/identifier/identifier_lexer.hpp"
#include "adaptive_json5/detail/lexer/string/string_lexer.hpp"
#include "adaptive_json5/detail/lexer/string/string_warning.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    result<std::optional<member_name_token>> consume_member_name(cursor& source, syntax_mode mode,
                                                                 const storage_callbacks& storage,
                                                                 std::size_t max_units) noexcept
    {
      using name_result = result<std::optional<member_name_token>>;

      cursor candidate = source;

      auto quoted = consume_string(candidate, mode, storage, max_units);

      if (!quoted.has_value())
      {
        return name_result::failure(*quoted.error_if());
      }

      if (quoted.value_if()->has_value())
      {
        auto& token = **quoted.value_if();

        member_name_token name{std::move(token.value), token.byte_offset, token.byte_length, token.warning_count,
                               token.first_warning};

        auto completed = name_result::success(std::optional<member_name_token>{std::move(name)});

        // Publish consumption only after the owned result exists.
        source = candidate;
        return completed;
      }

      // Strict JSON has no identifier-name alternative.
      if (mode != syntax_mode::json5)
      {
        return name_result::success(std::nullopt);
      }

      auto identifier = consume_identifier(candidate, storage, max_units);

      if (!identifier.has_value())
      {
        return name_result::failure(*identifier.error_if());
      }

      if (!identifier.value_if()->has_value())
      {
        return name_result::success(std::nullopt);
      }

      auto& token = **identifier.value_if();

      member_name_token name{std::move(token.value), token.byte_offset, token.byte_length, 0, std::nullopt};

      auto completed = name_result::success(std::optional<member_name_token>{std::move(name)});

      // Keep the caller's cursor unchanged until ownership is ready to return.
      source = candidate;
      return completed;
    }
  } // namespace detail
} // namespace adaptive_json5
