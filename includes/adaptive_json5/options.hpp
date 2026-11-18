#ifndef OPTIONS_HPP_
#define OPTIONS_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"

#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Select the input language.
   * @par Requirements
   * J5-005.
   * @note Selection does not permit additional nonstandard syntax.
   */
  enum class syntax_mode : std::uint8_t
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
   * reject_duplicates compares each name with the earlier names of its object, so work grows
   * with the square of the member count: one object with 1024 members needs about 524 000
   * work units. Raise resource_limits::max_work_units when using it with large objects.
   * Duplicates are checked after syntax and limits, so another error in the same document
   * is reported instead of the duplicate (ADR 0003, decision 7, C1).
   * secure_parse_options() selects reject_duplicates (CSR-07).
   * @warning Secure mode is not fully standard JSON or JSON5. It deliberately rejects some
   * documents that the standards allow: duplicate member names, zero bytes and control
   * characters in strings or member names, and unpaired surrogates. A valid document from
   * another system may therefore fail to parse. Use secure mode only when you control both
   * ends, or when rejecting these cases is what you want.
   */
  enum class duplicate_name_policy : std::uint8_t
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
  enum class source_retention_policy : std::uint8_t
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
  enum class strict_bom_policy : std::uint8_t
  {
    reject,
    skip_one
  };

  /**
   * @brief Select how the warning table of a parsed document is allocated.
   * @par Requirements
   * P039; J5-017; J5-046; J5-047.
   * @par Policies
   * - fixed: the first warning allocates max_diagnostics entries at once, and the
   *   table never grows. Default; the worst case is known at configuration time.
   * - adaptive: the first warning allocates min(max_diagnostics, 64) entries; the
   *   table then doubles as needed, capped at max_diagnostics.
   * @note A document without warnings allocates nothing under either policy.
   * More warnings than max_diagnostics fail the parse with resource_limit under either policy.
   */
  enum class warning_storage_policy : std::uint8_t
  {
    fixed,
    adaptive
  };

  /**
   * @brief Select how parsing handles U+0000 in strings and member names.
   * @par Requirements
   * CSR-07; issue #145.
   * @par Policies
   * - preserve: Keep the zero byte in the decoded value, as the standards allow. Default.
   * - reject: Fail the parse with error_code::security_policy and
   *   security_reason::zero_byte; source_range() covers the raw character or the whole escape.
   * @note Covers the raw character and the escapes `\0`, `\x00` and `\u0000`; strict JSON
   * has only `\u0000`, because a raw U+0000 is already a syntax error there.
   * In a member name, name_control_policy::reject still rejects U+0000 when this policy is
   * preserve.
   * Within one string or name, syntax and encoding errors are reported first; otherwise the
   * first error in source order wins. Applies in either syntax mode.
   * @warning Secure mode is not fully standard JSON or JSON5. It deliberately rejects some
   * documents that the standards allow: duplicate member names, zero bytes and control
   * characters in strings or member names, and unpaired surrogates. A valid document from
   * another system may therefore fail to parse. Use secure mode only when you control both
   * ends, or when rejecting these cases is what you want.
   */
  enum class zero_byte_policy : std::uint8_t
  {
    preserve,
    reject
  };

  /**
   * @brief Select how parsing handles control characters U+0000 to U+001F in member names.
   * @par Requirements
   * CSR-07; issue #145.
   * @par Policies
   * - preserve: Keep the character in the decoded name, as the standards allow. Default.
   * - reject: Fail the parse with error_code::security_policy and
   *   security_reason::name_control_character; source_range() covers the raw character or
   *   the whole escape.
   * @note Covers raw characters and escapes such as `\n`, `\x1F` and `\u001F`. Only quoted
   * names are affected, because identifier names cannot contain control characters.
   * String values are not checked by this policy; see zero_byte_policy for U+0000.
   * U+0000 in a name is reported as security_reason::zero_byte while zero_bytes rejects;
   * this policy still rejects it when zero_bytes is switched to preserve.
   * Within one string or name, syntax and encoding errors are reported first; otherwise the
   * first error in source order wins. Applies in either syntax mode.
   * @warning Secure mode is not fully standard JSON or JSON5. It deliberately rejects some
   * documents that the standards allow: duplicate member names, zero bytes and control
   * characters in strings or member names, and unpaired surrogates. A valid document from
   * another system may therefore fail to parse. Use secure mode only when you control both
   * ends, or when rejecting these cases is what you want.
   */
  enum class name_control_policy : std::uint8_t
  {
    preserve,
    reject
  };

  /**
   * @brief Select how parsing handles escapes that decode to an unpaired surrogate.
   * @par Requirements
   * CSR-07; issue #145.
   * @par Policies
   * - warn: Keep the surrogate unit and record string_warning_code::unpaired_surrogate_escape.
   *   Default.
   * - reject: Fail the parse with error_code::security_policy and
   *   security_reason::unpaired_surrogate; source_range() covers the whole escape.
   * @note Applies to strings and member names in either syntax mode. A high surrogate pairs
   * only with a low surrogate that is the next decoded unit. Within one string or name,
   * syntax and encoding errors are reported first; otherwise the first error in source order
   * wins.
   * @warning Secure mode is not fully standard JSON or JSON5. It deliberately rejects some
   * documents that the standards allow: duplicate member names, zero bytes and control
   * characters in strings or member names, and unpaired surrogates. A valid document from
   * another system may therefore fail to parse. Use secure mode only when you control both
   * ends, or when rejecting these cases is what you want.
   */
  enum class unpaired_surrogate_policy : std::uint8_t
  {
    warn,
    reject
  };

  /**
   * @brief Configure syntax, resource limits, duplicate-name handling and secure mode.
   * @par Requirements
   * P031; P036; P039; P040; J5-005; J5-025; J5-032; J5-041; J5-047; CSR-07.
   * @note JSON5 and preserve_all are the defaults.
   * Duplicate-name policy applies independently within each object.
   * Resource limits use the defaults documented in resource_limits.
   * This structure owns its configuration and performs no allocation.
   * Storing configuration here does not itself enforce it.
   * Storage providers are specified separately.
   * Source retention is off by default; retaining charges the input size to accounting.
   * Strict mode rejects a leading byte order mark unless strict_bom selects skip_one.
   * The secure-mode fields default to standard behavior; secure_parse_options() turns them all
   * on, and each can then be switched off on its own.
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

      // A fixed table has a worst case known in advance, so it suits embedded targets.
      warning_storage_policy warning_storage = warning_storage_policy::fixed;

      // Secure-mode rejections (CSR-07). Off by default to keep full standard behavior.
      zero_byte_policy zero_bytes = zero_byte_policy::preserve;
      name_control_policy name_controls = name_control_policy::preserve;
      unpaired_surrogate_policy unpaired_surrogates = unpaired_surrogate_policy::warn;
  };

  /**
   * @brief Return default options with secure mode on: every security rejection enabled.
   * @par Requirements
   * CSR-07; J5-032; issue #145.
   * @return Default parse_options with duplicate_names set to reject_duplicates, and
   * zero bytes, member-name control characters and unpaired surrogates set to reject.
   * @note Secure mode is not stored as a separate flag. To switch off one rejection, change its
   * field on the result; the others stay on. Performs no allocation.
   * @par ID SWU-API-043
   * @warning Secure mode is not fully standard JSON or JSON5. It deliberately rejects some
   * documents that the standards allow: duplicate member names, zero bytes and control
   * characters in strings or member names, and unpaired surrogates. A valid document from
   * another system may therefore fail to parse. Use secure mode only when you control both
   * ends, or when rejecting these cases is what you want.
   */
  [[nodiscard]] constexpr parse_options secure_parse_options() noexcept
  {
    parse_options o{};
    o.duplicate_names = duplicate_name_policy::reject_duplicates;
    o.zero_bytes = zero_byte_policy::reject;
    o.name_controls = name_control_policy::reject;
    o.unpaired_surrogates = unpaired_surrogate_policy::reject;
    return o;
  }

} // namespace adaptive_json5
#endif /* OPTIONS_HPP_ */
