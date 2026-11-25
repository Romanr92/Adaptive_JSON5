#ifndef ADAPTIVE_JSON5_CLI_HOST_IO_HPP_
#define ADAPTIVE_JSON5_CLI_HOST_IO_HPP_

#include "cli/cli_io.hpp"

namespace adaptive_json5::cli
{
  /**
   * @brief The real host environment: files and standard input through <cstdio>, standard
   * output and error, and malloc-backed storage.
   * @par Requirements
   * P056; J5-070; ADR 0007 (host-only; heap allowed).
   * @par ID SWU-CLI-023
   */
  [[nodiscard]] environment host_environment() noexcept;
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_HOST_IO_HPP_ */
