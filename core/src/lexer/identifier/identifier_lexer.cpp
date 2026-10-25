#include "lexer/identifier/identifier_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/identifier/identifier_token.hpp"
#include "lexer/string/unicode_escape.hpp"
#include "memory/decoded_string_builder.hpp"
#include "unicode/unicode.hpp"
#include "unicode/utf8.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Consume an identifier with ordinary UTF-8 characters or Unicode escapes.
   * @par Requirements
   * P022; P023; J5-019; J5-020; J5-041.
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
   * The storage provider state must outlive a returned token.
   */
  result<std::optional<identifier_token>> consume_identifier(cursor& source, const storage_callbacks& storage,
                                                             std::size_t max_units) noexcept
  {
    using identifier_result = result<std::optional<identifier_token>>;

    constexpr std::size_t c_backslash_byte_count = 1;

    const std::size_t start = source.offset();

    // Publish cursor movement only after the complete token succeeds.
    cursor candidate = source;
    decoded_string_builder builder(storage, max_units);
    bool first = true;
    bool contains_escape = false;

    while (true)
    {
      const std::size_t character_offset = candidate.offset();
      const auto next = candidate.peek();

      // Decode privately so a terminating character remains unconsumed.
      cursor lookahead = candidate;
      char32_t scalar{};
      const bool escaped = next.has_value() && (*next == '\\');

      if (escaped)
      {
        // The existing escape decoder expects the cursor after the backslash.
        lookahead.advance(c_backslash_byte_count);

        const auto decoded = consume_unicode_escape(lookahead);
        if (!decoded.has_value())
        {
          return identifier_result::failure(*decoded.error_if());
        }

        if (!decoded.value_if()->has_value())
        {
          // Other string escape forms are not identifier escapes.
          return identifier_result::failure(error::at_offset(error_code::syntax, character_offset));
        }

        scalar = static_cast<char32_t>(**decoded.value_if());
      }
      else
      {
        const auto decoded = decode_utf8(lookahead);
        if (!decoded.has_value())
        {
          return identifier_result::failure(*decoded.error_if());
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
          return identifier_result::failure(error::at_offset(error_code::syntax, character_offset));
        }

        break;
      }

      const auto appended = builder.append_scalar(scalar);
      if (!appended.has_value())
      {
        return identifier_result::failure(error::at_offset(appended.error_if()->code, character_offset));
      }

      candidate = lookahead;
      first = false;
      contains_escape = contains_escape || escaped;
    }

    if (first)
    {
      return identifier_result::success(std::nullopt);
    }

    identifier_token token{builder.finish(), start, candidate.offset() - start, contains_escape};

    // Prepare the owned result before committing the caller's position.
    auto completed = identifier_result::success(std::optional<identifier_token>{std::move(token)});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5
