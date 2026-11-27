#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_NORMALIZATION_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_NORMALIZATION_HPP_

#include "schema/idna.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace adaptive_json5::detail
{
  /**
   * @brief Report whether a label is in Unicode Normalization Form C.
   * @par Requirements
   * J5-069; RFC 5891 section 5.4; Unicode UAX #15; ADR 0006 (issue #187).
   * @note Normalizes a copy (canonical decomposition, canonical ordering, canonical composition,
   * Hangul included) in fixed storage and compares it with the label; no allocation, no
   * recursion. A label whose decomposition does not fit the storage is reported as not NFC.
   * @par ID SWU-SCHEMA-403
   */
  [[nodiscard]] bool is_nfc(const std::array<std::uint32_t, c_max_label_code_points>& label,
                            std::size_t count) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_NORMALIZATION_HPP_ */
