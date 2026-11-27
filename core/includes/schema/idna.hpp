#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_IDNA_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_IDNA_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  /**
   * @brief Longest label in code points (RFC 1035 section 2.3.4); a decoded A-label never exceeds it.
   * @par Requirements
   * J5-069; ADR 0006 (issue #187).
   */
  inline constexpr std::size_t c_max_label_code_points = 63U;

  /**
   * @brief Decode the Punycode part of an A-label (the text after "xn--").
   * @par Requirements
   * J5-069; RFC 3492 section 6.2; ADR 0006 (issue #187).
   * @return The number of decoded code points written to output, or nullopt for invalid input,
   * arithmetic overflow, a surrogate or out-of-range code point, or more than
   * c_max_label_code_points code points.
   * @par ID SWU-SCHEMA-377
   */
  [[nodiscard]] std::optional<std::size_t>
  punycode_decode(std::string_view input, std::array<std::uint32_t, c_max_label_code_points>& output) noexcept;

  /**
   * @brief Validate an A-label (a label starting with "xn--", ASCII case-insensitive).
   * @par Requirements
   * J5-069; RFC 5891 section 5.4; RFC 5892 section 3 and Appendix A; ADR 0006 (issue #187).
   * @return true when the Punycode decodes to a U-label that is in NFC and not all ASCII, has
   * no hyphen at either end or in positions 3 and 4, does not start with a combining mark,
   * and whose code points are PVALID or satisfy their CONTEXTJ/CONTEXTO rule. The RFC 5893
   * Bidi rule spans all labels and is checked by satisfies_bidi_rule.
   * @par ID SWU-SCHEMA-378
   */
  [[nodiscard]] bool is_a_label(std::string_view label) noexcept;

  /**
   * @brief Report whether a label starts with the ACE prefix "xn--" (ASCII case-insensitive).
   * @par Requirements
   * J5-069; RFC 5890 section 2.3.2.1; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-394
   */
  [[nodiscard]] bool has_ace_prefix(std::string_view label) noexcept;

  /**
   * @brief RFC 5893 Bidi rule of a host name whose labels are already valid: when any label
   * holds a right-to-left character (Bidi_Class R, AL or AN), every label must satisfy the six
   * conditions of RFC 5893 section 2; otherwise the rule does not apply.
   * @par Requirements
   * J5-069; RFC 5893 section 2; RFC 5891 section 5.4; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-410
   */
  [[nodiscard]] bool satisfies_bidi_rule(std::string_view host) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_IDNA_HPP_ */
