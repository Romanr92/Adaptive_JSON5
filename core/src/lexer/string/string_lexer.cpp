#include "lexer/string/string_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "common/checked_size.hpp"
#include "input/cursor.hpp"
#include "lexer/string/string_boundary.hpp"
#include "lexer/string/string_escape.hpp"
#include "lexer/string/string_token.hpp"
#include "lexer/string/string_warning.hpp"
#include "memory/decoded_string_builder.hpp"
#include "unicode/utf16.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Totals produced by one walk over a string body.
     * @par Requirements
     * P021; J5-017; J5-029; J5-046.
     */
    struct string_walk
    {
        std::size_t units{};                           // Decoded UTF-16 units.
        std::size_t warning_count{};                   // All warnings in the body.
        std::optional<string_warning> first_warning{}; // First warning, if any.
    };

    /**
     * @brief Walk a string body from after its opening quote through its closing quote.
     * @par Requirements
     * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
     * @param source Cursor after the opening quote; after success, after the closing quote.
     * @param start Original offset of the opening quote, for unterminated-string errors.
     * @param quote Quote character that closes the string.
     * @param mode Selected input syntax.
     * @param max_units Maximum decoded UTF-16 units.
     * @param output Null to count only; otherwise a builder reserved for this string.
     * @return Unit and warning totals, or a located error.
     * @note With a null output, performs no allocation (ADR 0003, decision 2).
     * resource_limit is reported at the piece that first exceeds max_units.
     */
    result<string_walk> walk_string_body(cursor& source, std::size_t start, string_quote quote, syntax_mode mode,
                                         std::size_t max_units, decoded_string_builder* output) noexcept
    {
      using walk_result = result<string_walk>;

      string_walk walk{};
      detail::surrogate_pairing pairing{};

      // Each warning covers distinct source bytes, so the count cannot exceed input size.
      const auto record = [&walk](const string_warning& warning) noexcept
      {
        if (!walk.first_warning.has_value())
        {
          walk.first_warning = warning;
        }

        ++walk.warning_count;
      };
      const auto record_all = [&record](const detail::surrogate_warnings& found) noexcept
      {
        for (std::size_t index{}; index < found.size; ++index)
        {
          record(found.items[index]);
        }
      };

      // Count one piece's units against the limit, and write them when decoding.
      const auto emit = [&walk, max_units, output](const encoded_utf16& units,
                                                   std::size_t offset) noexcept -> status_result
      {
        const auto total = checked_add(walk.units, units.size);
        if ((!total.has_value()) || (*total.value_if() > max_units))
        {
          return status_result::failure(error::at_offset(error_code::resource_limit, offset));
        }

        if (output != nullptr)
        {
          for (std::size_t index{}; index < units.size; ++index)
          {
            const auto appended = output->append_unit(units.units[index]);
            if (!appended.has_value())
            {
              return status_result::failure(error::at_offset(appended.error_if()->code, offset));
            }
          }
        }

        walk.units = *total.value_if();
        return status_result::success();
      };

      while (true)
      {
        const auto piece = consume_string_piece(source, quote, mode);
        if (!piece.has_value())
        {
          // Running out of input means the string is unterminated; cover it from its opening quote.
          if (source.is_at_end())
          {
            const source_span open_string{start, source.offset() - start};

            return walk_result::failure(
              error::syntax_at_range(source.offset(), open_string, syntax_error_reason::unterminated_string));
          }

          return walk_result::failure(*piece.error_if());
        }

        const std::size_t piece_offset = piece.value_if()->byte_offset;

        if (piece.value_if()->kind == string_piece_kind::closing_quote)
        {
          record_all(pairing.finish());
          return walk_result::success(walk);
        }

        if (piece.value_if()->kind == string_piece_kind::escape_start)
        {
          const auto escaped = consume_string_escape(source, mode);
          if (!escaped.has_value())
          {
            return walk_result::failure(*escaped.error_if());
          }

          const auto emitted = emit(*escaped.value_if(), piece_offset);
          if (!emitted.has_value())
          {
            return walk_result::failure(*emitted.error_if());
          }

          record_all(pairing.escape(*escaped.value_if(), piece_offset, source.offset() - piece_offset));
        }
        else
        {
          // An invalid scalar keeps the builder's former error, located at its character.
          const auto encoded = encode_utf16(piece.value_if()->scalar);
          if (!encoded.has_value())
          {
            return walk_result::failure(error::at_offset(encoded.error_if()->code, piece_offset));
          }

          const auto emitted = emit(*encoded.value_if(), piece_offset);
          if (!emitted.has_value())
          {
            return walk_result::failure(*emitted.error_if());
          }

          record_all(pairing.character());

          const auto warning = raw_separator_warning_for(*piece.value_if(), mode);
          if (warning.has_value())
          {
            record(*warning);
          }
        }
      }
    }
  } // namespace

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
      surrogate_pairing pairing{};

      // Deliver warnings in the same order the first pass counted them.
      const auto deliver = [&observer](const surrogate_warnings& found) noexcept
      {
        for (std::size_t index{}; index < found.size; ++index)
        {
          if (observer.notify != nullptr)
          {
            observer.notify(observer.context, found.items[index]);
          }
        }
      };

      while (true)
      {
        const auto piece = consume_string_piece(source, quote, mode);
        if (!piece.has_value())
        {
          return status_result::failure(*piece.error_if());
        }

        if (piece.value_if()->kind == string_piece_kind::closing_quote)
        {
          deliver(pairing.finish());
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

          const std::size_t escape_offset = piece.value_if()->byte_offset;
          deliver(pairing.escape(*escaped.value_if(), escape_offset, source.offset() - escape_offset));
        }
        else
        {
          deliver(pairing.character());

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
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-041; J5-046.
   * @param source Cursor positioned at a possible opening quote.
   * @param mode Selected input syntax.
   * @param storage Caller-owned fallible storage provider.
   * @param max_units Maximum decoded UTF-16 unit capacity.
   * @param observer Optional observer for all successful-string warnings.
   * @return Owned token, successful nullopt before nonquote input or EOF,
   * or an explicit located failure.
   * @note Success consumes through the matching closing quote only.
   * No-match and failure leave the caller's cursor unchanged.
   * Counts units first, then makes one allocation of exactly that size (ADR 0003, decision 2).
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
    const cursor body = candidate;

    // First pass: validate, count units and warnings, and allocate nothing.
    cursor counting = body;
    const auto counted = walk_string_body(counting, start, quote, mode, max_units, nullptr);

    if (!counted.has_value())
    {
      return string_result::failure(*counted.error_if());
    }

    const string_walk& totals = *counted.value_if();

    // One allocation of exactly the counted size; an empty string makes none.
    decoded_string_builder builder(storage, max_units);
    const auto reserved = builder.reserve_exact(totals.units);
    if (!reserved.has_value())
    {
      return string_result::failure(error::at_offset(reserved.error_if()->code, start));
    }

    // Second pass: write the units into the reservation.
    cursor decoding = body;
    const auto decoded = walk_string_body(decoding, start, quote, mode, max_units, &builder);
    if (!decoded.has_value())
    {
      return string_result::failure(*decoded.error_if());
    }

    // Both passes read the same bytes with the same code, so any difference is an internal error.
    if ((decoding.offset() != counting.offset()) || (decoded.value_if()->units != totals.units))
    {
      return string_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    string_token token{builder.finish(), start, counting.offset() - start, totals.warning_count, totals.first_warning};

    // Construct the complete result before publishing any warning callbacks.
    auto completed = string_result::success(std::optional<string_token>{std::move(token)});

    if ((observer.notify != nullptr) && (totals.warning_count != std::size_t{}))
    {
      const auto notified = detail::notify_string_warnings(body, quote, mode, observer);
      if (!notified.has_value())
      {
        return string_result::failure(*notified.error_if());
      }
    }

    source = counting;
    return completed;
  }
} // namespace adaptive_json5
