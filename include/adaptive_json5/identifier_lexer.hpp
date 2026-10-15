#ifndef IDENTIFIER_LEXER_HPP_
#define IDENTIFIER_LEXER_HPP_

#include "cursor.hpp"
#include "decoded_string_builder.hpp"
#include "error.hpp"
#include "identifier_token.hpp"
#include "result.hpp"
#include "storage.hpp"
#include "unicode.hpp"
#include "utf8.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Consume an identifier containing ordinary UTF-8 characters.
   * @par Requirements
   * P022; J5-019; J5-041.
   * @param source Cursor positioned at a possible identifier start.
   * @param storage Caller-provided storage for decoded UTF-16 units.
   * @param max_units Maximum decoded UTF-16 unit count.
   * @return An owned token, successful nullopt for no-match,
   * or an explicit located error.
   * @note This P022 version does not support identifier escapes yet.
   * A backslash reports a syntax error pending the P023 extension.
   * No-match and failure leave source unchanged.
   * Recognition is independent of key/value context and syntax mode.
   * Strict JSON member-name rejection belongs to the context-aware caller.
   * The storage provider state must outlive a returned token.
   */
  inline result<std::optional<identifier_token>> consume_identifier(cursor& source, const storage_callbacks& storage,
                                                                    std::size_t max_units) noexcept
  {
    using identifier_result = result<std::optional<identifier_token>>;

    const std::size_t start = source.offset();

    // Publish cursor movement only after the complete token succeeds.
    cursor candidate = source;
    decoded_string_builder builder(storage, max_units);
    bool first = true;

    while (true)
    {
      // P023 will replace this rejection with identifier escape decoding.
      const auto next = candidate.peek();
      if (next.has_value() && (*next == '\\'))
      {
        return identifier_result::failure(error::at_offset(error_code::syntax, candidate.offset()));
      }

      // Decode through a copy so the first nonmember stays unconsumed.
      cursor lookahead = candidate;
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

      const bool admitted = first ? is_identifier_start(character->value) : is_identifier_part(character->value);

      if (!admitted)
      {
        break;
      }

      const auto appended = builder.append_scalar(character->value);
      if (!appended.has_value())
      {
        return identifier_result::failure(error::at_offset(appended.error_if()->code, character->byte_offset));
      }

      candidate = lookahead;
      first = false;
    }

    if (first)
    {
      return identifier_result::success(std::nullopt);
    }

    identifier_token token{builder.finish(), start, candidate.offset() - start, false};

    // Prepare the owned result before committing the caller's position.
    auto completed = identifier_result::success(std::optional<identifier_token>{std::move(token)});

    source = candidate;
    return completed;
  }
} // namespace adaptive_json5

#endif /* IDENTIFIER_LEXER_HPP_ */
