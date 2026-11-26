#ifndef ADAPTIVE_JSON5_CLI_INSPECT_COMMAND_HPP_
#define ADAPTIVE_JSON5_CLI_INSPECT_COMMAND_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "cli/cli_app.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"

namespace adaptive_json5::cli
{
  /**
   * @brief Write a decoded string as a strict-JSON value: an escaped string, or
   * {"$utf16": [units...]} when it holds unpaired surrogates (ADR 0007 D2).
   * @par Requirements
   * P058; J5-071.
   * @return false when scratch storage for the UTF-8 copy could not be allocated.
   * @par ID SWU-CLI-060
   */
  [[nodiscard]] bool put_json_decoded(text_writer& out, const decoded_string& text,
                                      const storage_callbacks& scratch) noexcept;

  /**
   * @brief The inspect command: parse the input and show every value in source order with
   * kind, position, raw number spellings, checked conversions and string warnings.
   * @par Requirements
   * P058; J5-041; J5-045; J5-070; J5-071.
   * @return 0 parsed; 1 invalid input; 2 I/O, limit or operational failure.
   * @par ID SWU-CLI-061
   */
  [[nodiscard]] int run_inspect(const options& selected, const environment& host) noexcept;
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_INSPECT_COMMAND_HPP_ */
