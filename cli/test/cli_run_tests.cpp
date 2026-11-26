#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "cli/run_command.hpp"
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
   * @brief One manifest with a single case around the given expectation.
   * @par Requirements
   * P058; J5-070.
   */
  std::string one_case(const std::string& input, const std::string& syntax, const std::string& expect,
                       const std::string& extra = "")
  {
    return R"({"format_version": 2, "cases": [{"id": "c", "input": ")" + input + R"(", "syntax": ")" + syntax + R"(")" +
           extra + R"(, "expect": )" + expect + R"(}], "requirements": [{"requirement": "J5-X",
           "starter_inputs": [")" +
           input + R"("]}, {"requirement": "J5-Y", "starter_inputs": []}]})";
  }

  /**
   * @brief Fake host with the example inputs used by the run units.
   * @par Requirements
   * P058.
   */
  void add_inputs(fake_host& host)
  {
    host.files["dir/u.json5"] = R"({pair: "😀", raw: "😀", lone: "\uD800", nul: "a\0b"})";
    host.files["dir/n.json5"] = "[9007199254740993, 1.5, NaN, 0x10000000000000001, 1e400, -5]";
    host.files["dir/d.json5"] = "{name: 1, \"name\": 2, other: 3}";
    host.files["dir/bad.json"] = "{a: 1}";
    host.files["dir/s.json"] = R"({"properties": {"a": {"type": "integer"}}, "required": ["a"]})";
    host.files["dir/ok.json"] = R"({"a": 1})";
    host.files["dir/wrong.json"] = R"({"a": "x"})";
    host.files["dir/bad-schema.json"] = R"({"type": 5})";
    host.files["dir/enc.json"] = "\"\xFF\"";
  }

  /**
   * @brief One failing check of the given kind on an input (exit 1, label FAIL c).
   * @par Requirements
   * P058; J5-070.
   */
  std::string failing_check(const std::string& input, const std::string& check)
  {
    return one_case(input, "json5", R"({"parse": "success", "checks": [)" + check + "]}");
  }

  /**
   * @brief Run a one-case manifest and return the exit code.
   * @par Requirements
   * P058.
   */
  int run_manifest_text(fake_host& host, const std::string& manifest)
  {
    host.files["dir/m.json"] = manifest;
    host.out.clear();
    host.err.clear();
    return host.run({"run", "--manifest", "dir/m.json"});
  }

  /* ==================================== Test_P058_Run ==================================== */
  /**
   * @brief One run scenario: a manifest case and the expected exit and case label.
   * @par Requirements
   * P058; J5-070.
   */
  struct run_case
  {
      const char* name;
      std::string manifest;
      int exit;
      const char* label;
  };

  /**
   * @brief Scenarios: every check passing and failing, parse/exit expectations, intentionally
   * invalid inputs and missing inputs.
   * @par Requirements
   * P058; J5-070.
   */
  std::vector<run_case> run_cases()
  {
    return {
      {"InvalidInputExpected", one_case("bad.json", "json", R"({"parse": "error", "exit": 1})"), 0, "PASS c"},
      {"InvalidInputUnexpected", one_case("bad.json", "json", R"({"parse": "success", "exit": 0})"), 1, "FAIL c"},
      {"ExitMismatch", one_case("bad.json", "json5", R"({"parse": "success", "exit": 1})"), 1, "FAIL c"},
      {"SameString",
       one_case("u.json5", "json5",
                R"({"parse": "success", "checks": [{"check": "same_string", "pointer": "/pair", "other": "/raw"}]})"),
       0, "PASS c"},
      {"SameStringFails",
       one_case("u.json5", "json5",
                R"({"parse": "success", "checks": [{"check": "same_string", "pointer": "/pair", "other": "/lone"}]})"),
       1, "FAIL c"},
      {"Units",
       one_case(
         "u.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "string_units", "pointer": "/nul", "units": [97, 0, 98]}]})"),
       0, "PASS c"},
      {"UnitsFail",
       one_case("u.json5", "json5",
                R"({"parse": "success", "checks": [{"check": "string_units", "pointer": "/nul", "units": [97, 98]}]})"),
       1, "FAIL c"},
      {"Utf8Export",
       one_case(
         "u.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "utf8_export", "pointer": "/lone", "result": "failure"}, {"check": "utf8_export", "pointer": "/raw", "result": "success"}]})"),
       0, "PASS c"},
      {"Utf8ExportFails",
       one_case(
         "u.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "utf8_export", "pointer": "/lone", "result": "success"}]})"),
       1, "FAIL c"},
      {"Warnings",
       one_case("u.json5", "json5", R"({"parse": "success", "checks": [{"check": "warnings", "count": 1}]})"), 0,
       "PASS c"},
      {"WarningsFail",
       one_case("u.json5", "json5", R"({"parse": "success", "checks": [{"check": "warnings", "count": 0}]})"), 1,
       "FAIL c"},
      {"MemberNames",
       one_case(
         "d.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "member_names", "pointer": "", "names": ["name", "name", "other"]}]})"),
       0, "PASS c"},
      {"MemberNamesFail",
       one_case(
         "d.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "member_names", "pointer": "", "names": ["name", "other"]}]})"),
       1, "FAIL c"},
      {"Duplicates",
       one_case(
         "d.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "duplicate_rejection", "first": {"line": 1, "column": 2}, "repeated": {"line": 1, "column": 11}}]})"),
       0, "PASS c"},
      {"DuplicatesFail",
       one_case(
         "d.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "duplicate_rejection", "first": {"line": 1, "column": 3}, "repeated": {"line": 1, "column": 11}}]})"),
       1, "FAIL c"},
      {"Spelling",
       one_case(
         "n.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "number_spelling", "pointer": "/3", "spelling": "0x10000000000000001"}]})"),
       0, "PASS c"},
      {"SpellingFail",
       one_case(
         "n.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "number_spelling", "pointer": "/3", "spelling": "0x1"}]})"),
       1, "FAIL c"},
      {"Conversions",
       one_case(
         "n.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "conversion", "pointer": "/0", "to": "int64", "result": "ok", "value": "9007199254740993"}, {"check": "conversion", "pointer": "/1", "to": "int64", "result": "non_integral"}, {"check": "conversion", "pointer": "/2", "to": "double", "result": "non_finite"}, {"check": "conversion", "pointer": "/3", "to": "uint64", "result": "out_of_range"}, {"check": "conversion", "pointer": "/4", "to": "double", "result": "out_of_range"}]})"),
       0, "PASS c"},
      {"ConversionValueFails",
       one_case(
         "n.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "conversion", "pointer": "/0", "to": "int64", "result": "ok", "value": "9007199254740992"}]})"),
       1, "FAIL c"},
      {"AbsentMember",
       one_case("d.json5", "json5",
                R"({"parse": "success", "checks": [{"check": "absent_member", "pointer": "", "name": "label"}]})"),
       0, "PASS c"},
      {"AbsentMemberFails",
       one_case("d.json5", "json5",
                R"({"parse": "success", "checks": [{"check": "absent_member", "pointer": "", "name": "other"}]})"),
       1, "FAIL c"},
      {"UnknownCheckFails",
       one_case("d.json5", "json5", R"({"parse": "success", "checks": [{"check": "telepathy", "pointer": ""}]})"), 1,
       "FAIL c"},
      {"BadPointerFails",
       one_case("d.json5", "json5",
                R"({"parse": "success", "checks": [{"check": "absent_member", "pointer": "/nope/x", "name": "a"}]})"),
       1, "FAIL c"},
      {"MissingInput", one_case("none.json", "json", R"({"parse": "success", "exit": 0})"), 2, "ERROR c"},
      {"DuplicatesPolicy",
       one_case("d.json5", "json5", R"({"parse": "error", "exit": 1})", R"(, "duplicates": "reject")"), 0, "PASS c"},
      {"ParseErrorExpectedButValid", one_case("ok.json", "json", R"({"parse": "error"})"), 1, "FAIL c"},
      {"MoreConversions",
       one_case(
         "n.json5", "json5",
         R"({"parse": "success", "checks": [{"check": "conversion", "pointer": "/5", "to": "int64", "result": "ok", "value": "-5"}, {"check": "conversion", "pointer": "/0", "to": "uint64", "result": "ok", "value": "9007199254740993"}, {"check": "conversion", "pointer": "/1", "to": "double", "result": "ok"}, {"check": "conversion", "pointer": "/5", "to": "uint64", "result": "out_of_range"}]})"),
       0, "PASS c"},
      {"ConversionOfString",
       failing_check("u.json5", R"({"check": "conversion", "pointer": "/pair", "to": "int64", "result": "ok"})"), 1,
       "FAIL c"},
      {"ConversionWithoutTarget",
       failing_check("n.json5", R"({"check": "conversion", "pointer": "/0", "result": "ok"})"), 1, "FAIL c"},
      {"ConversionWithoutResult",
       failing_check("n.json5", R"({"check": "conversion", "pointer": "/0", "to": "int64"})"), 1, "FAIL c"},
      {"MemberNamesOfNumber",
       failing_check("d.json5", R"({"check": "member_names", "pointer": "/name", "names": ["a"]})"), 1, "FAIL c"},
      {"MemberNamesWithoutNames", failing_check("d.json5", R"({"check": "member_names", "pointer": ""})"), 1, "FAIL c"},
      {"MemberNamesNotStrings",
       failing_check("d.json5", R"({"check": "member_names", "pointer": "", "names": [1, 2, 3]})"), 1, "FAIL c"},
      {"MemberNamesTooMany",
       failing_check("d.json5",
                     R"({"check": "member_names", "pointer": "", "names": ["name", "name", "other", "extra"]})"),
       1, "FAIL c"},
      {"MemberNamesDiffer",
       failing_check("d.json5", R"({"check": "member_names", "pointer": "", "names": ["name", "nome", "other"]})"), 1,
       "FAIL c"},
      {"UnitsOfNumber", failing_check("n.json5", R"({"check": "string_units", "pointer": "/0", "units": [1]})"), 1,
       "FAIL c"},
      {"UnitsWithoutUnits", failing_check("u.json5", R"({"check": "string_units", "pointer": "/nul"})"), 1, "FAIL c"},
      {"UnitsNotNumbers",
       failing_check("u.json5", R"({"check": "string_units", "pointer": "/nul", "units": ["a", 0, 98]})"), 1, "FAIL c"},
      {"UnitsDiffer", failing_check("u.json5", R"({"check": "string_units", "pointer": "/nul", "units": [97, 1, 98]})"),
       1, "FAIL c"},
      {"SameStringWithoutOther", failing_check("u.json5", R"({"check": "same_string", "pointer": "/pair"})"), 1,
       "FAIL c"},
      {"SameStringOtherMissing",
       failing_check("u.json5", R"({"check": "same_string", "pointer": "/pair", "other": "/none"})"), 1, "FAIL c"},
      {"SameStringOfNumber", failing_check("n.json5", R"({"check": "same_string", "pointer": "/0", "other": "/1"})"), 1,
       "FAIL c"},
      {"DuplicatesWithoutDuplicates",
       failing_check(
         "n.json5",
         R"({"check": "duplicate_rejection", "first": {"line": 1, "column": 1}, "repeated": {"line": 1, "column": 1}})"),
       1, "FAIL c"},
      {"DuplicatesWithoutLocations", failing_check("d.json5", R"({"check": "duplicate_rejection"})"), 1, "FAIL c"},
      {"SpellingOfString",
       failing_check("u.json5", R"({"check": "number_spelling", "pointer": "/pair", "spelling": "1"})"), 1, "FAIL c"},
      {"SpellingWithoutSpelling", failing_check("n.json5", R"({"check": "number_spelling", "pointer": "/0"})"), 1,
       "FAIL c"},
      {"Utf8OfNumber", failing_check("n.json5", R"({"check": "utf8_export", "pointer": "/0", "result": "success"})"), 1,
       "FAIL c"},
      {"Utf8WithoutResult", failing_check("u.json5", R"({"check": "utf8_export", "pointer": "/raw"})"), 1, "FAIL c"},
      {"AbsentWithoutName", failing_check("d.json5", R"({"check": "absent_member", "pointer": ""})"), 1, "FAIL c"},
      {"CheckWithoutKind", failing_check("d.json5", R"({"pointer": "", "description": "no kind"})"), 1, "FAIL c"},
      {"CheckWithoutPointer", failing_check("d.json5", R"({"check": "absent_member", "name": "x"})"), 1, "FAIL c"},
      {"WarningsWithoutCount", failing_check("u.json5", R"({"check": "warnings"})"), 1, "FAIL c"},
      {"CheckNotAnObject", failing_check("d.json5", "1"), 1, "FAIL c"},
      {"ArrayIndexNotANumber", failing_check("n.json5", R"({"check": "absent_member", "pointer": "/x", "name": "a"})"),
       1, "FAIL c"},
      {"ArrayIndexTooLong",
       failing_check("n.json5", R"({"check": "absent_member", "pointer": "/1234567890", "name": "a"})"), 1, "FAIL c"},
      {"ChecksNotAnArray", one_case("d.json5", "json5", R"({"parse": "success", "checks": 5})"), 0, "PASS c"},
      {"NameTooLong",
       failing_check("d.json5",
                     R"({"check": "absent_member", "pointer": "", "name": ")" + std::string(300U, 'x') + "\"}"),
       1, "FAIL c"},
      {"NameNotUtf8", failing_check("d.json5", R"({"check": "absent_member", "pointer": "", "name": "\uD800"})"), 1,
       "FAIL c"},
      {"PointerTokenTooLong",
       failing_check("d.json5",
                     R"({"check": "absent_member", "pointer": "/)" + std::string(200U, 'y') + R"(/z", "name": "a"})"),
       1, "FAIL c"},
      {"EmptyArrayToken", failing_check("n.json5", R"({"check": "absent_member", "pointer": "/", "name": "a"})"), 1,
       "FAIL c"},
      {"WarningsCountNotANumber", failing_check("u.json5", R"({"check": "warnings", "count": "one"})"), 1, "FAIL c"},
      {"MemberNamesNotAnArray", failing_check("d.json5", R"({"check": "member_names", "pointer": "", "names": 5})"), 1,
       "FAIL c"},
      {"UnitsNotAnArray", failing_check("u.json5", R"({"check": "string_units", "pointer": "/nul", "units": 5})"), 1,
       "FAIL c"},
      {"DuplicatesWithoutRepeated",
       failing_check("d.json5", R"({"check": "duplicate_rejection", "first": {"line": 1, "column": 2}})"), 1, "FAIL c"},
      {"DuplicatesLineDiffers",
       failing_check(
         "d.json5",
         R"({"check": "duplicate_rejection", "first": {"line": 2, "column": 2}, "repeated": {"line": 1, "column": 11}})"),
       1, "FAIL c"},
      {"DuplicatesRepeatedDiffers",
       failing_check(
         "d.json5",
         R"({"check": "duplicate_rejection", "first": {"line": 1, "column": 2}, "repeated": {"line": 1, "column": 12}})"),
       1, "FAIL c"},
      {"InvalidUtf8Rejected", one_case("enc.json", "json", R"({"parse": "error", "exit": 1})"), 0, "PASS c"},
      {"CaseWithoutSyntax", R"({"format_version": 2, "cases": [{"id": "c", "input": "ok.json", "expect": {}}]})", 2,
       "ERROR c"},
      {"ValidationWithoutSchema", one_case("ok.json", "json", R"({"parse": "success", "validation": "success"})"), 0,
       "PASS c"},
      {"PreservePolicy",
       one_case("d.json5", "json5", R"({"parse": "success", "exit": 0})", R"(, "duplicates": "preserve")"), 0,
       "PASS c"},
      {"RequirementsNotAnArray",
       R"({"format_version": 2, "cases": [{"id": "c", "input": "ok.json", "syntax": "json", "expect": {}}], "requirements": 5})",
       0, "PASS c"},
      {"StarterInputsNotAnArray",
       R"({"format_version": 2, "cases": [{"id": "c", "input": "ok.json", "syntax": "json", "expect": {}}, {"id": "x"}], "requirements": [{"requirement": "J5-Z", "starter_inputs": 5}, {"requirement": "J5-W", "starter_inputs": ["ok.json"]}]})",
       2, "PASS c"},
      {"CaseWithoutExpect",
       R"({"format_version": 2, "cases": [{"id": "c", "input": "d.json5", "syntax": "json5"}], "requirements": []})", 2,
       "ERROR c"},
      {"CaseWithoutInput", R"({"format_version": 2, "cases": [{"id": "c", "syntax": "json5", "expect": {}}]})", 2,
       "ERROR c"},
      {"CaseNotAnObject",
       R"({"format_version": 2, "cases": [{"id": "c", "input": 5, "syntax": "json", "expect": {}}]})", 2, "ERROR c"},
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
      {"SchemaFailureUnexpected",
       one_case("wrong.json", "json", R"({"parse": "success", "validation": "success", "exit": 1})",
                R"(, "schema": "s.json")"),
       1, "FAIL c"},
      {"SchemaWithParseError",
       one_case("bad.json", "json", R"({"parse": "error", "exit": 1})", R"(, "schema": "s.json")"), 0, "PASS c"},
      {"SchemaInstanceOutsideDomain",
       one_case("n.json5", "json5", R"({"parse": "success", "exit": 1})", R"(, "schema": "s.json")"), 0, "PASS c"},
      {"SchemaNotJson",
       one_case("ok.json", "json", R"({"parse": "success", "validation": "success"})", R"(, "schema": "bad.json")"), 2,
       "ERROR c"},
      {"SchemaMissing",
       one_case("ok.json", "json", R"({"parse": "success", "validation": "success"})", R"(, "schema": "none.json")"), 2,
       "ERROR c"},
      {"SchemaInvalid",
       one_case("ok.json", "json", R"({"parse": "success", "validation": "success"})",
                R"(, "schema": "bad-schema.json")"),
       2, "ERROR c"},
#endif
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
      {"SchemaValid",
       one_case("ok.json", "json", R"({"parse": "success", "validation": "success", "exit": 0})",
                R"(, "schema": "s.json")"),
       0, "PASS c"},
      {"SchemaMismatch",
       one_case("ok.json", "json", R"({"parse": "success", "validation": "failure", "exit": 1})",
                R"(, "schema": "s.json")"),
       1, "FAIL c"},
#else
      {"SchemaUnsupported",
       one_case("ok.json", "json", R"({"parse": "success", "validation": "success", "exit": 0})",
                R"(, "schema": "s.json")"),
       0, "UNSUPPORTED c"},
#endif
    };
  }

  class P058_Run : public testing::TestWithParam<run_case>
  {
  };

  /*
   * Arrange: One-case manifests exercising each expectation and each structured check, in a
   *          passing and a failing form, plus missing inputs and unsupported capabilities.
   * Act: Run the manifest.
   * Expect: The case label and the run exit contract: 0 all supported matched, 1 mismatch,
   *         2 operational; unsupported never counts as a pass (J5-070, J5-094).
   */
  TEST_P(P058_Run, AppliesTheExitContract)
  {
    fake_host host;
    add_inputs(host);
    EXPECT_EQ(run_manifest_text(host, GetParam().manifest), GetParam().exit) << host.out << host.err;
    EXPECT_NE(host.out.find(std::string{GetParam().label} + "\n"), std::string::npos) << host.out;
  }

  INSTANTIATE_TEST_SUITE_P(Cases, P058_Run, testing::ValuesIn(run_cases()),
                           [](const testing::TestParamInfo<run_case>& param_info)
                           {
                             return std::string{param_info.param.name};
                           });

  /* ================================= Test_P058_RunReport ================================= */
  /*
   * Arrange: A passing one-case manifest.
   * Act: Run it in text and JSON format.
   * Expect: The requirement summary marks the requirement with the input "demonstrated" and
   *         the one without inputs "not demonstrated by file cases"; the JSON report parses and
   *         carries the summary counts (J5-071, J5-094).
   */
  TEST(P058_RunReport, SummarisesRequirements)
  {
    fake_host host;
    add_inputs(host);
    const std::string manifest = one_case("bad.json", "json", R"({"parse": "error", "exit": 1})");
    EXPECT_EQ(run_manifest_text(host, manifest), 0);
    EXPECT_NE(host.out.find("  J5-X: demonstrated\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("  J5-Y: not demonstrated by file cases\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("1 passed, 0 failed, 0 unsupported, 0 errors"), std::string::npos) << host.out;
    host.files["dir/m.json"] = manifest;
    host.out.clear();
    EXPECT_EQ(host.run({"run", "--diagnostics", "json", "--manifest", "dir/m.json"}), 0);
    EXPECT_NE(host.out.find(R"("summary": {"passed": 1, "failed": 0, "unsupported": 0, "errors": 0})"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find(R"("result": "pass", "exit": 0})"), std::string::npos);
  }

  /*
   * Arrange: A missing manifest, a malformed one and one without a case list.
   * Act: Run.
   * Expect: exit 2 with a located error, never a pass (J5-070).
   */
  TEST(P058_RunReport, ManifestFailuresAreOperational)
  {
    fake_host host;
    EXPECT_EQ(host.run({"run", "--manifest", "missing.json"}), 2);
    EXPECT_NE(host.err.find("cannot open input"), std::string::npos);
    EXPECT_EQ(run_manifest_text(host, "{\"cases\": ["), 2);
    EXPECT_EQ(run_manifest_text(host, R"({"format_version": 2})"), 2);
    EXPECT_NE(host.out.find("manifest is not a valid case list"), std::string::npos) << host.out << host.err;
    EXPECT_EQ(run_manifest_text(host, "[]"), 2);
    EXPECT_EQ(run_manifest_text(host, R"({"cases": 5})"), 2);
    host.files["m.json"] = R"({"cases": [], "requirements": []})";
    EXPECT_EQ(host.run({"run", "--manifest", "m.json"}), 0);
  }

  /*
   * Arrange: A manifest whose cases share one input (pass, then fail) plus a second requirement
   *          whose input case errors, and a case with a described failing check.
   * Act: Run it in text and JSON format.
   * Expect: The worst result wins per requirement ("failed"), mismatches carry their
   *         description, the JSON report lists every case and mismatch and the run exits 2 for
   *         the error case (J5-070, J5-071, J5-094).
   */
  TEST(P058_RunReport, WorstResultWinsAndJsonListsMismatches)
  {
    fake_host host;
    add_inputs(host);
    const std::string manifest = R"({"format_version": 2, "cases": [
      {"id": "a", "input": "d.json5", "syntax": "json5", "expect": {"parse": "success"}},
      {"id": "b", "input": "d.json5", "syntax": "json5", "expect": {"parse": "success", "checks": [
        {"check": "absent_member", "pointer": "", "name": "other", "description": "other is absent"},
        {"check": "warnings", "count": 7}]}},
      {"id": "e", "input": "none.json", "syntax": "json", "expect": {"parse": "success"}}],
      "requirements": [{"requirement": "J5-A", "starter_inputs": ["d.json5"]},
                       {"requirement": "J5-B", "starter_inputs": ["none.json", 5]},
                       {"requirement": "J5-C"}, 7]})";
    EXPECT_EQ(run_manifest_text(host, manifest), 2) << host.out;
    EXPECT_NE(host.out.find("  b: absent_member (other is absent)\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("  b: warnings\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("  J5-A: failed\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("  J5-B: failed\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("  J5-C: not demonstrated by file cases\n"), std::string::npos) << host.out;
    host.out.clear();
    EXPECT_EQ(host.run({"run", "--diagnostics", "json", "--manifest", "dir/m.json"}), 2);
    EXPECT_NE(host.out.find(R"({"what": "absent_member", "detail": "other is absent"}, {"what": "warnings")"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find(R"(, {"id": "e", "mismatches": [], "result": "error"})"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find(R"(, {"requirement": "J5-B", "status": "failed"})"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find(R"("result": "error", "exit": 2})"), std::string::npos) << host.out;
  }

  /*
   * Arrange: A manifest with only failing cases, in JSON format.
   * Act: Run it.
   * Expect: Result "fail" and exit 1 (J5-070).
   */
  TEST(P058_RunReport, JsonFailResult)
  {
    fake_host host;
    add_inputs(host);
    host.files["dir/m.json"] = one_case("bad.json", "json", R"({"parse": "success"})");
    EXPECT_EQ(host.run({"run", "--diagnostics", "json", "--manifest", "dir/m.json"}), 1);
    EXPECT_NE(host.out.find(R"("result": "fail", "exit": 1})"), std::string::npos) << host.out;
  }

  /*
   * Arrange: A passing manifest; the output sink fails, or the run's limits are too small for
   *          the case input.
   * Act: Run it.
   * Expect: exit 2: a lost report is never a pass, and a limit failure is operational (J5-070).
   */
  TEST(P058_RunReport, WriteAndLimitFailuresAreOperational)
  {
    fake_host host;
    add_inputs(host);
    host.files["dir/m.json"] = one_case("n.json5", "json5", R"({"parse": "success", "exit": 0})");
    host.fail_writes = true;
    EXPECT_EQ(host.run({"run", "--manifest", "dir/m.json"}), 2);
    host.fail_writes = false;
    EXPECT_EQ(host.run({"run", "--max-nodes", "2", "--manifest", "dir/m.json"}), 2);
    EXPECT_NE(host.out.find("ERROR c\n"), std::string::npos) << host.out;
  }

  /* ================================= Test_P058_Pointers ================================= */
  /*
   * Arrange: A document with nested members, escaped names and arrays.
   * Act: Resolve pointers.
   * Expect: RFC 6901 resolution including "" for the root, ~0 and ~1, array indices; nullopt for
   *         absent members, bad indices and malformed pointers (J5-070).
   */
  TEST(P058_Pointers, ResolveRfc6901)
  {
    fake_host host;
    const std::string text = R"({"a": {"b/c": [10, 20], "d~e": true}, "": 5})";
    accounted_storage accounting{host.environment().storage, parse_options{}.limits.max_owned_bytes};
    const auto parsed = parse_document({text.data(), text.size()}, parse_options{}, accounting);
    ASSERT_TRUE(parsed.has_value());
    const value_view root = *parsed.value_if()->values.root().value_if();
    EXPECT_TRUE(cli::resolve_pointer(root, "").has_value());
    EXPECT_EQ(*cli::resolve_pointer(root, "/a/b~1c/1")->kind().value_if(), value_kind::number);
    EXPECT_EQ(*cli::resolve_pointer(root, "/a/d~0e")->kind().value_if(), value_kind::boolean);
    EXPECT_TRUE(cli::resolve_pointer(root, "/").has_value());
    EXPECT_FALSE(cli::resolve_pointer(root, "a").has_value());
    EXPECT_FALSE(cli::resolve_pointer(root, "/x").has_value());
    EXPECT_FALSE(cli::resolve_pointer(root, "/a/b~1c/2").has_value());
    EXPECT_FALSE(cli::resolve_pointer(root, "/a/b~1c/-1").has_value());
    EXPECT_FALSE(cli::resolve_pointer(root, "/a/b~").has_value());
  }

  /* ================================== Test_P058_Inspect ================================== */
  /*
   * Arrange: Strings with surrogates and NUL, and numbers outside each conversion range.
   * Act: Run inspect in text and JSON format.
   * Expect: Ordered values with positions, $utf16 for unpaired surrogates, raw spellings and
   *         each checked conversion's value or failure reason; the JSON output parses as strict
   *         JSON (J5-030, J5-034, J5-041, J5-071).
   */
  TEST(P058_Inspect, ShowsValuesAndConversions)
  {
    fake_host host;
    add_inputs(host);
    EXPECT_EQ(host.run({"inspect", "--syntax", "json5", "dir/u.json5"}), 0);
    EXPECT_NE(host.out.find("  \"lone\": string (line 1, column 29) {\"$utf16\": [55296]}\n"), std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find("\"nul\": string (line 1, column 44) \"a\\u0000b\""), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("warning: "), std::string::npos);
    host.out.clear();
    EXPECT_EQ(host.run({"inspect", "--syntax", "json5", "dir/n.json5"}), 0);
    EXPECT_NE(host.out.find("\"1.5\" (decimal; int64 error non_integral; uint64 error non_integral; double 1.5)"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find("\"NaN\" (nan; int64 error non_finite"), std::string::npos);
    host.out.clear();
    EXPECT_EQ(host.run({"inspect", "--diagnostics", "json", "--syntax", "json5", "dir/n.json5"}), 0);
    EXPECT_NE(host.out.find(R"("spelling": "9007199254740993", "number_kind": "decimal", "int64": 9007199254740993)"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find(R"("uint64": {"error": "out_of_range"})"), std::string::npos);
    EXPECT_NE(host.out.find(R"("result": "valid", "exit": 0})"), std::string::npos);
  }

  /*
   * Arrange: A document with every value kind and number spelling (null, booleans, hexadecimal,
   *          signed infinities, negative and fractional numbers, an underflowing exponent),
   *          nested and empty containers and two string warnings.
   * Act: Inspect it in text and JSON format, in JSON syntax, and with a failing output sink.
   * Expect: Every kind is named with its payload and conversions, non-finite numbers keep their
   *         spelling with a non_finite conversion error, warnings are listed in order, nested members are indented
   *         (text) or nested (JSON); a lost output is exit 2 (J5-021, J5-034, J5-041, J5-071).
   */
  TEST(P058_Inspect, EveryKindInBothFormats)
  {
    fake_host host;
    host.files["all.json5"] =
      R"({n: null, t: true, f: false, h: 0xFF, i: Infinity, j: -Infinity, m: -5, d: -0.25, u: 1e-400,
          e: "", a: [[1], {k: []}, {}], w: ["\uD800", "\uDC00"]})";
    host.files["s.json"] = R"({"a": [1, {"b": null}]})";
    EXPECT_EQ(host.run({"inspect", "--syntax", "json5", "all.json5"}), 0);
    EXPECT_NE(host.out.find("  \"n\": null (line 1, column 5)\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("\"t\": boolean (line 1, column 14) true\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("\"f\": boolean (line 1, column 23) false\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("\"0xFF\" (hexadecimal; int64 255; uint64 255; double 255)"), std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find("\"Infinity\" (infinity; int64 error non_finite"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find(
                "\"-Infinity\" (infinity; int64 error non_finite; uint64 error non_finite; double error non_finite)"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find("double error underflow)"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("\"-5\" (decimal; int64 -5; uint64 error out_of_range; double -5)"), std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find("double -0.25)"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("\"1e-400\" (decimal; int64 error"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("\"e\": string (line 2, column 14) \"\"\n"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("      \"k\": array"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find("all.json5: warning: "), std::string::npos) << host.out;
    host.out.clear();
    EXPECT_EQ(host.run({"inspect", "--diagnostics", "json", "--syntax", "json5", "all.json5"}), 0);
    EXPECT_NE(host.out.find(R"("warnings": [{"message": )"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find(R"(, {"message": )"), std::string::npos) << host.out;
    EXPECT_NE(host.out.find(R"({"name": "t", "kind": "boolean", "line": 1, "column": 14, "value": true})"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find(R"("spelling": "-Infinity", "number_kind": "infinity", "int64": {"error": "non_finite"})"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find(R"("kind": "array", "line": 2, "column": 21, "elements": [{"kind": "array")"),
              std::string::npos)
      << host.out;
    EXPECT_NE(host.out.find(R"("value": {"$utf16": [55296]})"), std::string::npos) << host.out;
    host.out.clear();
    EXPECT_EQ(host.run({"inspect", "--diagnostics", "json", "--syntax", "json", "s.json"}), 0);
    EXPECT_NE(host.out.find(R"("syntax": "json", "warnings": [], "value": {"kind": "object")"), std::string::npos)
      << host.out;
    host.fail_writes = true;
    EXPECT_EQ(host.run({"inspect", "--syntax", "json", "s.json"}), 2);
  }

  /*
   * Arrange: Malformed input, a missing file and allocation n failing for every n.
   * Act: Run inspect.
   * Expect: exit 1 for invalid input, 2 for I/O and allocation failures, nothing leaks
   *         (J5-046, J5-070).
   */
  TEST(P058_Inspect, FailuresAndAllocationSweep)
  {
    fake_host host;
    add_inputs(host);
    EXPECT_EQ(host.run({"inspect", "--syntax", "json", "dir/bad.json"}), 1);
    EXPECT_EQ(host.run({"inspect", "--syntax", "json", "none.json"}), 2);
    EXPECT_EQ(host.run({"inspect", "--syntax", "json5", "--max-depth", "999999999999999999", "dir/n.json5"}), 2);
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      fake_host failing;
      add_inputs(failing);
      failing.fail_on = fail_on;
      const int exit = failing.run({"inspect", "--diagnostics", "json", "--syntax", "json5", "dir/n.json5"});
      if (exit == 0)
      {
        break;
      }
      ASSERT_EQ(exit, 2) << fail_on;
      ASSERT_LT(fail_on, 500U);
    }
  }

  /*
   * Arrange: A passing manifest and allocation n failing for every n.
   * Act: Run it.
   * Expect: exit 0 once enough memory is available and 2 (or an ERROR case) before, never a
   *         crash or a false pass, with everything released (J5-046, J5-070).
   */
  TEST(P058_RunReport, AllocationSweep)
  {
    const std::string manifest = one_case(
      "n.json5", "json5",
      R"({"parse": "success", "checks": [{"check": "conversion", "pointer": "/4", "to": "double", "result": "out_of_range"}]})");
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      fake_host host;
      add_inputs(host);
      host.files["dir/m.json"] = manifest;
      host.fail_on = fail_on;
      const int exit = host.run({"run", "--manifest", "dir/m.json"});
      if (exit == 0)
      {
        EXPECT_NE(host.out.find("PASS c\n"), std::string::npos);
        break;
      }
      ASSERT_TRUE((exit == 2) || (exit == 1)) << fail_on;
      EXPECT_EQ(host.out.find("PASS c\n"), std::string::npos) << fail_on;
      ASSERT_LT(fail_on, 2000U);
    }
  }
} // namespace
