#include "cli/cli_app.hpp"
#include "cli/host_io.hpp"

/**
 * @brief Entry point of adaptive_json5_examples: run the tool with the real host environment.
 * @par Requirements
 * P056; J5-070; ADR 0009.
 * @note The tool is noexcept throughout; when exceptions are enabled, anything escaping
 * (for example from the C++ runtime) still ends in exit 2.
 * @par ID SWU-CLI-029
 */
// MISRA rule: RULE-18-3-1 cpp/misra/missing-catch-all-exception-handler-in-main
// MISRA justification: Exception-enabled builds wrap dispatch in catch (...). The scanned exception-disabled build
// dispatches through noexcept interfaces.
// codeql::misra_deviation_next_line(DEV-215.1)
int main(int argc, char** argv)
{
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try
  {
    return adaptive_json5::cli::run_cli(argc, argv, adaptive_json5::cli::host_environment());
  }
  catch (...)
  {
    return adaptive_json5::cli::exit_failure;
  }
#else
  return adaptive_json5::cli::run_cli(argc, argv, adaptive_json5::cli::host_environment());
#endif
}
