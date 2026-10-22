#ifndef RESOURCE_LIMITS_HPP_
#define RESOURCE_LIMITS_HPP_

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Configure finite resource ceilings for one operation.
   * @par Requirements
   * P031; J5-047; J5-048; J5-049.
   * @note Limits are inclusive: consuming exactly the limit is permitted.
   * Zero permits no consumption of the corresponding resource.
   * No value acts as an unlimited sentinel.
   * These settings describe limits; construction performs no allocation.
   * Enforcement and checked arithmetic belong to the resource guards.
   */
  struct [[nodiscard]] resource_limits
  {
      // Bound the complete supplied input, including whitespace and comments.
      std::size_t max_input_bytes = 16384;

      // Count simultaneously open arrays and objects; scalar roots have depth zero.
      std::size_t max_depth = 64;

      // Count all document values, including the root and container values.
      std::size_t max_nodes = 1024;

      // Count object members across the whole document, including duplicate names.
      std::size_t max_members = 1024;

      // Bound each decoded string value or member name in UTF-16 units.
      std::size_t max_string_units = 4096;

      // Bound the sum of decoded string-value and member-name units.
      std::size_t max_total_string_units = 16384;

      // Bound each complete numeric spelling in input bytes.
      std::size_t max_number_bytes = 1024;

      // Count exponent digits, including leading zeros but excluding its sign.
      std::size_t max_exponent_digits = 128;

      // Bound simultaneously outstanding requested allocation bytes.
      std::size_t max_owned_bytes = 32768;

      // Bound retained diagnostic entries, including warning entries.
      std::size_t max_diagnostics = 16;

      // Bound cumulative charged work using operation-specific accounting rules.
      std::size_t max_work_units = 32768;
  };
} // namespace adaptive_json5

#endif /* RESOURCE_LIMITS_HPP_ */
