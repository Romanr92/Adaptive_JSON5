#include "lexer/member_name_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/identifier/identifier_lexer.hpp"
#include "lexer/string/string_lexer.hpp"
#include "unicode/unicode.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    namespace
    {
      /**
       * @brief Report whether input starts like a JSON5 identifier name.
       * @par Requirements
       * P033; J5-019; J5-020.
       * @param source Copy of the cursor at the possible name.
       * @return True when the first character is an identifier start or a backslash.
       * @note Decodes one character only and allocates nothing.
       * Lets strict mode name the violation instead of reporting a missing name.
       */
      bool starts_identifier_name(cursor source) noexcept
      {
        const auto first = decode_utf8(source);

        return first.has_value() && first.value_if()->has_value() &&
               (is_identifier_start((*first.value_if())->value) || ((*first.value_if())->value == U'\\'));
      }
    } // namespace

    /**
     * @brief Recognize one quoted or mode-permitted identifier member name.
     * @par Requirements
     * P033; P035; J5-017; J5-019; J5-020; J5-041.
     * @param source Cursor positioned after caller-managed trivia.
     * @param mode Selected input syntax.
     * @param storage Fallible provider for decoded name storage.
     * @param max_units Inclusive decoded-name limit for this recognition.
     * @return Owned name, successful nullopt on no-match, or an explicit failure.
     * @note Failure and no-match preserve source.
     * Strict mode reports an identifier-shaped name as not_allowed_in_strict_json.
     * Success consumes only the name, leaving the colon unconsumed.
     * Does not classify reserved words as value literals.
     * Does not skip trivia, update frames or invoke warning callbacks.
     * The caller handles shared resource and work accounting.
     */
    result<std::optional<member_name_token>> consume_member_name(cursor& source, syntax_mode mode,
                                                                 const storage_callbacks& storage,
                                                                 std::size_t max_units) noexcept
    {
      using name_result = result<std::optional<member_name_token>>;

      cursor candidate = source;

      // P035: quoted and identifier names must produce the same owned decoded representation.
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
        // Name a JSON5 identifier as the strict-mode violation instead of a missing name, without allocating.
        if (starts_identifier_name(source))
        {
          return name_result::failure(
            error::syntax_at(source.offset(), syntax_error_reason::not_allowed_in_strict_json));
        }

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

    /**
     * @brief Recognize a member name like consume_member_name, but measure it without allocating.
     * @par Requirements
     * P033; P035; J5-017; J5-019; J5-020; J5-041; J5-046; J5-047.
     * @param source Cursor positioned after caller-managed trivia.
     * @param mode Selected input syntax.
     * @param max_units Inclusive decoded-name limit for this recognition.
     * @return A measurement, nullopt on no-match, or a located failure.
     * @note Keeps consume_member_name's order so both report the same errors (ADR 0003, decision 7).
     */
    result<std::optional<member_name_measure>> measure_member_name(cursor& source, syntax_mode mode,
                                                                   std::size_t max_units) noexcept
    {
      using measure_result = result<std::optional<member_name_measure>>;

      cursor candidate = source;

      const auto quoted = measure_string(candidate, mode, max_units);

      if (!quoted.has_value())
      {
        return measure_result::failure(*quoted.error_if());
      }

      // Success can still mean "no quoted name here"; only then try an identifier.
      if (quoted.value_if()->has_value())
      {
        const string_measure& name = **quoted.value_if();
        const member_name_measure measured{name.units, name.byte_offset, name.byte_length, name.warning_count};

        source = candidate;
        return measure_result::success(measured);
      }

      // Strict JSON has no identifier-name alternative.
      if (mode != syntax_mode::json5)
      {
        // Name a JSON5 identifier as the strict-mode violation instead of a missing name, without allocating.
        if (starts_identifier_name(source))
        {
          return measure_result::failure(
            error::syntax_at(source.offset(), syntax_error_reason::not_allowed_in_strict_json));
        }

        return measure_result::success(std::nullopt);
      }

      const auto identifier = measure_identifier(candidate, max_units);

      if (!identifier.has_value())
      {
        return measure_result::failure(*identifier.error_if());
      }

      // Success can still mean "no identifier here"; then there is no name at all.
      if (!identifier.value_if()->has_value())
      {
        return measure_result::success(std::nullopt);
      }

      const identifier_measure& name = **identifier.value_if();
      const member_name_measure measured{name.units, name.byte_offset, name.byte_length, 0};

      source = candidate;
      return measure_result::success(measured);
    }
  } // namespace detail
} // namespace adaptive_json5
