#include "cli/cli_app.hpp"
#include "cli/host_io.hpp"

/**
 * @brief Entry point of adaptive_json5_examples: run the tool with the real host environment.
 * @par Requirements
 * P056; J5-070; ADR 0007.
 * @par ID SWU-CLI-029
 */
int main(int argc, char** argv)
{
  return adaptive_json5::cli::run_cli(argc, argv, adaptive_json5::cli::host_environment());
}
