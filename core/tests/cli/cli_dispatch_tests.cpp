#include "cli/cli_app.hpp"
#include "cli/cli_options.hpp"
#include "cli_fakes.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{
  using adaptive_json5::cli::argument_error_code;
  using adaptive_json5::cli::command;
  using adaptive_json5::cli::io_status;
  using adaptive_json5::cli::parse_arguments;
  using adaptive_json5_test::fake_host;

  /**
   * @brief A command line that owns its strings, so the parsed views stay valid.
   * @par Requirements
   * P056; J5-070.
   */
  struct command_line
  {
      std::vector<std::string> strings;
      std::vector<const char*> argv;
      adaptive_json5::cli::parsed_arguments result;

      explicit command_line(std::vector<std::string> arguments) : strings(std::move(arguments))
      {
        strings.insert(strings.begin(), "adaptive_json5_examples");
        for (const std::string& argument : strings)
        {
          argv.push_back(argument.c_str());
        }
        result = parse_arguments(static_cast<int>(argv.size()), argv.data());
      }
  };

  /* ================================== Test_P056_Arguments ================================== */
  /*
   * Arrange: Valid command lines for each command and option.
   * Act: Parse them.
   * Expect: The command, syntax, duplicate policy, limits, schema, manifest, diagnostics
   *         format and input are taken over exactly (J5-070).
   */
  TEST(P056_Arguments, ParsesEveryOption)
  {
    const command_line full_line(
      {"validate", "--syntax",           "json", "--duplicates", "reject", "--schema",    "s.json", "--diagnostics",
       "json",     "--max-input-bytes",  "100",  "--max-depth",  "7",      "--max-nodes", "8",      "--max-members",
       "9",        "--max-string-units", "10",   "in.json"});
    const auto& full = full_line.result;
    ASSERT_TRUE(full.parsed.has_value());
    EXPECT_EQ(full.parsed->selected, command::validate);
    EXPECT_EQ(full.parsed->syntax,
              std::optional<adaptive_json5::syntax_mode>{adaptive_json5::syntax_mode::strict_json});
    EXPECT_EQ(full.parsed->duplicates, adaptive_json5::duplicate_name_policy::reject_duplicates);
    EXPECT_EQ(full.parsed->schema, "s.json");
    EXPECT_EQ(full.parsed->diagnostics, adaptive_json5::cli::diagnostics_format::json);
    EXPECT_EQ(full.parsed->limits.max_input_bytes, 100U);
    EXPECT_EQ(full.parsed->limits.max_depth, 7U);
    EXPECT_EQ(full.parsed->limits.max_nodes, 8U);
    EXPECT_EQ(full.parsed->limits.max_members, 9U);
    EXPECT_EQ(full.parsed->limits.max_string_units, 10U);
    EXPECT_EQ(full.parsed->input, "in.json");

    const command_line stdin_line({"inspect", "--syntax", "json5", "-"});
    const auto& stdin_input = stdin_line.result;
    ASSERT_TRUE(stdin_input.parsed.has_value());
    EXPECT_EQ(stdin_input.parsed->input, "-");
    EXPECT_EQ(stdin_input.parsed->syntax,
              std::optional<adaptive_json5::syntax_mode>{adaptive_json5::syntax_mode::json5});

    const command_line run_line({"run", "--manifest", "m.json"});
    const auto& run = run_line.result;
    ASSERT_TRUE(run.parsed.has_value());
    EXPECT_EQ(run.parsed->manifest, "m.json");
    for (const char* name : {"help", "--help", "-h", "version", "--version", "capabilities"})
    {
      EXPECT_TRUE(command_line({name}).result.parsed.has_value()) << name;
    }
  }

  /**
   * @brief One rejected command line and its expected error.
   * @par Requirements
   * P056; J5-070.
   */
  struct rejected_case
  {
      std::vector<std::string> arguments;
      argument_error_code code;
      std::string argument;
  };

  /*
   * Arrange: Command lines with each kind of mistake.
   * Act: Parse them.
   * Expect: The specific error code and the offending argument; never options (J5-070).
   */
  TEST(P056_Arguments, RejectsEveryMistake)
  {
    const std::vector<rejected_case> cases{
      {{}, argument_error_code::missing_command, ""},
      {{"lint"}, argument_error_code::unknown_command, "lint"},
      {{"validate", "--color", "x"}, argument_error_code::unknown_option, "--color"},
      {{"validate", "--syntax"}, argument_error_code::missing_value, "--syntax"},
      {{"validate", "--syntax", "yaml", "a"}, argument_error_code::invalid_value, "yaml"},
      {{"validate", "--duplicates", "drop", "a"}, argument_error_code::invalid_value, "drop"},
      {{"validate", "--diagnostics", "xml", "a"}, argument_error_code::invalid_value, "xml"},
      {{"validate", "--max-depth", "0", "a"}, argument_error_code::invalid_value, "0"},
      {{"validate", "--max-depth", "-3", "a"}, argument_error_code::invalid_value, "-3"},
      {{"validate", "--max-depth", "1234567890123456789", "a"},
       argument_error_code::invalid_value,
       "1234567890123456789"},
      {{"validate", "--schema", "", "a"}, argument_error_code::invalid_value, ""},
      {{"validate", "--syntax", "json", "--syntax", "json5", "a"}, argument_error_code::repeated_option, "--syntax"},
      {{"inspect", "--schema", "s.json", "a"}, argument_error_code::option_not_allowed, "--schema"},
      {{"run", "--syntax", "json"}, argument_error_code::option_not_allowed, "--syntax"},
      {{"validate", "--syntax", "json"}, argument_error_code::missing_input, ""},
      {{"validate", "a"}, argument_error_code::missing_syntax, "--syntax"},
      {{"validate", "--syntax", "json", "a", "b"}, argument_error_code::extra_argument, "b"},
      {{"version", "x"}, argument_error_code::extra_argument, "x"},
      {{"run"}, argument_error_code::missing_value, "--manifest"},
    };
    for (const rejected_case& item : cases)
    {
      const command_line line(item.arguments);
      const auto& parsed = line.result;
      EXPECT_FALSE(parsed.parsed.has_value()) << ::testing::PrintToString(item.arguments);
      EXPECT_EQ(parsed.failure.code, item.code) << ::testing::PrintToString(item.arguments);
      EXPECT_EQ(parsed.failure.argument, item.argument) << ::testing::PrintToString(item.arguments);
      EXPECT_FALSE(adaptive_json5::cli::describe(item.code).empty());
    }
  }

  /* ================================== Test_P056_Dispatch ================================== */
  /*
   * Arrange: A fake host with valid, malformed and duplicate-name inputs.
   * Act: Run validate in both syntax modes and duplicate policies, from a file and stdin.
   * Expect: exit 0 for valid input, 1 for invalid input with a located message, through the
   *         injected reader and sink only (J5-070).
   */
  TEST(P056_Dispatch, ValidateMapsParseOutcomes)
  {
    fake_host host;
    host.files["ok.json5"] = "{a: 1, // comment\n b: [2, 3]}";
    host.files["ok.json"] = R"({"a": 1})";
    host.files["bad.json"] = "{a: 1}";
    host.files["dup.json5"] = "{a: 1, a: 2}";
    host.files["-"] = "[1, 2]";
    EXPECT_EQ(host.run({"validate", "--syntax", "json5", "ok.json5"}), 0);
    EXPECT_EQ(host.out, "ok.json5: valid\n");
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "ok.json"}), 0);
    EXPECT_EQ(host.run({"validate", "--syntax", "json5", "-"}), 0);
    EXPECT_EQ(host.run({"validate", "--syntax", "json5", "bad.json"}), 0);
    host.out.clear();
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "bad.json"}), 1);
    EXPECT_NE(host.out.find("bad.json: "), std::string::npos);
    EXPECT_NE(host.out.find("line: 1, column: 2: not allowed in strict JSON"), std::string::npos) << host.out;
    EXPECT_EQ(host.run({"validate", "--syntax", "json5", "dup.json5"}), 0);
    EXPECT_EQ(host.run({"validate", "--syntax", "json5", "--duplicates", "reject", "dup.json5"}), 1);
  }

  /*
   * Arrange: Missing files, read errors, inputs above --max-input-bytes and parser limits.
   * Act: Run validate.
   * Expect: exit 2 with an error message on stderr for I/O and limit failures (J5-047, J5-070).
   */
  TEST(P056_Dispatch, IoAndBudgetFailuresExitTwo)
  {
    fake_host host;
    host.files["big.json"] = "[1, 2, 3, 4, 5, 6, 7, 8, 9]";
    host.files["deep.json"] = "[[[[1]]]]";
    host.read_status["broken.json"] = io_status::read_error;
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "missing.json"}), 2);
    EXPECT_NE(host.err.find("cannot open input: missing.json"), std::string::npos) << host.err;
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "broken.json"}), 2);
    EXPECT_NE(host.err.find("cannot read input"), std::string::npos);
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "--max-input-bytes", "8", "big.json"}), 2);
    EXPECT_NE(host.err.find("exceeds --max-input-bytes"), std::string::npos);
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "--max-depth", "2", "deep.json"}), 2);
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "--max-depth", "4", "deep.json"}), 0);
  }

  /*
   * Arrange: Argument mistakes, help, commands not yet available and a failing output sink.
   * Act: Run the tool.
   * Expect: Usage errors exit 2 with the usage text on stderr; help exits 0 on stdout; a failed
   *         write is an operational failure (J5-070, J5-071).
   */
  TEST(P056_Dispatch, UsageHelpAndOutputFailures)
  {
    fake_host host;
    EXPECT_EQ(host.run({}), 2);
    EXPECT_NE(host.err.find("error: missing command"), std::string::npos);
    EXPECT_NE(host.err.find("usage:"), std::string::npos);
    EXPECT_EQ(host.run({"validate", "--bogus", "1"}), 2);
    EXPECT_NE(host.err.find("unknown option: --bogus"), std::string::npos);
    EXPECT_EQ(host.run({"help"}), 0);
    EXPECT_NE(host.out.find("usage:"), std::string::npos);
    host.files["ok.json"] = "1";
    host.fail_writes = true;
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "ok.json"}), 2);
    EXPECT_EQ(host.run({"help"}), 2);
  }

  /*
   * Arrange: --schema with a trivial schema.
   * Act: Run validate.
   * Expect: Without the schema component: exit 2 and an explicit "not available" message,
   *         never a silent pass; with it: the schema is applied (J5-070).
   */
  TEST(P056_Dispatch, SchemaFlagReportsUnavailable)
  {
    fake_host host;
    host.files["ok.json"] = "1";
    host.files["s.json"] = "{}";
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "--schema", "s.json", "ok.json"}), 0);
#else
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "--schema", "s.json", "ok.json"}), 2);
    EXPECT_NE(host.err.find("not available"), std::string::npos) << host.err;
#endif
  }

  /*
   * Arrange: Allocation n failing for each n.
   * Act: Run validate on a valid input.
   * Expect: exit 2 (out of memory) or 0, never a crash, with everything released (J5-046,
   *         J5-071).
   */
  TEST(P056_Dispatch, AllocationFailureSweep)
  {
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      fake_host host;
      host.files["ok.json"] = R"({"a": [1, "x", {"b": null}]})";
      host.fail_on = fail_on;
      const int exit = host.run({"validate", "--syntax", "json", "ok.json"});
      if (exit == 0)
      {
        break;
      }
      ASSERT_EQ(exit, 2) << fail_on;
      ASSERT_LT(fail_on, 100U);
    }
  }
} // namespace
