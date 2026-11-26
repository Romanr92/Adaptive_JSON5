#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "cli/cli_app.hpp"
#include "cli/report.hpp"
#include "cli_fakes.hpp"

#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::fake_host;

  /**
   * @brief Strict-JSON parse of tool output, kept alive for inspection.
   * @par Requirements
   * P057; J5-071.
   */
  class json_output
  {
    private:
      static result<void*> allocate(void*, std::size_t bytes, std::size_t) noexcept
      {
        void* address = std::malloc(bytes);
        return (address == nullptr) ? result<void*>::failure(error::without_location(error_code::allocation_failure))
                                    : result<void*>::success(address);
      }

      static void release(void*, void* address, std::size_t, std::size_t) noexcept
      {
        std::free(address);
      }

      std::string m_text;
      parse_options m_options{};
      accounted_storage m_accounting;
      std::optional<parsed_document> m_parsed;

      static parse_options strict() noexcept
      {
        parse_options options{};
        options.syntax = syntax_mode::strict_json;
        options.duplicate_names = duplicate_name_policy::reject_duplicates;
        options.limits.max_input_bytes = 1U << 20U;
        options.limits.max_nodes = 1U << 16U;
        options.limits.max_members = 1U << 16U;
        options.limits.max_total_string_units = 1U << 20U;
        options.limits.max_owned_bytes = 1U << 24U;
        return options;
      }

    public:
      explicit json_output(std::string text)
          : m_text(std::move(text)), m_options(strict()),
            m_accounting(storage_callbacks{nullptr, &allocate, &release}, m_options.limits.max_owned_bytes)
      {
        auto parsed = parse_document({m_text.data(), m_text.size()}, m_options, m_accounting);
        if (parsed.has_value())
        {
          m_parsed.emplace(std::move(*parsed.value_if()));
        }
      }

      [[nodiscard]] bool valid() const
      {
        return m_parsed.has_value();
      }

      [[nodiscard]] value_view root() const
      {
        return *m_parsed->values.root().value_if();
      }

      /**
       * @brief Member value as std::string (ASCII strings) or as decimal text (numbers).
       */
      static std::string text_of(const value_view& value)
      {
        const auto text = value.as_string();
        if (text.has_value())
        {
          std::string out;
          for (std::size_t index = 0U; index < (*text.value_if())->size(); ++index)
          {
            out.push_back(static_cast<char>(*(*text.value_if())->unit_at(index).value_if()));
          }
          return out;
        }
        const auto number = value.as_number();
        if (number.has_value())
        {
          const input_view spelling = (*number.value_if())->spelling();
          return std::string{spelling.data, spelling.size};
        }
        return "<not text>";
      }

      [[nodiscard]] std::string field(std::string_view name) const
      {
        const auto member = root().find_member(name);
        return member.has_value() ? text_of(*member.value_if()) : std::string{"<missing>"};
      }

      [[nodiscard]] std::size_t diagnostic_count() const
      {
        return *root().find_member("diagnostics").value_if()->container_size().value_if();
      }

      [[nodiscard]] std::string diagnostic_field(std::size_t index, std::string_view name) const
      {
        const value_view item = *root().find_member("diagnostics").value_if()->element_at(index).value_if();
        const auto member = item.find_member(name);
        return member.has_value() ? text_of(*member.value_if()) : std::string{"<missing>"};
      }
  };

  /**
   * @brief One validate scenario: files, arguments and the expected exit, result and first
   * diagnostic category.
   * @par Requirements
   * P057; J5-070; J5-071.
   */
  struct outcome_case
  {
      const char* name;
      std::vector<std::string> arguments;
      int exit;
      const char* result;
      const char* category; // First diagnostic category, or "" for none.
  };

  /**
   * @brief Fixture files used by the outcome matrix.
   * @par Requirements
   * P057; J5-070.
   */
  void add_files(fake_host& host)
  {
    host.files["ok.json"] = R"({"a": 1, "b": [true, null]})";
    host.files["bad.json"] = R"({"a": 1,})";
    host.files["dup.json5"] = "{a: 1, a: 2}";
    host.files["deep.json"] = "[[[[[1]]]]]";
    host.files["bytes.json"] = std::string{"\"\xC3\x28\""};
    host.files["inf.json5"] = "{a: Infinity}";
    host.files["s-min.json"] = R"({"properties": {"a": {"minimum": 5}}})";
    host.files["s-bad.json"] = R"({"minLength": -1})";
    host.files["s-ref.json"] = R"({"$ref": "#/definitions/none"})";
    host.files["s-dialect.json"] = R"({"$schema": "http://json-schema.org/draft-04/schema#"})";
    host.files["s-syntax.json"] = "{";
  }

  /* ================================= Test_P057_Outcomes ================================= */
  class P057_Outcomes : public testing::TestWithParam<outcome_case>
  {
  };

  /*
   * Arrange: Valid, malformed, duplicate, limit, encoding, I/O, invalid-schema, unresolved
   *          reference, dialect, assertion and outside-domain inputs.
   * Act: Run validate with --diagnostics json.
   * Expect: The exit code of J5-070, a strict-JSON report with format, version, result, exit
   *         and the expected first diagnostic category (J5-070, J5-071).
   */
  TEST_P(P057_Outcomes, MapResultsAndExitCodes)
  {
    const outcome_case& item = GetParam();
    fake_host host;
    add_files(host);
    std::vector<std::string> arguments{"validate", "--diagnostics", "json"};
    arguments.insert(arguments.end(), item.arguments.begin(), item.arguments.end());
    EXPECT_EQ(host.run(arguments), item.exit) << host.out << host.err;
    const json_output report{host.out};
    ASSERT_TRUE(report.valid()) << host.out;
    EXPECT_EQ(report.field("format"), "adaptive-json5-diagnostics");
    EXPECT_EQ(report.field("version"), "1");
    EXPECT_EQ(report.field("command"), "validate");
    EXPECT_EQ(report.field("result"), item.result);
    EXPECT_EQ(report.field("exit"), std::to_string(item.exit));
    if (std::string{item.category}.empty())
    {
      EXPECT_EQ(report.diagnostic_count(), 0U) << host.out;
    }
    else
    {
      ASSERT_GE(report.diagnostic_count(), 1U) << host.out;
      EXPECT_EQ(report.diagnostic_field(0U, "category"), item.category) << host.out;
      EXPECT_EQ(report.diagnostic_field(0U, "severity"), "error");
    }
  }

  /**
   * @brief The outcome matrix; schema cases only when the schema component is built.
   * @par Requirements
   * P057; J5-070.
   */
  std::vector<outcome_case> outcome_cases()
  {
    std::vector<outcome_case> cases{
      outcome_case{"Valid", {"--syntax", "json", "ok.json"}, 0, "valid", ""},
      outcome_case{"Malformed", {"--syntax", "json", "bad.json"}, 1, "invalid", "syntax"},
      outcome_case{
        "DuplicateRejected", {"--syntax", "json5", "--duplicates", "reject", "dup.json5"}, 1, "invalid", "duplicate"},
      outcome_case{"Encoding", {"--syntax", "json", "bytes.json"}, 1, "invalid", "encoding"},
      outcome_case{"DepthLimit", {"--syntax", "json", "--max-depth", "3", "deep.json"}, 2, "error", "limit"},
      outcome_case{"MissingFile", {"--syntax", "json", "none.json"}, 2, "error", "io"},
      outcome_case{"InputTooLarge", {"--syntax", "json", "--max-input-bytes", "4", "ok.json"}, 2, "error", "io"}};
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    const std::vector<outcome_case> schema_cases{
      outcome_case{"SchemaValid", {"--syntax", "json", "--schema", "s-min.json", "ok.json"}, 1, "invalid", "assertion"},
      outcome_case{"InvalidSchema", {"--syntax", "json", "--schema", "s-bad.json", "ok.json"}, 2, "error", "schema"},
      outcome_case{"SchemaSyntax", {"--syntax", "json", "--schema", "s-syntax.json", "ok.json"}, 2, "error", "schema"},
      outcome_case{
        "UnresolvedReference", {"--syntax", "json", "--schema", "s-ref.json", "ok.json"}, 2, "error", "schema"},
      outcome_case{"OtherDialect", {"--syntax", "json", "--schema", "s-dialect.json", "ok.json"}, 2, "error", "schema"},
      outcome_case{"MissingSchemaFile", {"--syntax", "json", "--schema", "none.json", "ok.json"}, 2, "error", "io"},
      outcome_case{
        "OutsideDomain", {"--syntax", "json5", "--schema", "s-min.json", "inf.json5"}, 1, "invalid", "instance"}};
    cases.insert(cases.end(), schema_cases.begin(), schema_cases.end());
#endif
    return cases;
  }

  INSTANTIATE_TEST_SUITE_P(Cases, P057_Outcomes, testing::ValuesIn(outcome_cases()),
                           [](const testing::TestParamInfo<outcome_case>& param_info)
                           {
                             return std::string{param_info.param.name};
                           });

  /* ================================ Test_P057_Diagnostics ================================ */
  /*
   * Arrange: A malformed input and a schema assertion failure.
   * Act: Run validate with JSON diagnostics.
   * Expect: Line, column and byte offset of the error; keyword, instance pointer and schema
   *         pointer of the assertion (J5-064, J5-071).
   */
  TEST(P057_Diagnostics, LocateFailures)
  {
    fake_host host;
    host.files["bad.json"] = "{\n  \"a\": 1,\n}";
    EXPECT_EQ(host.run({"validate", "--diagnostics", "json", "--syntax", "json", "bad.json"}), 1);
    const json_output parse_report{host.out};
    ASSERT_TRUE(parse_report.valid()) << host.out;
    EXPECT_EQ(parse_report.diagnostic_field(0U, "line"), "3");
    EXPECT_EQ(parse_report.diagnostic_field(0U, "column"), "1");
    EXPECT_EQ(parse_report.diagnostic_field(0U, "byte_offset"), "12");
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    host.out.clear();
    host.files["s.json"] = R"({"properties": {"a/b": {"items": {"type": "string"}}}})";
    host.files["in.json"] = "{\"a/b\": [\"x\",\n 7]}";
    EXPECT_EQ(host.run({"validate", "--diagnostics", "json", "--syntax", "json", "--schema", "s.json", "in.json"}), 1);
    const json_output schema_report{host.out};
    ASSERT_TRUE(schema_report.valid()) << host.out;
    EXPECT_EQ(schema_report.diagnostic_field(0U, "keyword"), "type");
    EXPECT_EQ(schema_report.diagnostic_field(0U, "instance_pointer"), "/a~1b/1");
    EXPECT_EQ(schema_report.diagnostic_field(0U, "schema_pointer"), "#/properties/a~1b/items");
    EXPECT_EQ(schema_report.diagnostic_field(0U, "line"), "2");
    host.out.clear();
    EXPECT_EQ(host.run({"validate", "--syntax", "json", "--schema", "s.json", "in.json"}), 1);
    EXPECT_NE(host.out.find("(type at \"/a~1b/1\", schema #/properties/a~1b/items)"), std::string::npos) << host.out;
#endif
  }

  /*
   * Arrange: A JSON5 string containing a raw U+2028 (a string warning).
   * Act: Run validate in both formats.
   * Expect: Valid (exit 0) with one warning-severity diagnostic of category string (J5-045,
   *         J5-071).
   */
  TEST(P057_Diagnostics, WarningsHaveWarningSeverity)
  {
    fake_host host;
    host.files["sep.json5"] = std::string{"\"a\xE2\x80\xA8"} + "b\"";
    EXPECT_EQ(host.run({"validate", "--diagnostics", "json", "--syntax", "json5", "sep.json5"}), 0);
    const json_output report{host.out};
    ASSERT_TRUE(report.valid()) << host.out;
    EXPECT_EQ(report.field("result"), "valid");
    ASSERT_EQ(report.diagnostic_count(), 1U);
    EXPECT_EQ(report.diagnostic_field(0U, "severity"), "warning");
    EXPECT_EQ(report.diagnostic_field(0U, "category"), "string");
    host.out.clear();
    EXPECT_EQ(host.run({"validate", "--syntax", "json5", "sep.json5"}), 0);
    EXPECT_NE(host.out.find("sep.json5: valid"), std::string::npos);
  }

  /*
   * Arrange: Strings with quotes, backslashes, control characters, multi-byte UTF-8 and
   *          malformed, overlong and surrogate UTF-8.
   * Act: Write them with put_json_text and in a report's input field.
   * Expect: Escaped strict-JSON strings for well-formed UTF-8 and {"$bytes": [...]} otherwise;
   *         the result always parses as strict JSON (J5-071, ADR 0009 D2).
   */
  TEST(P057_Diagnostics, EscapesArbitraryText)
  {
    const auto written = [](const std::string& text)
    {
      fake_host host;
      adaptive_json5::cli::environment environment = host.environment();
      adaptive_json5::cli::text_writer out{environment.output, adaptive_json5::cli::output_stream::out};
      adaptive_json5::cli::put_json_text(out, text);
      return host.out;
    };
    EXPECT_EQ(written("a\"b\\c"), R"("a\"b\\c")");
    EXPECT_EQ(written(std::string{"\x01\n\x1f", 3}), R"("\u0001\u000a\u001f")");
    EXPECT_EQ(written("\xC3\xA9\xF0\x9F\x98\x80"), "\"\xC3\xA9\xF0\x9F\x98\x80\"");
    EXPECT_EQ(written("\xC3\x28"), R"({"$bytes": [195, 40]})");
    EXPECT_EQ(written("\xC0\xAF"), R"({"$bytes": [192, 175]})");
    EXPECT_EQ(written("\xE0\x80\xAF"), R"({"$bytes": [224, 128, 175]})");
    EXPECT_EQ(written("\xED\xA0\x80"), R"({"$bytes": [237, 160, 128]})");
    EXPECT_EQ(written("\xF4\x90\x80\x80"), R"({"$bytes": [244, 144, 128, 128]})");
    EXPECT_EQ(written("\xE2\x82"), R"({"$bytes": [226, 130]})");
    EXPECT_EQ(written(""), R"("")");

    fake_host host;
    const std::string path = std::string{"we\"ird\x01\xFF"} + ".json";
    host.files[path] = "1";
    EXPECT_EQ(host.run({"validate", "--diagnostics", "json", "--syntax", "json", path}), 0);
    const json_output report{host.out};
    EXPECT_TRUE(report.valid()) << host.out;
  }

  /*
   * Arrange: The capabilities command.
   * Act: Run it.
   * Expect: A strict-JSON object with format, version, commands, syntax modes, diagnostic
   *         formats and schema support matching the build (J5-069, J5-071).
   */
  TEST(P057_Diagnostics, CapabilitiesAreStrictJson)
  {
    fake_host host;
    EXPECT_EQ(host.run({"capabilities"}), 0);
    const json_output report{host.out};
    ASSERT_TRUE(report.valid()) << host.out;
    EXPECT_EQ(report.field("format"), "adaptive-json5-capabilities");
    EXPECT_EQ(report.field("version"), "1");
    const auto schema = report.root().find_member("schema");
    ASSERT_TRUE(schema.has_value());
    const auto available = schema.value_if()->find_member("available");
    ASSERT_TRUE(available.has_value());
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    EXPECT_TRUE(*available.value_if()->as_boolean().value_if());
    EXPECT_NE(host.out.find("\"regex\""), std::string::npos);
#else
    EXPECT_FALSE(*available.value_if()->as_boolean().value_if());
#endif
    host.fail_writes = true;
    EXPECT_EQ(host.run({"capabilities"}), 2);
  }

  /*
   * Arrange: Allocation n failing, for every n, in a validate run with a schema.
   * Act: Run validate with JSON diagnostics.
   * Expect: Each failure is exit 2 with a strict-JSON report whose last diagnostic is
   *         category allocation (or the parser's allocation failure), nothing leaks, and the
   *         final run succeeds (J5-046, J5-071 minimal OOM reporting).
   */
  TEST(P057_Diagnostics, OutOfMemoryFallbackIsStrictJson)
  {
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      fake_host host;
      add_files(host);
      host.fail_on = fail_on;
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
      const int exit =
        host.run({"validate", "--diagnostics", "json", "--syntax", "json", "--schema", "s-min.json", "ok.json"});
      const int final_exit = 1;
#else
      const int exit = host.run({"validate", "--diagnostics", "json", "--syntax", "json", "ok.json"});
      const int final_exit = 0;
#endif
      const json_output report{host.out};
      ASSERT_TRUE(report.valid()) << fail_on << ": " << host.out;
      if (exit == final_exit)
      {
        break;
      }
      ASSERT_EQ(exit, 2) << fail_on << ": " << host.out;
      EXPECT_EQ(report.field("result"), "error") << fail_on;
      ASSERT_GE(report.diagnostic_count(), 1U) << fail_on;
      EXPECT_EQ(report.diagnostic_field(report.diagnostic_count() - 1U, "category"), "allocation") << host.out;
      ASSERT_LT(fail_on, 2000U);
    }
  }
} // namespace
