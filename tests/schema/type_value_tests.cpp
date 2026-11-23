#include "adaptive_json5/schema.hpp"
#include "schema_run.hpp"

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::run_schema;
  using adaptive_json5_test::schema_verdict;

  struct verdict_case
  {
      const char* name;
      const char* schema;
      const char* instance;
      bool valid;
  };

  /* =================================== Test_P047_Type ==================================== */
  class P047_Type : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: Every Draft-07 type name, type unions and integers spelled in every JSON5 form.
   * Act: Validate.
   * Expect: Kinds match by value; integer is decided by mathematical value, not spelling;
   *         number includes integers (J5-057, J5-059).
   */
  TEST_P(P047_Type, MatchesDraft07)
  {
    EXPECT_EQ(schema_verdict(GetParam().schema, GetParam().instance), std::optional<bool>{GetParam().valid})
      << GetParam().schema << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P047_Type,
    testing::Values(verdict_case{"null_ok", R"({"type": "null"})", "null", true},
                    verdict_case{"null_bad", R"({"type": "null"})", "false", false},
                    verdict_case{"boolean_ok", R"({"type": "boolean"})", "true", true},
                    verdict_case{"boolean_not_number", R"({"type": "boolean"})", "1", false},
                    verdict_case{"object_ok", R"({"type": "object"})", "{}", true},
                    verdict_case{"object_not_array", R"({"type": "object"})", "[]", false},
                    verdict_case{"array_ok", R"({"type": "array"})", "[1]", true},
                    verdict_case{"string_ok", R"({"type": "string"})", R"("x")", true},
                    verdict_case{"string_not_number", R"({"type": "string"})", "1", false},
                    verdict_case{"number_integer", R"({"type": "number"})", "1", true},
                    verdict_case{"number_fraction", R"({"type": "number"})", "1.5", true},
                    verdict_case{"number_not_string", R"({"type": "number"})", R"("1")", false},
                    verdict_case{"integer_plain", R"({"type": "integer"})", "1", true},
                    verdict_case{"integer_point_zero", R"({"type": "integer"})", "1.0", true},
                    verdict_case{"integer_exponent", R"({"type": "integer"})", "1e0", true},
                    verdict_case{"integer_scaled", R"({"type": "integer"})", "12.5e1", true},
                    verdict_case{"integer_hex", R"({"type": "integer"})", "0x1F", true},
                    verdict_case{"integer_negative_zero", R"({"type": "integer"})", "-0", true},
                    verdict_case{"integer_huge", R"({"type": "integer"})", "123456789012345678901234567890", true},
                    verdict_case{"integer_huge_exponent", R"({"type": "integer"})", "1e400", true},
                    verdict_case{"integer_fraction", R"({"type": "integer"})", "1.5", false},
                    verdict_case{"integer_small_exponent", R"({"type": "integer"})", "1e-1", false},
                    verdict_case{"integer_close", R"({"type": "integer"})", "1.0000000000000000000001", false},
                    verdict_case{"integer_not_bool", R"({"type": "integer"})", "true", false},
                    verdict_case{"union_ok", R"({"type": ["string", "null"]})", "null", true},
                    verdict_case{"union_bad", R"({"type": ["string", "null"]})", "0", false},
                    verdict_case{"union_integer", R"({"type": ["integer", "boolean"]})", "2.0", true},
                    verdict_case{"union_integer_bad", R"({"type": ["integer", "boolean"]})", "2.5", false},
                    verdict_case{"nested_type", R"({"properties": {"a": {"type": "integer"}}})", R"({"a": "x"})",
                                 false}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: Malformed type keywords.
   * Act: Compile.
   * Expect: invalid_schema (keyword_shape).
   */
  TEST(P047_TypeShape, RejectsMalformedTypes)
  {
    for (const char* schema : {R"({"type": "float"})", R"({"type": []})", R"({"type": ["string", "string"]})",
                               R"({"type": 5})", R"({"type": ["string", 1]})", R"({"type": ["int"]})"})
    {
      const auto outcome = run_schema(schema, "1");
      EXPECT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::invalid_schema}) << schema;
      EXPECT_EQ(outcome.reason, std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape}) << schema;
    }
  }

  /* =================================== Test_P047_EnumConst ==================================== */
  class P047_EnumConst : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: enum and const with scalars, containers and equivalent number spellings.
   * Act: Validate.
   * Expect: Type-aware exact equality (J5-061): equal values pass, others fail.
   */
  TEST_P(P047_EnumConst, UsesExactEquality)
  {
    EXPECT_EQ(schema_verdict(GetParam().schema, GetParam().instance), std::optional<bool>{GetParam().valid})
      << GetParam().schema << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P047_EnumConst,
    testing::Values(verdict_case{"enum_number_spelling", R"({"enum": [1, "a", null]})", "1.0", true},
                    verdict_case{"enum_hex", R"({"enum": [16]})", "0x10", true},
                    verdict_case{"enum_string", R"({"enum": [1, "a", null]})", R"('a')", true},
                    verdict_case{"enum_null", R"({"enum": [1, "a", null]})", "null", true},
                    verdict_case{"enum_bool_not_number", R"({"enum": [1, 0]})", "true", false},
                    verdict_case{"enum_missing", R"({"enum": [1, "a"]})", R"("b")", false},
                    verdict_case{"enum_object_reordered", R"({"enum": [{"a": [1, 2], "b": null}]})",
                                 R"({b: null, a: [1.0, 2]})", true},
                    verdict_case{"enum_array_reordered", R"({"enum": [[1, 2]]})", "[2, 1]", false},
                    verdict_case{"enum_empty", R"({"enum": []})", "1", false},
                    verdict_case{"const_object", R"({"const": {"a": [1, {"b": "c"}]}})", R"({"a": [1e0, {"b": "c"}]})",
                                 true},
                    verdict_case{"const_extra_member", R"({"const": {"a": 1}})", R"({"a": 1, "b": 2})", false},
                    verdict_case{"const_false_not_zero", R"({"const": false})", "0", false},
                    verdict_case{"const_zero_not_false", R"({"const": 0})", "false", false},
                    verdict_case{"const_null", R"({"const": null})", "null", true},
                    verdict_case{"const_negative_zero", R"({"const": 0})", "-0.0", true},
                    verdict_case{"const_huge", R"({"const": 100000000000000000000})", "1e20", true},
                    verdict_case{"const_close", R"({"const": 0.1})", "0.10000000000000001", false},
                    verdict_case{"const_string_escape", R"({"const": "é"})", R"("é")", true},
                    verdict_case{"under_ref_ignored",
                                 R"({"definitions": {"t": true}, "$ref": "#/definitions/t", "const": 1})", "2", true}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: A non-array enum.
   * Act: Compile.
   * Expect: invalid_schema (keyword_shape).
   */
  TEST(P047_EnumConstShape, RejectsNonArrayEnum)
  {
    const auto outcome = run_schema(R"({"enum": {"a": 1}})", "1");
    EXPECT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::invalid_schema});
    EXPECT_EQ(outcome.reason, std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape});
  }

  /*
   * Arrange: One instance failing type, enum and const at once.
   * Act: Validate collect-all and fail-fast.
   * Expect: Each failed keyword is its own diagnostic, in keyword order.
   */
  TEST(P047_EnumConstShape, ReportsEachFailedKeyword)
  {
    const auto outcome = run_schema(R"({"type": "string", "enum": ["a"], "const": "a"})", "1");
    ASSERT_FALSE(outcome.error.has_value());
    EXPECT_FALSE(outcome.valid);
    const std::vector<schema_keyword> expected{schema_keyword::const_value, schema_keyword::enum_values,
                                               schema_keyword::type};
    EXPECT_EQ(outcome.keywords, expected);
  }

  /*
   * Arrange: A 50-element const array and a shorter instance.
   * Act: Validate.
   * Expect: The whole value was copied into the compiled schema and compared exactly.
   */
  TEST(P047_EnumConstShape, CopiesLargeValues)
  {
    std::string big = R"({"const": [)";
    for (int index = 0; index < 50; ++index)
    {
      big += (index == 49) ? "0" : "0, ";
    }
    big += "]}";
    EXPECT_EQ(schema_verdict(big, "[0]"), std::optional<bool>{false});
  }
} // namespace
