#ifndef ADAPTIVE_JSON5_SECURITY_REASON_HPP_
#define ADAPTIVE_JSON5_SECURITY_REASON_HPP_

#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Name the secure-mode rejection that stopped the parse.
   * @par Requirements
   * CSR-07; issue #145.
   * @par Reasons
   * - zero_byte: A string or member name contains U+0000, raw or escaped, and
   *   parse_options::zero_bytes is zero_byte_policy::reject.
   * - name_control_character: A member name contains U+0000 to U+001F, raw or escaped, and
   *   parse_options::name_controls is name_control_policy::reject.
   * - unpaired_surrogate: An escape decodes to an unpaired surrogate and
   *   parse_options::unpaired_surrogates is unpaired_surrogate_policy::reject.
   * @note The reason refines error_code::security_policy; it never replaces the error code.
   * U+0000 in a member name is reported as zero_byte while zero_bytes rejects, otherwise
   * as name_control_character while name_controls rejects.
   * Duplicate names keep error_code::duplicate_policy. Values are stable; new reasons are
   * only appended.
   */
  enum class security_reason : std::uint8_t
  {
    zero_byte,
    name_control_character,
    unpaired_surrogate
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SECURITY_REASON_HPP_ */
