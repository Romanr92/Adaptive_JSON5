#ifndef ADAPTIVE_JSON5_CLI_VALIDATE_COMMAND_HPP_
#define ADAPTIVE_JSON5_CLI_VALIDATE_COMMAND_HPP_

#include "adaptive_json5/error.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"
#include "cli/report.hpp"

#include <cstddef>
#include <string_view>

namespace adaptive_json5::cli
{
  /**
   * @brief The validate command: parse the input with the selected syntax and limits, report
   * string warnings, and validate against --schema when given.
   * @par Requirements
   * P056; P057; J5-070; J5-071.
   * @return 0 valid; 1 invalid input or failed assertion; 2 configuration, I/O, schema or
   * operational failure.
   * @par ID SWU-CLI-044
   */
  [[nodiscard]] int run_validate(const options& selected, const environment& host) noexcept;

  /**
   * @brief The capabilities command: a strict-JSON description of commands, syntax modes,
   * diagnostic formats and schema support.
   * @par Requirements
   * P057; P059; J5-069; J5-071.
   * @par ID SWU-CLI-045
   */
  [[nodiscard]] int run_capabilities(const environment& host) noexcept;

  /**
   * @brief Diagnostic category of a parser failure.
   * @par Requirements
   * P057; J5-070; J5-071.
   * @par ID SWU-CLI-046
   */
  [[nodiscard]] category category_of(error_code code) noexcept;

  /**
   * @brief Read path into buffer through the host reader.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-047
   */
  [[nodiscard]] io_status read_input(const environment& host, std::string_view path, char* buffer, std::size_t capacity,
                                     std::size_t& size) noexcept;

  /**
   * @brief Text of a reader failure.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-020
   */
  [[nodiscard]] std::string_view describe_io(io_status status) noexcept;
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_VALIDATE_COMMAND_HPP_ */
