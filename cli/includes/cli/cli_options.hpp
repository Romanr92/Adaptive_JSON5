#ifndef ADAPTIVE_JSON5_CLI_CLI_OPTIONS_HPP_
#define ADAPTIVE_JSON5_CLI_CLI_OPTIONS_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/options.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  /**
   * @brief Commands of adaptive_json5_examples.
   * @par Requirements
   * P056; P058; J5-070.
   */
  enum class command : std::uint8_t
  {
    help,
    version,
    capabilities,
    validate, // Parse (and optionally schema-validate) one input.
    inspect,  // Parse one input and show its values (P058).
    run       // Run the example manifest (P058).
  };

  /**
   * @brief Diagnostic output format.
   * @par Requirements
   * P056; P057; J5-071.
   */
  enum class diagnostics_format : std::uint8_t
  {
    text,
    json
  };

  /**
   * @brief Parsed command line; text fields view the argument strings.
   * @par Requirements
   * P056; J5-070.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: The type relies on its member types for copy, move and destruction, or explicitly restricts
  // ownership transfer.
  // codeql::misra_deviation_next_line(DEV-204.7)
  struct options
  {
      command selected = command::help;
      std::optional<syntax_mode> syntax{}; // Required for validate and inspect.
      duplicate_name_policy duplicates = duplicate_name_policy::preserve_all;
      resource_limits limits{};
      diagnostics_format diagnostics = diagnostics_format::text;
      std::string_view input{};    // Path, or "-" for standard input.
      std::string_view schema{};   // Optional schema path (validate).
      std::string_view manifest{}; // Manifest path (run).
  };

  /**
   * @brief Why a command line was rejected.
   * @par Requirements
   * P056; J5-070.
   */
  enum class argument_error_code : std::uint8_t
  {
    missing_command,
    unknown_command,
    unknown_option,
    missing_value,
    invalid_value,
    repeated_option,
    option_not_allowed, // The option does not apply to the command.
    missing_input,
    extra_argument,
    missing_syntax
  };

  /**
   * @brief A rejected command line and the argument that caused it.
   * @par Requirements
   * P056; J5-070.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: The type relies on its member types for copy, move and destruction, or explicitly restricts
  // ownership transfer.
  // codeql::misra_deviation_next_line(DEV-204.6)
  struct argument_error
  {
      argument_error_code code = argument_error_code::missing_command;
      std::string_view argument{};
  };

  /**
   * @brief Outcome of parse_arguments(): options or an argument error.
   * @par Requirements
   * P056; J5-070.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: The type relies on its member types for copy, move and destruction, or explicitly restricts
  // ownership transfer.
  // codeql::misra_deviation_next_line(DEV-204.5)
  struct parsed_arguments
  {
      std::optional<options> parsed{};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Aggregate initialization supplies the declared member defaults or value-initializes an
      // unsigned member to zero.
      // codeql::misra_deviation_next_line(DEV-203.1)
      argument_error failure{};
  };

  /**
   * @brief Parse argv (argv[0] is the program name) without allocating.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-001
   */
  [[nodiscard]] parsed_arguments parse_arguments(std::int32_t argc, const char* const* argv) noexcept;

  /**
   * @brief Short human-readable text of an argument error code.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-002
   */
  [[nodiscard]] std::string_view describe(argument_error_code code) noexcept;
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_CLI_OPTIONS_HPP_ */
