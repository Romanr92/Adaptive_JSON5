#ifndef ADAPTIVE_JSON5_CLI_RUN_COMMAND_HPP_
#define ADAPTIVE_JSON5_CLI_RUN_COMMAND_HPP_

#include "adaptive_json5/value/value_view.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  /// @brief Bytes reserved for reading a manifest (larger than one example input).
  inline constexpr std::size_t c_manifest_bytes = 1U << 20U;

  /**
   * @brief Resolve an RFC 6901 JSON Pointer ("" is the root) in a parsed document.
   * @par Requirements
   * P058; J5-070.
   * @return The value, or nullopt when a token names no member or element.
   * @par ID SWU-CLI-079
   */
  [[nodiscard]] std::optional<value_view> resolve_pointer(const value_view& root, std::string_view pointer) noexcept;

  /**
   * @brief The run command: evaluate every manifest case (parse, optional schema validation,
   * expected exit and structured checks) and summarise per case and per requirement.
   * @par Requirements
   * P058; J5-070; J5-071; J5-094.
   * @return 0 when every selected supported expectation matched (intentionally invalid
   * inputs included), 1 on any mismatch, 2 when the manifest or an input cannot be processed.
   * Unsupported capabilities are reported and never count as passing.
   * @par ID SWU-CLI-080
   */
  [[nodiscard]] std::int32_t run_manifest(const options& selected, const environment& host) noexcept;
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_RUN_COMMAND_HPP_ */
