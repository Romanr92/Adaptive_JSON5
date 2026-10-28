#ifndef MEMBER_NAME_LEXER_HPP_
#define MEMBER_NAME_LEXER_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "input/cursor.hpp"
#include "lexer/string/string_token.hpp"

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
     * @brief Describe one validated member name without owning its decoded units.
     * @par Requirements
     * P033; P035; J5-017; J5-019; J5-020; J5-046; J5-047.
     * @note byte_offset and byte_length cover the whole name spelling, quotes included.
     * Identifier names have no string warnings, so their warning_count is zero.
     * Owns nothing, so it can be produced and discarded without a storage provider.
     */
    struct [[nodiscard]] member_name_measure
    {
        std::size_t units{};
        std::size_t byte_offset{};
        std::size_t byte_length{};
        std::size_t warning_count{};
    };

    /**
     * @brief Recognize one quoted or mode-permitted identifier member name.
     * @par Requirements
     * P033; P035; J5-017; J5-019; J5-020; J5-041.
     * @param source Cursor positioned after caller-managed trivia.
     * @param mode Selected input syntax.
     * @param storage Fallible provider for decoded name storage.
     * @param max_units Inclusive decoded-name limit for this recognition.
     * @param sink Optional parser sink for quoted names: reserve after allocation, record while decoding.
     * @return Owned name, successful nullopt on no-match, or an explicit failure.
     * @note Failure and no-match preserve source.
     * Success consumes only the name, leaving the colon unconsumed.
     * Does not classify reserved words as value literals.
     * Does not skip trivia, update frames or invoke warning callbacks; identifier names never use the sink.
     * The caller handles shared resource and work accounting.
     */
    [[nodiscard]] result<std::optional<member_name_token>>
    consume_member_name(cursor& source, syntax_mode mode, const storage_callbacks& storage, std::size_t max_units,
                        const string_warning_sink* sink = nullptr) noexcept;

    /**
     * @brief Recognize a member name like consume_member_name, but measure it without allocating.
     * @par Requirements
     * P033; P035; J5-017; J5-019; J5-020; J5-041; J5-046; J5-047.
     * @param source Cursor positioned after caller-managed trivia.
     * @param mode Selected input syntax.
     * @param max_units Inclusive decoded-name limit for this recognition.
     * @return A measurement, successful nullopt on no-match,
     * or the same located failure consume_member_name reports for the same input.
     * @note Tries a quoted name first, then an identifier name in JSON5 mode only.
     * Failure and no-match preserve source.
     * Performs no allocation and needs no storage provider (ADR 0003, decision 7).
     */
    [[nodiscard]] result<std::optional<member_name_measure>> measure_member_name(cursor& source, syntax_mode mode,
                                                                                 std::size_t max_units) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* MEMBER_NAME_LEXER_HPP_ */
