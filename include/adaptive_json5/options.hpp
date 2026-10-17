#ifndef OPTIONS_HPP_
#define OPTIONS_HPP_

#include "resource_limits.hpp"

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
   * @brief Configure the reader's syntax selection.
   * @par Requirements
   * P031; J5-005; J5-047.
   * @note JSON5 is the default. Strict JSON requires explicit selection.
   * Resource limits use the defaults documented in resource_limits.
   * This structure owns its configuration and performs no allocation.
   * Storing limits here does not itself enforce them.
   * Storage providers and other policies are specified separately.
   */
  struct parse_options
  {
      syntax_mode syntax = syntax_mode::json5;
      resource_limits limits{};
  };
} // namespace adaptive_json5
#endif /* OPTIONS_HPP_ */
