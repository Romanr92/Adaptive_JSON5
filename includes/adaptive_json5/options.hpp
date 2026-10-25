#ifndef OPTIONS_HPP_
#define OPTIONS_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"

namespace adaptive_json5
{
  /**
   * @brief Select the input language.
   * @par Requirements
   * J5-005.
   * @note Selection does not permit additional nonstandard syntax.
   */
  enum class syntax_mode
  {
    json5,
    strict_json
  };

  /**
   * @brief Select how parsing handles equal decoded names within an object.
   * @par Requirements
   * P036; J5-032; J5-033.
   * @note Equality uses exact decoded UTF-16 units.
   * The policy applies independently within each object in either syntax mode.
   */
  enum class duplicate_name_policy
  {
    preserve_all,
    reject_duplicates
  };

  /**
   * @brief Select whether a parsed document keeps a copy of its input.
   * @par Requirements
   * P039; J5-041.
   * @par Policies
   * - discard: Keep no input bytes; the caller's input may be destroyed after parsing.
   * - retain_copy: Copy the complete input into the parsed document after parsing succeeds.
   * @note The copy is charged to the caller's accounting against max_owned_bytes.
   */
  enum class source_retention_policy
  {
    discard,
    retain_copy
  };

  /**
   * @brief Select how strict JSON mode treats a UTF-8 byte order mark at the start of input.
   * @par Requirements
   * P040; J5-025.
   * @par Policies
   * - reject: Report a leading byte order mark as a syntax error at offset 0.
   * - skip_one: Ignore exactly one leading byte order mark; any further one is still rejected.
   * @note JSON5 mode ignores this policy because its whitespace grammar already admits the mark.
   * Skipping keeps original byte offsets in every error, span and warning.
   */
  enum class strict_bom_policy
  {
    reject,
    skip_one
  };

  /**
   * @brief Configure syntax, resource limits and duplicate-name handling.
   * @par Requirements
   * P031; P036; P039; P040; J5-005; J5-025; J5-032; J5-041; J5-047.
   * @note JSON5 and preserve_all are the defaults.
   * Duplicate-name policy applies independently within each object.
   * Resource limits use the defaults documented in resource_limits.
   * This structure owns its configuration and performs no allocation.
   * Storing configuration here does not itself enforce it.
   * Storage providers are specified separately.
   * Source retention is off by default; retaining charges the input size to accounting.
   * Strict mode rejects a leading byte order mark unless strict_bom selects skip_one.
   */
  struct [[nodiscard]] parse_options
  {
      syntax_mode syntax = syntax_mode::json5;
      resource_limits limits{};

      // Preserve existing generic behavior unless the caller requests rejection.
      duplicate_name_policy duplicate_names = duplicate_name_policy::preserve_all;

      // Retaining input costs a full copy, so it is opt-in.
      source_retention_policy source_text = source_retention_policy::discard;

      // JSON permits ignoring a leading byte order mark, but only when the caller asks for it.
      strict_bom_policy strict_bom = strict_bom_policy::reject;
  };
} // namespace adaptive_json5
#endif /* OPTIONS_HPP_ */
