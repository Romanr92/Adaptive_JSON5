#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_test_storage.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief Result of compiling with format options and validating one instance.
   * @par Requirements
   * J5-069.
   */
  struct format_outcome
  {
      std::optional<schema_error_code> error{};
      std::optional<invalid_schema_reason> reason{};
      std::optional<schema_limit_reason> limit{};
      bool valid{};
  };

  /**
   * @brief Compile schema_text with the given format options and validate instance_text.
   * @par Requirements
   * J5-069; J5-065.
   */
  format_outcome run_format(const std::string& schema_text, const std::string& instance_text, bool assert_format,
                            bool assert_content = false, const validation_budget& budget = {},
                            std::size_t work_fail_on = 0U)
  {
    format_outcome outcome{};
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
    auto registry = schema_registry::create(storage, 1U);
    EXPECT_TRUE(registry.value_if()->add({schema_text.data(), schema_text.size()}).has_value()) << schema_text;
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    options.assert_format = assert_format;
    options.assert_content = assert_content;
    auto compiled = compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    if (!compiled.has_value())
    {
      outcome.error = compiled.error_if()->code();
      outcome.reason = compiled.error_if()->invalid_schema_reason();
      return outcome;
    }
    schema_test_storage instance_provider;
    accounted_storage accounting{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto instance = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, accounting);
    EXPECT_TRUE(instance.has_value()) << instance_text;
    schema_test_storage work;
    work.fail_on = work_fail_on;
    schema_test_storage report;
    validation_options run{};
    run.work_storage = work.callbacks();
    run.report_storage = report.callbacks();
    run.budget = budget;
    const auto result = validate(*compiled.value_if(), instance.value_if()->values, run);
    EXPECT_EQ(work.live_bytes, 0U);
    if (!result.has_value())
    {
      outcome.error = result.error_if()->code();
      outcome.limit = result.error_if()->limit_reason();
      return outcome;
    }
    outcome.valid = result.value_if()->valid();
    return outcome;
  }

  /**
   * @brief One format case: the format name, a JSON string instance and its validity.
   * @par Requirements
   * J5-069.
   */
  struct format_case
  {
      const char* format;
      const char* instance;
      bool valid;
  };

  /* ================================ Test_J5_069_FormatAssertion ================================ */
  class J5_069_FormatAssertion : public testing::TestWithParam<format_case>
  {
  };

  /*
   * Arrange: Valid and invalid values of every supported format.
   * Act: Validate with assert_format and without it.
   * Expect: The format verdict with assert_format; always valid without it (annotation only)
   *         (J5-069).
   */
  TEST_P(J5_069_FormatAssertion, ChecksTheSupportedSet)
  {
    const format_case& item = GetParam();
    const std::string schema = std::string{R"({"format": ")"} + item.format + R"("})";
    const format_outcome asserted = run_format(schema, item.instance, true);
    ASSERT_FALSE(asserted.error.has_value()) << item.format << " " << item.instance;
    EXPECT_EQ(asserted.valid, item.valid) << item.format << " " << item.instance;
    const format_outcome annotation = run_format(schema, item.instance, false);
    ASSERT_FALSE(annotation.error.has_value());
    EXPECT_TRUE(annotation.valid) << item.format << " " << item.instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Formats, J5_069_FormatAssertion,
    testing::Values(
      format_case{"date-time", R"("1963-06-19T08:30:06.283185Z")", true},
      format_case{"date-time", R"("1963-06-19t08:30:06z")", true},
      format_case{"date-time", R"("1990-12-31T15:59:50.123-08:00")", true},
      format_case{"date-time", R"("1990-02-31T15:59:60.123-08:00")", false},
      format_case{"date-time", R"("1990-12-31T15:59:60-24:00")", false},
      format_case{"date-time", R"("06/19/1963 08:30:06 PST")", false},
      format_case{"date-time", R"("2013-350T01:01:01")", false}, format_case{"date", R"("1963-06-19")", true},
      format_case{"date", R"("2020-02-29")", true}, format_case{"date", R"("2000-02-29")", true},
      format_case{"date", R"("1900-02-29")", false}, format_case{"date", R"("2021-02-29")", false},
      format_case{"date", R"("2020-04-31")", false}, format_case{"date", R"("2020-13-01")", false},
      format_case{"date", R"("2020-00-10")", false}, format_case{"date", R"("06/19/1963")", false},
      format_case{"date", R"("1963-6-19")", false}, format_case{"time", R"("08:30:06.283185Z")", true},
      format_case{"time", R"("23:59:60Z")", true}, format_case{"time", R"("00:59:60+01:00")", true},
      format_case{"time", R"("23:59:60+01:00")", false}, format_case{"time", R"("08:30:06")", false},
      format_case{"time", R"("24:00:00Z")", false}, format_case{"time", R"("08:60:00Z")", false},
      format_case{"time", R"("08:30:06.Z")", false}, format_case{"time", R"("08:30:06+0100")", false},
      format_case{"email", R"("joe.bloggs@example.com")", true}, format_case{"email", R"("te~st@example.com")", true},
      format_case{"email", R"("2962")", false}, format_case{"email", R"("\"joe bloggs\"@example.com")", true},
      format_case{"email", R"("\"a\\\"b\"@example.com")", true},
      format_case{"email", R"("\"unterminated@example.com")", false},
      format_case{"email", R"("joe@[127.0.0.1]")", true}, format_case{"email", R"("joe@[IPv6:::1]")", true},
      format_case{"email", R"("joe@[IPv6:zz]")", false}, format_case{"email", R"("joe@[300.0.0.1]")", false},
      format_case{"email", R"(".test@example.com")", false}, format_case{"email", R"("test.@example.com")", false},
      format_case{"email", R"("te..st@example.com")", false}, format_case{"email", R"("joe@-example.com")", false},
      format_case{"email", R"("jöe@example.com")", false}, format_case{"hostname", R"("www.example.com")", true},
      format_case{"hostname", R"("xn--4gbwdl.xn--wgbh1c")", true},
      format_case{"hostname", R"("www.xn--mnchen-3ya.de")", true},
      format_case{"hostname", R"("XN--mnchen-3ya.de")", true}, format_case{"hostname", R"("xn--abc-.example")", false},
      format_case{"hostname", R"("xn--hello-zed.example")", false}, format_case{"hostname", R"("xn--l-fda")", false},
      format_case{"email", R"("joe@xn--mnchen-3ya.de")", true}, format_case{"email", R"("joe@xn--abc-.de")", false},
      format_case{"hostname", R"("-a-host-name-that-starts-with--")", false},
      format_case{"hostname", R"("not_a_valid_host_name")", false}, format_case{"hostname", R"("a..b")", false},
      format_case{"hostname", R"("")", false},
      format_case{"hostname",
                  R"("a-vvvvvvvvvvvvvvvveeeeeeeeeeeeeeeerrrrrrrrrrrrrrrryyyyyyyyyyyyyyyy-long-host-name-component")",
                  false},
      format_case{"hostname", R"("höst")", false}, format_case{"ipv4", R"("192.168.0.1")", true},
      format_case{"ipv4", R"("0.0.0.0")", true}, format_case{"ipv4", R"("127.0.0.0.1")", false},
      format_case{"ipv4", R"("256.256.256.256")", false}, format_case{"ipv4", R"("127.0")", false},
      format_case{"ipv4", R"("087.10.0.1")", false}, format_case{"ipv4", R"("1.2.3.")", false},
      format_case{"ipv6", R"("::1")", true}, format_case{"ipv6", R"("::")", true},
      format_case{"ipv6", R"("1:2:3:4:5:6:7:8")", true}, format_case{"ipv6", R"("::ffff:192.168.0.1")", true},
      format_case{"ipv6", R"("fe80::a:b")", true}, format_case{"ipv6", R"("12345::")", false},
      format_case{"ipv6", R"("1:1:1:1:1:1:1:1:1")", false}, format_case{"ipv6", R"("1::2::3")", false},
      format_case{"ipv6", R"(":::1")", false}, format_case{"ipv6", R"("1:2:3:4:5:6:7")", false},
      format_case{"ipv6", R"("1:2:3:4:5:6:7:8::")", false}, format_case{"ipv6", R"("::laptop")", false},
      format_case{"uri", R"("http://foo.bar/?baz=qux#quux")", true},
      format_case{"uri", R"("urn:oasis:names:specification:docbook:dtd:xml:4.1.2")", true},
      format_case{"uri", R"("http://example.com/%7Euser")", true},
      format_case{"uri", R"("//foo.bar/?baz=qux#quux")", false}, format_case{"uri", R"("/abc")", false},
      format_case{"uri", R"("\\\\WINDOWS\\fileshare")", false},
      format_case{"uri", R"("http:// shouldfail.com")", false},
      format_case{"uri", R"("http://example.com/%zz")", false}, format_case{"uri", R"("1http://example.com")", false},
      format_case{"uri", R"("a#b#c")", false}, format_case{"uri", R"("http://user:pw@[2001:db8::7]:8080/p?q#f")", true},
      format_case{"uri", R"("http://[v1.fe80::a]/")", true}, format_case{"uri", R"("http://example.com:80a/")", false},
      format_case{"uri", R"("http://a@b@example.com/")", false}, format_case{"uri", R"("http://[::1/")", false},
      format_case{"uri", R"("http://[::1]x/")", false}, format_case{"uri", R"("http://[@example.com/")", false},
      format_case{"uri", R"("http:/[::1]")", false}, format_case{"uri", R"("file:///etc/hosts")", true},
      format_case{"uri", R"("http://host:/p")", true}, format_case{"uri", R"("http://[V7.abc]/")", true},
      format_case{"uri", R"("http://hst/")", false}, format_case{"uri-reference", R"("")", true},
      format_case{"uri-reference", R"("?q=[1]")", false}, format_case{"hostname", R"("a	b")", false},
      format_case{"email", R"("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@example.com")", false},
      format_case{"email", R"("\"a\\\"@example.com")", false}, format_case{"email", R"("\"a\"@example.com")", false},
      format_case{"email", R"("a@[ipv6:::1]")", true}, format_case{"email", R"("a@[IPv4:1.2.3.4]")", false},
      format_case{"email", R"("a@")", false}, format_case{"time", R"("12:00:00.5Z")", true},
      format_case{"time", R"("12:00:00+05:30")", true}, format_case{"time", R"("12:00:00+24:00")", false},
      format_case{"time", R"("1a:00:00Z")", false}, format_case{"date", R"("2024-1a-01")", false},
      format_case{"date-time", R"("2024-01-01X12:00:00Z")", false}, format_case{"json-pointer", R"("/ab")", true},
      format_case{"relative-json-pointer", R"("0#")", true}, format_case{"relative-json-pointer", R"("")", false},
      format_case{"ipv6", R"("1:2:3:4:5:6:1.2.3.4")", true}, format_case{"ipv6", R"("1:2:3:4:5:6:7:1.2.3.4")", false},
      format_case{"ipv4", R"("1.2.3.4.")", false}, format_case{"uri-reference", R"("/abc")", true},
      format_case{"uri-reference", R"("#fragment")", true},
      format_case{"uri-reference", R"("http://foo.bar/?baz=qux#quux")", true},
      format_case{"uri-reference", R"("\\\\WINDOWS\\filePath")", false},
      format_case{"uri-reference", R"("#frag^ment")", false},
      format_case{"json-pointer", R"("/foo/bar~0/baz~1/%a")", true}, format_case{"json-pointer", R"("")", true},
      format_case{"json-pointer", R"("/é/x")", true}, format_case{"json-pointer", R"("/foo/bar~")", false},
      format_case{"json-pointer", R"("/~2")", false}, format_case{"json-pointer", R"("#/foo")", false},
      format_case{"relative-json-pointer", R"("1")", true},
      format_case{"relative-json-pointer", R"("0/foo/bar")", true},
      format_case{"relative-json-pointer", R"("2#")", true},
      format_case{"relative-json-pointer", R"("/foo/bar")", false},
      format_case{"relative-json-pointer", R"("-1/foo")", false},
      format_case{"relative-json-pointer", R"("01")", false}, format_case{"relative-json-pointer", R"("0##")", false},
      format_case{"regex", R"("([abc])+\\s+$")", true}, format_case{"regex", R"x("(?=lookahead)")x", true},
      format_case{"regex", R"x("^(abc]")x", false}, format_case{"regex", R"("[")", false},
      format_case{"regex", R"x("(?<=a)b(?<n>c)(?!d)")x", true}, format_case{"regex", R"x("(?i)abc")x", false},
      format_case{"regex", R"x("(?P<n>a)")x", false}, format_case{"regex", R"x("(?#comment)a")x", false},
      format_case{"regex", R"x("(?<name>a)")x", true}, format_case{"regex", R"x("(?<!a)b")x", true},
      format_case{"regex", R"x("(?<1a>b)")x", false}, format_case{"regex", R"x("(?<")x", false},
      format_case{"regex", R"x("(?")x", false}),
    [](const testing::TestParamInfo<format_case>& param_info)
    {
      std::string name = std::to_string(param_info.index) + "_" + param_info.param.format;
      for (char& c : name)
      {
        c = (c == '-') ? '_' : c;
      }
      return name;
    });

  /* =============================== Test_J5_069_FormatOptions =============================== */
  /*
   * Arrange: Unknown formats, non-string instances, a non-string format value and the content
   *          assertion request.
   * Act: Compile and validate.
   * Expect: Unknown formats and non-strings pass; a non-string format is keyword_shape;
   *         assert_content is unsupported_option; the capability query lists exactly the supported
   *         set (J5-054, J5-069).
   */
  TEST(J5_069_FormatOptions, UnsupportedAndNonApplicable)
  {
    EXPECT_TRUE(run_format(R"({"format": "color"})", R"("not a color")", true).valid);
    EXPECT_TRUE(run_format(R"({"format": "idn-email"})", R"("x")", true).valid);
    EXPECT_TRUE(run_format(R"({"format": "ipv4"})", "12", true).valid);
    EXPECT_TRUE(run_format(R"({"format": "date"})", R"({"a": 1})", true).valid);
    const format_outcome shape = run_format(R"({"format": 4})", "1", true);
    EXPECT_EQ(shape.error, std::optional<schema_error_code>{schema_error_code::invalid_schema});
    EXPECT_EQ(shape.reason, std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape});
    EXPECT_EQ(run_format(R"({"contentEncoding": "base64"})", R"("x")", false, true).error,
              std::optional<schema_error_code>{schema_error_code::unsupported_option});
    EXPECT_TRUE(run_format(R"({"contentEncoding": "base64"})", R"("not base64!")", true).valid);

    for (const char* name : {"date-time", "date", "time", "email", "hostname", "ipv4", "ipv6", "uri", "uri-reference",
                             "json-pointer", "relative-json-pointer", "regex"})
    {
      EXPECT_TRUE(format_assertion_supported(name)) << name;
    }
    for (const char* name :
         {"", "color", "idn-email", "idn-hostname", "iri", "iri-reference", "uri-template", "Date", "date-times"})
    {
      EXPECT_FALSE(format_assertion_supported(name)) << name;
    }
  }

  /*
   * Arrange: Format checks with a regex budget too small for the text, and with work storage
   *          failing at each allocation.
   * Act: Validate.
   * Expect: resource_limit (max_regex_steps) or allocation_failure with storage released, then
   *         the verdict (J5-046, J5-065).
   */
  TEST(J5_069_FormatOptions, BudgetAndStorageFailures)
  {
    validation_budget small{};
    small.max_regex_steps = 4U;
    const format_outcome limited = run_format(R"({"format": "hostname"})", R"("www.example.com")", true, false, small);
    EXPECT_EQ(limited.error, std::optional<schema_error_code>{schema_error_code::resource_limit});
    EXPECT_EQ(limited.limit, std::optional<schema_limit_reason>{schema_limit_reason::max_regex_steps});
    EXPECT_EQ(run_format(R"({"format": "regex"})", R"("(a|b)*c")", true, false, small).error,
              std::optional<schema_error_code>{schema_error_code::resource_limit});

    for (const char* format : {"email", "regex", "json-pointer"})
    {
      const std::string schema = std::string{R"({"format": ")"} + format + R"("})";
      for (std::size_t fail_on = 1U;; ++fail_on)
      {
        const format_outcome outcome =
          run_format(schema, R"("a.b@example.com")", true, false, validation_budget{}, fail_on);
        if (!outcome.error.has_value())
        {
          break;
        }
        ASSERT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::allocation_failure})
          << format << " " << fail_on;
        ASSERT_LT(fail_on, 100U);
      }
    }
  }
} // namespace
