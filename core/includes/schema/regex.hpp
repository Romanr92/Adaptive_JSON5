#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_REGEX_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_REGEX_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "schema/pod_vector.hpp"
#include "schema/regex_program.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_work.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5::detail
{
  /**
   * @brief Compile an ECMA-262 pattern (the Draft-07 regex dialect) into a Pike VM program.
   * @par Requirements
   * P049; J5-054; J5-060; ADR 0006 (owner decision 2026-11-22: own linear-time engine).
   * @param pattern Decoded pattern text (UTF-16 units; matched as code points).
   * @param code Instruction table; the program is appended.
   * @param ranges Class range table; the program's ranges are appended.
   * @param scratch Storage for the parse tree, released before returning.
   * @param steps Compile steps used so far; one is charged per parsed item and per instruction.
   * @param step_limit Inclusive max_compile_steps.
   * @param span Source range of the pattern string, for errors.
   * @return The program as a range of the instruction table, or invalid_schema (invalid_pattern)
   * for malformed patterns and unknown property names, invalid_schema (unsupported_pattern) for
   * backreferences, lookaround, named groups, other binary properties and Script_Extensions, resource_limit
   * (max_compile_steps) or a storage failure.
   * @note Supported: literals and escapes (\t \n \v \f \r \0 \xHH \uHHHH, \cX, identity escapes),
   * `.`, classes with ranges, negation and \d \D \w \W \s \S, Unicode property escapes \p{...}
   * and \P{...} (General_Category, Script, Any, ASCII, Assigned; Unicode 17.0.0; issue #187),
   * ^ $ \b \B, groups and (?:...),
   * alternation, * + ? {n} {n,} {n,m} and their lazy forms. `{` that does not start a valid
   * quantifier and unmatched `]` or `}` are literals (ECMA-262 Annex B). Matching works on
   * code points, so `.` matches one code point, also an astral one. No recursion.
   * @par ID SWU-SCHEMA-232
   */
  [[nodiscard]] result<node_range, schema_error>
  compile_regex(const decoded_string& pattern, pod_vector<regex_inst>& code, pod_vector<regex_range>& ranges,
                const storage_callbacks& scratch, std::size_t& steps, std::size_t step_limit,
                std::optional<source_span> span) noexcept;

  /**
   * @brief Report whether a compiled pattern matches anywhere in text (unanchored search).
   * @par Requirements
   * P049; J5-057; J5-060; J5-065.
   * @param scratch Storage for the thread lists and decoded code points, released before returning.
   * @return The verdict, resource_limit (max_regex_steps) or a storage failure.
   * @note Pike VM: time is at most proportional to program size times text length; every
   * step is charged to max_regex_steps. text must hold no unpaired surrogate (instance check).
   * @par ID SWU-SCHEMA-233
   */
  [[nodiscard]] result<bool, schema_error> regex_search(const regex_tables& tables, node_range program,
                                                        const decoded_string& text, schema_work& work,
                                                        const storage_callbacks& scratch) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_REGEX_HPP_ */
