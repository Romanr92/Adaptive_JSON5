#ifndef ADAPTIVE_JSON5_INTERNAL_LEXER_MEMBER_NAME_LEXER_HPP_
#define ADAPTIVE_JSON5_INTERNAL_LEXER_MEMBER_NAME_LEXER_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "input/cursor.hpp"
#include "lexer/string/string_checks.hpp"
#include "lexer/string/string_token.hpp"

#include <cstddef>
#include <limits>
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
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
    // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
    // implicit members that are never odr-used are not in its database.
    // codeql::misra_deviation_next_line(DEV-020.03)
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
     * excess_warning is the quoted name's first warning past the measuring allowance.
     * Owns nothing, so it can be produced and discarded without a storage provider.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
    // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
    // implicit members that are never odr-used are not in its database.
    // codeql::misra_deviation_next_line(DEV-020.02)
    struct [[nodiscard]] member_name_measure
    {
        std::size_t units{};
        // MISRA rule: RULE-0-2-1 cpp/misra/unused-limited-visibility-variable
        // MISRA justification: Part of the span result returned to callers, so the result describes the
        // complete recognized range. Unit tests read the member to verify the range; production callers that
        // only need success ignore it.
        // codeql::misra_deviation_next_line(DEV-008.07)
        std::size_t byte_offset{};
        // MISRA rule: RULE-0-2-1 cpp/misra/unused-limited-visibility-variable
        // MISRA justification: Part of the span result returned to callers, so the result describes the
        // complete recognized range. Unit tests read the member to verify the range; production callers that
        // only need success ignore it.
        // codeql::misra_deviation_next_line(DEV-008.06)
        std::size_t byte_length{};
        std::size_t warning_count{};
        std::optional<string_warning> excess_warning{};
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
     * @param checks Secure-mode checks for quoted names, normally name_string_checks();
     * the default applies none (CSR-07). Identifier names are not checked: their grammar
     * already excludes these characters.
     * @return Owned name, successful nullopt on no-match, or an explicit failure.
     * @note Failure and no-match preserve source.
     * Success consumes only the name, leaving the colon unconsumed.
     * Does not classify reserved words as value literals.
     * Does not skip trivia, update frames or invoke warning callbacks; identifier names never use the sink.
     * The caller handles shared resource and work accounting.
     * @par ID SWU-LEXER-012
     */
    [[nodiscard]] result<std::optional<member_name_token>>
    consume_member_name(cursor& source, syntax_mode mode, const storage_callbacks& storage, std::size_t max_units,
                        const string_warning_sink* sink = nullptr, string_checks checks = {}) noexcept;

    /**
     * @brief Recognize a member name like consume_member_name, but measure it without allocating.
     * @par Requirements
     * P033; P035; J5-017; J5-019; J5-020; J5-041; J5-046; J5-047.
     * @param source Cursor positioned after caller-managed trivia.
     * @param mode Selected input syntax.
     * @param max_units Inclusive decoded-name limit for this recognition.
     * @param warning_allowance Warnings that still fit the caller's diagnostic budget; the default never limits.
     * Only quoted names can warn; identifier names leave excess_warning empty.
     * @param checks Secure-mode checks for quoted names, normally name_string_checks();
     * the default applies none (CSR-07). Identifier names are not checked: their grammar
     * already excludes these characters.
     * @return A measurement, successful nullopt on no-match,
     * or the same located failure consume_member_name reports for the same input.
     * @note Tries a quoted name first, then an identifier name in JSON5 mode only.
     * Failure and no-match preserve source.
     * Performs no allocation and needs no storage provider (ADR 0003, decision 7).
     * @par ID SWU-LEXER-013
     */
    [[nodiscard]] result<std::optional<member_name_measure>>
    measure_member_name(cursor& source, syntax_mode mode, std::size_t max_units,
                        std::size_t warning_allowance = std::numeric_limits<std::size_t>::max(),
                        string_checks checks = {}) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_INTERNAL_LEXER_MEMBER_NAME_LEXER_HPP_ */
