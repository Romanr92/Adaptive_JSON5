#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_UNICODE_PROPERTY_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_UNICODE_PROPERTY_HPP_

#include "schema/unicode_properties.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  /**
   * @brief Category bit mask of a General_Category value name or alias (L, Letter, Nd, digit, ...).
   * @par Requirements
   * J5-060; ADR 0006 (issue #187).
   * @return The mask (one bit per category of c_general_category_ranges), or nullopt.
   * @par ID SWU-SCHEMA-359
   */
  [[nodiscard]] std::optional<std::uint32_t> general_category_mask(std::string_view name) noexcept;

  /**
   * @brief Script number of a script name or alias (Greek, Grek, ...).
   * @par Requirements
   * J5-060; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-360
   */
  [[nodiscard]] std::optional<std::uint32_t> script_number(std::string_view name) noexcept;

  /**
   * @brief Category bit of a code point (c_unassigned_category_mask for unassigned ones).
   * @par Requirements
   * J5-060; J5-069; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-361
   */
  [[nodiscard]] std::uint32_t general_category_bit(std::uint32_t code_point) noexcept;

  /**
   * @brief Script number of a code point (c_unknown_script when it has none).
   * @par Requirements
   * J5-060; J5-069; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-362
   */
  [[nodiscard]] std::uint32_t script_of(std::uint32_t code_point) noexcept;

  /**
   * @brief Joining_Type of a code point; nullopt for U (non-joining).
   * @par Requirements
   * J5-069; RFC 5892 Appendix A.2; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-363
   */
  [[nodiscard]] std::optional<joining_type> joining_type_of(std::uint32_t code_point) noexcept;

  /**
   * @brief Report whether a code point has Canonical_Combining_Class Virama (9).
   * @par Requirements
   * J5-069; RFC 5892 Appendix A.1 and A.2; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-364
   */
  [[nodiscard]] bool is_virama(std::uint32_t code_point) noexcept;

  /**
   * @brief IDNA2008 derived property of a code point; nullopt for DISALLOWED or UNASSIGNED.
   * @par Requirements
   * J5-069; RFC 5892 section 3; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-365
   */
  [[nodiscard]] std::optional<idna_property> idna_property_of(std::uint32_t code_point) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_UNICODE_PROPERTY_HPP_ */
