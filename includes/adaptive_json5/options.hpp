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
   * @brief Configure syntax, resource limits and duplicate-name handling.
   * @par Requirements
   * P031; P036; J5-005; J5-032; J5-047.
   * @note JSON5 and preserve_all are the defaults.
   * Duplicate-name policy applies independently within each object.
   * Resource limits use the defaults documented in resource_limits.
   * This structure owns its configuration and performs no allocation.
   * Storing configuration here does not itself enforce it.
   * Storage providers are specified separately.
   */
  struct [[nodiscard]] parse_options
  {
      syntax_mode syntax = syntax_mode::json5;
      resource_limits limits{};

      // Preserve existing generic behavior unless the caller requests rejection.
      duplicate_name_policy duplicate_names = duplicate_name_policy::preserve_all;
  };
} // namespace adaptive_json5
#endif /* OPTIONS_HPP_ */
