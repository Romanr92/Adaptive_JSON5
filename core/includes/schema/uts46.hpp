#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_UTS46_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_UTS46_HPP_

#include "schema/pod_vector.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5::detail
{
  /**
   * @brief Longest host name in ASCII form (RFC 1123 section 2.1, as format hostname checks it).
   * @par Requirements
   * J5-069.
   */
  inline constexpr std::size_t c_max_host_octets = 253U;

  /**
   * @brief ASCII form of a host name.
   * @par Requirements
   * J5-069.
   */
  using ascii_host = std::array<char, c_max_host_octets>;

  /**
   * @brief UTS #46 ToASCII of the code points [first, end) of text, nontransitional: ignored
   * code points are removed, mapped ones replaced (U+3002, U+FF0E and U+FF61 become '.'), each
   * label is normalized to NFC, and a label with a non-ASCII code point becomes an A-label
   * ("xn--" and Punycode). STD3 rules, label checks and the Bidi rule are left to the host name
   * check of the result.
   * @par Requirements
   * J5-069; UTS #46 sections 4 and 4.2; RFC 5891 section 4.4; ADR 0006 (issue #187).
   * @return The length of the ASCII form in out, or nullopt when a code point is disallowed, a
   * label cannot be in a host name of c_max_host_octets (its canonical decomposition or A-label
   * is too long) or the ASCII form is longer than c_max_host_octets.
   * @par ID SWU-SCHEMA-428
   */
  [[nodiscard]] std::optional<std::size_t> idn_to_ascii(const pod_vector<std::uint32_t>& text, std::size_t first,
                                                        std::size_t end, ascii_host& out) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_UTS46_HPP_ */
