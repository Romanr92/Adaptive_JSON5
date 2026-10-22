#ifndef MEMBER_NAME_LEXER_HPP_
#define MEMBER_NAME_LEXER_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Own a decoded object member name and its lexical metadata.
     * @par Requirements
     * P033; P035; J5-019; J5-020; J5-041.
     * @note Quoted and identifier spellings share the same decoded representation.
     * Provider state must outlive the owned name.
     * Identifier names have no string warnings.
     */
    struct [[nodiscard]] member_name_token
    {
        decoded_string value{};
        std::size_t byte_offset{};
        std::size_t byte_length{};
        std::size_t warning_count{};
        std::optional<string_warning> first_warning{};
    };

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
     * Success consumes only the name, leaving the colon unconsumed.
     * Does not classify reserved words as value literals.
     * Does not skip trivia, update frames or invoke warning callbacks.
     * The caller handles shared resource and work accounting.
     */
    [[nodiscard]] result<std::optional<member_name_token>> consume_member_name(cursor& source, syntax_mode mode,
                                                                               const storage_callbacks& storage,
                                                                               std::size_t max_units) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* MEMBER_NAME_LEXER_HPP_ */
