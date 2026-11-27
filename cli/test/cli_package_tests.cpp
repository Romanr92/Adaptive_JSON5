#include "cli/cli_version.hpp"
#include "cli_fakes.hpp"

#include <gtest/gtest.h>
#include <string>

namespace
{
  using adaptive_json5_test::fake_host;

  /* =================================== Test_P059_Version =================================== */
  /*
   * Arrange: The version command.
   * Act: Run it.
   * Expect: The tool version (independent of the library), the library version, the source
   *         revision and the schema and exception build options, one per line (J5-093).
   */
  TEST(P059_Version, ReportsToolLibraryAndBuildOptions)
  {
    fake_host host;
    EXPECT_EQ(host.run({"version"}), 0);
    const std::string tool = std::string{adaptive_json5::cli::c_tool_version};
    const std::string library = std::string{adaptive_json5::cli::c_library_version};
    EXPECT_EQ(host.out.find("adaptive_json5_examples " + tool + "\n"), 0U) << host.out;
    EXPECT_NE(host.out.find("library " + library + "\n"), std::string::npos);
    EXPECT_NE(host.out.find("revision " + std::string{adaptive_json5::cli::c_source_revision} + "\n"),
              std::string::npos);
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    EXPECT_NE(host.out.find("schema on\n"), std::string::npos);
#else
    EXPECT_NE(host.out.find("schema off\n"), std::string::npos);
#endif
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    EXPECT_NE(host.out.find("exceptions on\n"), std::string::npos);
#else
    EXPECT_NE(host.out.find("exceptions off\n"), std::string::npos);
#endif
    EXPECT_FALSE(tool.empty());
    EXPECT_FALSE(library.empty());
    host.fail_writes = true;
    EXPECT_EQ(host.run({"version"}), 2);
  }

  /*
   * Arrange: The capabilities command.
   * Act: Run it.
   * Expect: The same versions and revision as version, every command and the exception mode
   *         in the machine-readable output (J5-071, J5-093).
   */
  TEST(P059_Version, CapabilitiesCarryTheVersions)
  {
    fake_host host;
    EXPECT_EQ(host.run({"capabilities"}), 0);
    EXPECT_NE(host.out.find("\"tool_version\": \"" + std::string{adaptive_json5::cli::c_tool_version} + "\""),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find("\"library_version\": \"" + std::string{adaptive_json5::cli::c_library_version} + "\""),
              std::string::npos);
    EXPECT_NE(host.out.find(R"("commands": ["validate", "inspect", "run", "capabilities", "version", "help"])"),
              std::string::npos);
    EXPECT_NE(host.out.find("\"exceptions\": "), std::string::npos);
  }
} // namespace
