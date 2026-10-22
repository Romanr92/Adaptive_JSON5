#include "adaptive_json5/detail/lexer/string/string_lexer.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"
#include "adaptive_json5/detail/lexer/string/string_escape.hpp"
#include "adaptive_json5/detail/lexer/string/string_token.hpp"
#include "adaptive_json5/detail/lexer/string/string_warning.hpp"
#include "adaptive_json5/detail/memory/decoded_string_builder.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Deliver warnings from an already validated string body.
     * @par Requirements
     * P021; J5-017.
     * @param source Copy of the cursor immediately after the opening quote.
     * @note Requires the same unchanged input validated by the first pass.
     * The observer must satisfy string_warning_observer's lifetime contract.
     * Performs no allocation and does not modify the caller's cursor.
     */
    status_result notify_string_warnings(cursor source, string_quote quote, syntax_mode mode,
                                         const string_warning_observer& observer) noexcept
    {
      while (true)
      {
        const auto piece = consume_string_piece(source, quote, mode);
        if (!piece.has_value())
        {
          return status_result::failure(*piece.error_if());
        }

        if (piece.value_if()->kind == string_piece_kind::closing_quote)
        {
          return status_result::success();
        }

        if (piece.value_if()->kind == string_piece_kind::escape_start)
        {
          // Follow the same escape boundaries without rebuilding decoded storage.
          const auto escaped = consume_string_escape(source, mode);
          if (!escaped.has_value())
          {
            return status_result::failure(*escaped.error_if());
          }
        }
        else
        {
          const auto warning = raw_separator_warning_for(*piece.value_if(), mode);

          if (warning.has_value() && (observer.notify != nullptr))
          {
            observer.notify(observer.context, *warning);
          }
        }
      }
    }
  } // namespace detail

  /**
   * @brief Consume one complete quoted string into owned decoded storage.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-041.
   * @param source Cursor positioned at a possible opening quote.
   * @param mode Selected input syntax.
   * @param storage Caller-owned fallible storage provider.
   * @param max_units Maximum decoded UTF-16 unit capacity.
   * @param observer Optional observer for all successful-string warnings.
   * @return Owned token, successful nullopt before nonquote input or EOF,
   * or an explicit located failure.
   * @note Success consumes through the matching closing quote only.
   * No-match and failure leave the caller's cursor unchanged.
   * Partial owned storage is released on failure.
   * No warning callbacks occur for a string that fails validation or storage.
   * Input must remain unchanged throughout both passes and callbacks.
   * Observer callbacks must satisfy string_warning_observer's contract.
   * Does not skip trivia or validate input following the closing quote.
   * max_units is a component limit, not complete parser memory accounting.
   */
  result<std::optional<string_token>> consume_string(cursor& source, syntax_mode mode, const storage_callbacks& storage,
                                                     std::size_t max_units, string_warning_observer observer) noexcept
  {
    using string_result = result<std::optional<string_token>>;

    const std::size_t start = source.offset();

    // Keep all token consumption private until the complete operation succeeds.
    cursor candidate = source;
    const auto opening = consume_string_opening(candidate, mode);
    if (!opening.has_value())
    {
      return string_result::failure(*opening.error_if());
    }

    if (!opening.value_if()->has_value())
    {
      return string_result::success(std::nullopt);
    }

    const string_quote quote = **opening.value_if();
    const cursor warning_source = candidate;
    decoded_string_builder builder(storage, max_units);
    std::size_t warning_count{};
    std::optional<string_warning> first_warning{};

    while (true)
    {
      const auto piece = consume_string_piece(candidate, quote, mode);
      if (!piece.has_value())
      {
        return string_result::failure(*piece.error_if());
      }

      if (piece.value_if()->kind == string_piece_kind::closing_quote)
      {
        string_token token{builder.finish(), start, candidate.offset() - start, warning_count, first_warning};

        // Construct the complete result before publishing any warning callbacks.
        auto completed = string_result::success(std::optional<string_token>{std::move(token)});

        if ((observer.notify != nullptr) && (warning_count != std::size_t{}))
        {
          const auto notified = detail::notify_string_warnings(warning_source, quote, mode, observer);
          if (!notified.has_value())
          {
            return string_result::failure(*notified.error_if());
          }
        }

        source = candidate;
        return completed;
      }

      if (piece.value_if()->kind == string_piece_kind::escape_start)
      {
        const auto escaped = consume_string_escape(candidate, mode);
        if (!escaped.has_value())
        {
          return string_result::failure(*escaped.error_if());
        }

        const auto& units = *escaped.value_if();
        for (std::size_t index{}; index < units.size; ++index)
        {
          const auto appended = builder.append_unit(units.units[index]);
          if (!appended.has_value())
          {
            return string_result::failure(error::at_offset(appended.error_if()->code, piece.value_if()->byte_offset));
          }
        }
      }
      else
      {
        const auto appended = builder.append_scalar(piece.value_if()->scalar);
        if (!appended.has_value())
        {
          return string_result::failure(error::at_offset(appended.error_if()->code, piece.value_if()->byte_offset));
        }

        const auto warning = raw_separator_warning_for(*piece.value_if(), mode);
        if (warning.has_value())
        {
          if (!first_warning.has_value())
          {
            first_warning = warning;
          }

          // Each warning consumes source bytes, so its count cannot exceed input size.
          ++warning_count;
        }
      }
    }
  }
} // namespace adaptive_json5
