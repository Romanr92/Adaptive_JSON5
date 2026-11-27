#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_NORMALIZATION_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_NORMALIZATION_HPP_

#include "schema/idna.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

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

  /**
   * @brief Capacity of a label during normalization: four code points (the longest canonical
   * decomposition) per code point of the longest label.
   * @par Requirements
   * J5-069; Unicode UAX #15.
   */
  inline constexpr std::size_t c_max_decomposed_label = c_max_label_code_points * 4U;

  /**
   * @brief Code points of a label during normalization.
   * @par Requirements
   * J5-069; Unicode UAX #15.
   */
  using decomposed_label = std::array<std::uint32_t, c_max_decomposed_label>;

  /**
   * @brief Normalize the first count code points of label to Unicode Normalization Form C in
   * place.
   * @par Requirements
   * J5-069; UTS #46 section 4; Unicode UAX #15; ADR 0006 (issue #187).
   * @note Fixed storage, no allocation, no recursion. A label in NFC of at most
   * c_max_label_code_points code points always fits: its canonical decomposition, which every
   * canonically equivalent input shares, has at most c_max_decomposed_label code points.
   * @return The normalized length, or nullopt when the canonical decomposition does not fit.
   * @par ID SWU-SCHEMA-438
   */
  [[nodiscard]] std::optional<std::size_t> normalize_nfc(decomposed_label& label, std::size_t count) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_NORMALIZATION_HPP_ */
