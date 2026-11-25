#ifndef ADAPTIVE_JSON5_CLI_CLI_APP_HPP_
#define ADAPTIVE_JSON5_CLI_CLI_APP_HPP_

#include "cli/cli_io.hpp"

#include <cstddef>
#include <string_view>

namespace adaptive_json5::cli
{
  /**
   * @brief Process exit codes (J5-070).
   * @par Requirements
   * P056; P057; P058; J5-070.
   * @note validate/inspect: valid, invalid input/assertion/profile, configuration/I/O/operational.
   * run: every selected supported expectation matched, a mismatch, operational failure.
   */
  enum exit_code : int
  {
    exit_ok = 0,
    exit_invalid = 1,
    exit_failure = 2
  };

  /**
   * @brief Writes text pieces to one output stream without allocating; remembers write failures.
   * @par Requirements
   * P056; J5-071.
   */
  class text_writer
  {
    private:
      const output_sink* m_sink;
      output_stream m_stream;
      bool m_ok{true};

    public:
      /**
       * @brief Bind the writer to a sink and stream.
       * @par Requirements
       * P056.
       * @par ID SWU-CLI-010
       */
      text_writer(const output_sink& sink, output_stream stream) noexcept;

      /**
       * @brief Write text; ignored after a failed write.
       * @par Requirements
       * P056; J5-071.
       * @par ID SWU-CLI-011
       */
      text_writer& put(std::string_view text) noexcept;

      /**
       * @brief Write an unsigned decimal number.
       * @par Requirements
       * P056; J5-071.
       * @par ID SWU-CLI-012
       */
      text_writer& put_count(std::size_t value) noexcept;

      /**
       * @brief Return false when any write failed.
       * @par Requirements
       * P056; J5-071.
       * @par ID SWU-CLI-013
       */
      [[nodiscard]] bool ok() const noexcept;
  };

  /**
   * @brief Run the tool: parse the arguments, dispatch the command and return the exit code.
   * @par Requirements
   * P056; P057; P058; J5-070; ADR 0007.
   * @note All input, output and memory go through host; nothing else is touched.
   * @par ID SWU-CLI-014
   */
  [[nodiscard]] int run_cli(int argc, const char* const* argv, const environment& host) noexcept;
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_CLI_APP_HPP_ */
