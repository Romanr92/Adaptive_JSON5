#include "../schema/schema_test_storage.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::schema_test_storage;
  namespace fs = std::filesystem;

  constexpr std::string_view c_remote_base{"http://localhost:1234/"};
  constexpr std::string_view c_meta_schema_uri{"http://json-schema.org/draft-07/schema"};

  /**
   * @brief Read a whole file as bytes.
   * @par Requirements
   * P055; J5-096.
   */
  std::string read_file(const fs::path& path)
  {
    std::ifstream stream{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  }

  /**
   * @brief Parse options for whole fixture files, which exceed the default document limits
   * (ref.json is about 30 KB); each test schema is still registered with the default limits.
   * @par Requirements
   * P055; J5-096.
   */
  parse_options fixture_options()
  {
    parse_options options{};
    options.limits.max_input_bytes = 1U << 20U;
    options.limits.max_nodes = 1U << 20U;
    options.limits.max_members = 1U << 20U;
    options.limits.max_total_string_units = 1U << 22U;
    options.limits.max_owned_bytes = 1U << 26U;
    return options;
  }

  /**
   * @brief A parsed fixture file kept alive with its storage.
   * @par Requirements
   * P055; J5-096.
   */
  struct parsed_file
  {
      schema_test_storage provider;
      std::string text;
      accounted_storage accounting;
      std::optional<parsed_document> parsed;

      explicit parsed_file(std::string input)
          : text(std::move(input)), accounting(provider.callbacks(), fixture_options().limits.max_owned_bytes)
      {
        auto outcome = parse_document({text.data(), text.size()}, fixture_options(), accounting);
        if (outcome.has_value())
        {
          parsed.emplace(std::move(*outcome.value_if()));
        }
      }

      /**
       * @brief Return the source text of a value (its exact bytes in the file).
       * @par Requirements
       * P055.
       */
      std::string slice(const value_view& value) const
      {
        const auto span = value.source_span();
        if (!span.has_value() || !span.value_if()->has_value())
        {
          return {};
        }
        return text.substr((*span.value_if())->byte_offset, (*span.value_if())->byte_length);
      }
  };

  /**
   * @brief One test of the suite: a schema, an instance and the expected verdict.
   * @par Requirements
   * P055; J5-096.
   */
  struct suite_test
  {
      std::string name;     // GoogleTest-safe unique name.
      std::string location; // file#group/test for messages and the deviation table.
      std::string schema;   // Schema source text.
      std::string data;     // Instance source text.
      bool valid{};
      bool assert_format{};
      bool unsupported_pattern{};
  };

  /**
   * @brief Inventory of the suite as selected for this implementation's capabilities.
   * @par Requirements
   * P055; J5-096; J5-069.
   */
  struct suite_inventory
  {
      std::vector<suite_test> tests;
      std::size_t required{};
      std::size_t optional{};
      std::vector<std::string> skipped_files; // Optional files outside the advertised capabilities.
      std::vector<std::string> unreadable;    // Files that failed to parse (must stay empty).
  };

  /**
   * @brief Decide whether an optional file is run, and with format assertion.
   * @par Requirements
   * P055; J5-069; J5-096.
   * @return nullopt when the file needs a capability the implementation does not advertise.
   */
  std::optional<bool> optional_selection(const fs::path& relative)
  {
    const std::string generic = relative.generic_string();
    if (generic.rfind("optional/format/", 0U) == 0U)
    {
      // ecmascript-regex.json tests "format": "regex" with ECMA-262 specifics.
      const std::string stem = relative.stem().string();
      const std::string format = (stem == "ecmascript-regex") ? std::string{"regex"} : stem;
      if (format == "unknown")
      {
        return true;
      }
      return format_assertion_supported(format) ? std::optional<bool>{true} : std::nullopt;
    }
    if (generic == "optional/content.json")
    {
      return std::nullopt; // Content assertion is unsupported (compile_options::assert_content).
    }
    if (generic == "optional/cross-draft.json")
    {
      return std::nullopt; // Needs 2019-09 processing; only Draft-07 is supported (ADR 0006 decision 2).
    }
    return false;
  }

  /**
   * @brief Turn a location into a GoogleTest parameter name.
   * @par Requirements
   * P055.
   */
  std::string test_name(const std::string& location)
  {
    std::string name;
    for (const char c : location)
    {
      const bool keep = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'));
      name.push_back(keep ? c : '_');
    }
    return name;
  }

  /**
   * @brief Load every selected test of tests/draft7, required and optional.
   * @par Requirements
   * P055; J5-096.
   */
  const suite_inventory& inventory()
  {
    static const suite_inventory loaded = []
    {
      suite_inventory result;
      const fs::path root = fs::path{ADAPTIVE_JSON5_JSON_SCHEMA_TEST_SUITE_DIR} / "tests" / "draft7";
      std::vector<fs::path> files;
      for (const auto& entry : fs::recursive_directory_iterator{root})
      {
        if (entry.is_regular_file() && (entry.path().extension() == ".json"))
        {
          files.push_back(entry.path());
        }
      }
      std::sort(files.begin(), files.end());
      for (const fs::path& path : files)
      {
        const fs::path relative = fs::relative(path, root);
        const bool is_optional = relative.generic_string().rfind("optional/", 0U) == 0U;
        const std::optional<bool> selection = is_optional ? optional_selection(relative) : std::optional<bool>{false};
        if (!selection.has_value())
        {
          result.skipped_files.push_back(relative.generic_string());
          continue;
        }
        const parsed_file file{read_file(path)};
        if (!file.parsed.has_value())
        {
          result.unreadable.push_back(relative.generic_string());
          continue;
        }
        const value_view groups = *file.parsed->values.root().value_if();
        const std::size_t group_count = *groups.container_size().value_if();
        for (std::size_t group = 0U; group < group_count; ++group)
        {
          const value_view entry = *groups.element_at(group).value_if();
          const value_view schema = *entry.find_member("schema").value_if();
          const value_view tests = *entry.find_member("tests").value_if();
          const std::size_t test_count = *tests.container_size().value_if();
          // ADR 0006 advertises RFC 1123 hostname syntax. This group requires IDNA.
          if (relative == "optional/format/hostname.json" && group == 1U)
          {
            result.skipped_files.push_back(relative.generic_string() + "#1 (IDNA A-label validation)");
            continue;
          }
          for (std::size_t index = 0U; index < test_count; ++index)
          {
            const value_view test = *tests.element_at(index).value_if();
            suite_test item{};
            item.location = relative.generic_string() + "#" + std::to_string(group) + "/" + std::to_string(index);
            item.name = test_name(item.location);
            item.schema = file.slice(schema);
            item.data = file.slice(*test.find_member("data").value_if());
            item.valid = *test.find_member("valid").value_if()->as_boolean().value_if();
            item.assert_format = *selection;
            // ADR 0006 requires an explicit compile error for control and Unicode property escapes.
            item.unsupported_pattern = relative == "optional/ecmascript-regex.json" &&
                                       (group == 2U || group == 3U || group == 10U || group == 14U ||
                                        group == 15U || group == 19U);
            result.tests.push_back(item);
            (is_optional ? result.optional : result.required) += 1U;
          }
        }
      }
      return result;
    }();
    return loaded;
  }

  /**
   * @brief Known deviations: tests whose expected verdict this implementation does not give,
   * with the reason. Each entry must still fail; a passing entry is reported as obsolete.
   * @par Requirements
   * P055; J5-096 (no silently skipped required cases).
   */
  const std::set<std::string>& deviations()
  {
    // Location -> reason; keep in sync with tests/fixtures/third_party/README.md.
    static const std::set<std::string> entries{
      // "\a" is invalid only with the ECMA-262 u flag; the pattern engine follows Annex B (owner
      // decision, ADR 0006 #48 section), where an unknown identity escape matches the letter.
      "optional/format/ecmascript-regex.json#0/0",
    };
    return entries;
  }

  /**
   * @brief Remote schema files under remotes/, keyed by their localhost:1234 URI.
   * @par Requirements
   * P055; J5-055; J5-096.
   */
  const std::vector<std::pair<std::string, std::string>>& remotes()
  {
    static const std::vector<std::pair<std::string, std::string>> loaded = []
    {
      std::vector<std::pair<std::string, std::string>> result;
      const fs::path root = fs::path{ADAPTIVE_JSON5_JSON_SCHEMA_TEST_SUITE_DIR} / "remotes";
      for (const auto& entry : fs::recursive_directory_iterator{root})
      {
        const std::string relative = fs::relative(entry.path(), root).generic_string();
        // Remotes of other dialects carry their own $schema and belong to those dialects' tests.
        const bool other_dialect = (relative.rfind("draft", 0U) == 0U) && (relative.rfind("draft7/", 0U) != 0U);
        const bool v1 = relative.rfind("v1/", 0U) == 0U;
        if (entry.is_regular_file() && (entry.path().extension() == ".json") && !other_dialect && !v1)
        {
          result.emplace_back(std::string{c_remote_base} + relative, read_file(entry.path()));
        }
      }
      std::sort(result.begin(), result.end());
      return result;
    }();
    return loaded;
  }

  /**
   * @brief The Draft-07 meta-schema text, taken from the embedded copy in the library source.
   * @par Requirements
   * P055; J5-096.
   */
  const std::string& meta_schema_text()
  {
    static const std::string text = []
    {
      const std::string source = read_file(ADAPTIVE_JSON5_META_SCHEMA_SOURCE);
      const std::size_t begin = source.find("R\"meta(") + 7U;
      const std::size_t end = source.find(")meta\"", begin);
      return source.substr(begin, end - begin);
    }();
    return text;
  }

  /**
   * @brief Outcome of one suite test: a verdict or an error code.
   * @par Requirements
   * P055; J5-063.
   */
  struct suite_outcome
  {
      std::optional<schema_error_code> error{};
      std::optional<invalid_schema_reason> compile_reason{};
      bool valid{};
  };

  /**
   * @brief Register the schema (plus the remotes it may reference), compile and validate.
   * @par Requirements
   * P055; J5-055; J5-096.
   */
  suite_outcome run_test(const suite_test& item)
  {
    suite_outcome outcome{};
    const bool uses_remotes = item.schema.find("localhost:1234") != std::string::npos;
    const bool uses_meta = item.schema.find("json-schema.org/draft-07/schema") != std::string::npos;
    const std::size_t capacity = 2U + (uses_remotes ? remotes().size() : 0U);
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    accounted_storage storage{registry_provider.callbacks(), 1U << 24U};
    auto registry = schema_registry::create(storage, capacity);
    auto added = registry.value_if()->add({item.schema.data(), item.schema.size()});
    if (added.has_value() && uses_meta)
    {
      added = registry.value_if()->add(c_meta_schema_uri, {meta_schema_text().data(), meta_schema_text().size()});
    }
    for (const auto& remote : uses_remotes ? remotes() : std::vector<std::pair<std::string, std::string>>{})
    {
      if (added.has_value())
      {
        added = registry.value_if()->add(remote.first, {remote.second.data(), remote.second.size()});
      }
    }
    if (!added.has_value())
    {
      outcome.error = added.error_if()->code();
      return outcome;
    }
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    options.assert_format = item.assert_format;
    const auto compiled =
      compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    if (!compiled.has_value())
    {
      outcome.compile_reason = compiled.error_if()->invalid_schema_reason();
      outcome.error = compiled.error_if()->code();
      return outcome;
    }
    const parsed_file instance{item.data};
    if (!instance.parsed.has_value())
    {
      outcome.error = schema_error_code::unsupported_instance;
      return outcome;
    }
    schema_test_storage work;
    schema_test_storage report;
    validation_options run{};
    run.work_storage = work.callbacks();
    run.report_storage = report.callbacks();
    const auto verdict = validate(*compiled.value_if(), instance.parsed->values, run);
    if (!verdict.has_value())
    {
      outcome.error = verdict.error_if()->code();
      return outcome;
    }
    outcome.valid = verdict.value_if()->valid();
    return outcome;
  }

  /* ============================== Test_P055_Draft07Conformance ============================== */
  class P055_Draft07Conformance : public testing::TestWithParam<suite_test>
  {
  };

  /*
   * Arrange: One test of the pinned JSON-Schema-Test-Suite draft7 directory (required and the
   *          optional files within the advertised capabilities), with remotes registered offline.
   * Act: Compile the schema and validate the instance.
   * Expect: The suite's verdict, or failure for a documented deviation (J5-096, J5-053, J5-057).
   */
  TEST_P(P055_Draft07Conformance, MatchesTheSuite)
  {
    const suite_test& item = GetParam();
    const suite_outcome outcome = run_test(item);
    if (item.unsupported_pattern)
    {
      EXPECT_EQ(outcome.compile_reason, invalid_schema_reason::unsupported_pattern) << item.location;
      return;
    }
    const bool matches = !outcome.error.has_value() && (outcome.valid == item.valid);
    const bool deviation = deviations().count(item.location) != 0U;
    if (deviation)
    {
      EXPECT_FALSE(matches) << "obsolete deviation: " << item.location;
      return;
    }
    EXPECT_TRUE(matches) << item.location << "\nschema: " << item.schema << "\ndata: " << item.data
                         << "\nexpected valid=" << item.valid << " got "
                         << (outcome.error.has_value() ? "error " + std::to_string(static_cast<int>(*outcome.error))
                                                       : std::string{outcome.valid ? "valid" : "invalid"});
  }

  INSTANTIATE_TEST_SUITE_P(Suite, P055_Draft07Conformance, testing::ValuesIn(inventory().tests),
                           [](const testing::TestParamInfo<suite_test>& param_info)
                           {
                             return param_info.param.name;
                           });

  /*
   * Arrange: The selected inventory.
   * Act: Count required and optional tests and skipped optional files.
   * Expect: Every fixture file parses, required tests exist and none is skipped; only optional
   *         files outside the advertised capabilities are skipped (J5-096, J5-069).
   */
  TEST(P055_Draft07Inventory, NoRequiredCaseIsSkipped)
  {
    const suite_inventory& loaded = inventory();
    std::ostringstream unreadable;
    for (const std::string& file : loaded.unreadable)
    {
      unreadable << file << ' ';
    }
    EXPECT_TRUE(loaded.unreadable.empty()) << "fixture files that failed to parse: " << unreadable.str();
    EXPECT_GT(loaded.required, 0U);
    for (const std::string& file : loaded.skipped_files)
    {
      EXPECT_EQ(file.rfind("optional/", 0U), 0U) << file;
    }
    RecordProperty("required_tests", std::to_string(loaded.required));
    RecordProperty("optional_tests", std::to_string(loaded.optional));
    RecordProperty("deviations", std::to_string(deviations().size()));
    std::ostringstream skipped;
    for (const std::string& file : loaded.skipped_files)
    {
      skipped << file << ' ';
    }
    RecordProperty("skipped_optional_files", skipped.str());
    std::cout << "required " << loaded.required << ", optional " << loaded.optional << ", deviations "
              << deviations().size() << ", skipped optional files: " << skipped.str() << '\n';
  }
} // namespace
