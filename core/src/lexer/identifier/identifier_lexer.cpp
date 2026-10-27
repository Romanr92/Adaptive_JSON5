#include "lexer/identifier/identifier_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "common/checked_size.hpp"
#include "input/cursor.hpp"
#include "lexer/identifier/identifier_token.hpp"
#include "lexer/string/unicode_escape.hpp"
#include "memory/decoded_string_builder.hpp"
#include "unicode/unicode.hpp"
#include "unicode/utf16.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Totals produced by one walk over an identifier.
     * @par Requirements
     * P022; P023; J5-019; J5-046.
     */
    struct identifier_walk
    {
        std::size_t units{};    // Decoded UTF-16 units; zero means no identifier started here.
        bool contains_escape{}; // True when any character was a Unicode escape.
    };

    /**
     * @brief Walk one identifier, counting its units and optionally writing them.
     * @par Requirements
     * P022; P023; J5-019; J5-020; J5-041; J5-046.
     * @param source Cursor at a possible identifier start; after success, after the identifier.
     * @param max_units Maximum decoded UTF-16 units.
     * @param output Null to count only; otherwise a builder reserved for this identifier.
     * @return Unit totals, zero units for no-match, or a located error.
     * @note With a null output, performs no allocation (ADR 0003, decision 2).
     * resource_limit is reported at the character that first exceeds max_units.
     */
    result<identifier_walk> walk_identifier(cursor& source, std::size_t max_units,
                                            decoded_string_builder* output) noexcept
    {
      using walk_result = result<identifier_walk>;

      constexpr std::size_t c_backslash_byte_count = 1;

      identifier_walk walk{};
      bool first = true;

      while (true)
      {
        const std::size_t character_offset = source.offset();
        const auto next = source.peek();

        // Decode privately so a terminating character remains unconsumed.
        cursor lookahead = source;
        char32_t scalar{};
        const bool escaped = next.has_value() && (*next == '\\');

        if (escaped)
        {
          // The existing escape decoder expects the cursor after the backslash.
          lookahead.advance(c_backslash_byte_count);

          const auto decoded = consume_unicode_escape(lookahead);

          if (!decoded.has_value())
          {
            return walk_result::failure(*decoded.error_if());
          }

          if (!decoded.value_if()->has_value())
          {
            // Other string escape forms are not identifier escapes.
            return walk_result::failure(error::syntax_at(character_offset, syntax_error_reason::invalid_identifier));
          }

          scalar = static_cast<char32_t>(**decoded.value_if());
        }
        else
        {
          const auto decoded = decode_utf8(lookahead);

          if (!decoded.has_value())
          {
            return walk_result::failure(*decoded.error_if());
          }

          const auto& character = *decoded.value_if();

          if (!character.has_value())
          {
            break;
          }

          scalar = character->value;
        }

        const bool admitted = first ? is_identifier_start(scalar) : is_identifier_part(scalar);

        if (!admitted)
        {
          // An escape cannot hide an illegal character or terminate a name.
          if (escaped)
          {
            return walk_result::failure(error::syntax_at(character_offset, syntax_error_reason::invalid_identifier));
          }

          break;
        }

        // An invalid scaler keeps the builder's former error, located at it's character.
        const auto encoded = encode_utf16(scalar);

        if (!encoded.has_value())
        {
          return walk_result::failure(error::at_offset(encoded.error_if()->code, character_offset));
        }

        const auto total = checked_add(walk.units, encoded.value_if()->size);
        if ((!total.has_value()) || (*total.value_if() > max_units))
        {
          return walk_result::failure(error::at_offset(error_code::resource_limit, character_offset));
        }

        if (output != nullptr)
        {
          for (std::size_t index{}; index < encoded.value_if()->size; ++index)
          {
            const auto appended = output->append_unit(encoded.value_if()->units[index]);
            if (!appended.has_value())
            {
              return walk_result::failure(error::at_offset(appended.error_if()->code, character_offset));
            }
          }
        }

        walk.units = *total.value_if();
        source = lookahead;
        first = false;
        walk.contains_escape = walk.contains_escape || escaped;
      }

      return walk_result::success(walk);
    }
  } // namespace
  /**
   * @brief Consume an identifier with ordinary UTF-8 characters or Unicode escapes.
   * @par Requirements
   * P022; P023; J5-019; J5-020; J5-041; J5-046.
   * @param source Cursor positioned at a possible identifier start.
   * @param storage Caller-provided storage for decoded UTF-16 units.
   * @param max_units Maximum decoded UTF-16 unit count.
   * @return An owned token, successful nullopt for no-match,
   * or an explicit located error.
   * @note Only backslash-u followed by four hexadecimal digits is an escape.
   * Decoded escapes must satisfy the current start/part rule.
   * Illegal escaped characters and unsupported escape forms report the backslash offset.
   * Malformed Unicode escapes retain the decoder's error location.
   * No-match and failure leave source unchanged.
   * Recognition is independent of key/value context and syntax mode.
   * Strict JSON member-name rejection belongs to the context-aware caller.
   * Counts units first, then makes one allocation of exactly that size (ADR 0003, decision 2).
   * The storage provider state must outlive a returned token.
   */
  result<std::optional<identifier_token>> consume_identifier(cursor& source, const storage_callbacks& storage,
                                                             std::size_t max_units) noexcept
  {
    using identifier_result = result<std::optional<identifier_token>>;

    constexpr std::size_t c_backslash_byte_count = 1;

    const std::size_t start = source.offset();

    // First pass: validate and count, allocating nothing; publish nothing until the end.
    cursor counting = source;
    const auto counted = walk_identifier(counting, max_units, nullptr);

    if (!counted.has_value())
    {
      return identifier_result::failure(*counted.error_if());
    }

    const identifier_walk& totals = *counted.value_if();

    // Every identifier character decodes to at least one unit, so zero units means no-match.
    if (totals.units == std::size_t{})
    {
      return identifier_result::success(std::nullopt);
    }

    decoded_string_builder builder(storage, max_units);
    const auto reserved = builder.reserve_exact(totals.units);
    if (!reserved.has_value())
    {
      return identifier_result::failure(error::at_offset(reserved.error_if()->code, start));
    }

    // Second pass: write the units into the exact reservation.
    cursor decoding = source;
    const auto decoded = walk_identifier(decoding, max_units, &builder);
    if (!decoded.has_value())
    {
      return identifier_result::failure(*decoded.error_if());
    }

    // Both passes read the same bytes with the same code, so any difference is an internal error.
    if ((decoding.offset() != counting.offset()) || (decoded.value_if()->units != totals.units))
    {
      return identifier_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    identifier_token token{builder.finish(), start, counting.offset() - start, totals.contains_escape};

    // Prepare the owned result before committing the caller's position.
    auto completed = identifier_result::success(std::optional<identifier_token>{std::move(token)});

    source = counting;
    return completed;
  }
} // namespace adaptive_json5
