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

  /* =================================== Test_P051_Objects ==================================== */
  class P051_Objects : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: Missing fields, overlapping patterns, dynamic names, dependent schemas, unknown
   *          fields and non-object instances.
   * Act: Validate.
   * Expect: Draft-07 object keyword semantics (J5-057).
   */
  TEST_P(P051_Objects, MatchDraft07)
  {
    EXPECT_EQ(schema_verdict(GetParam().schema, GetParam().instance), std::optional<bool>{GetParam().valid})
      << GetParam().schema << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P051_Objects,
    testing::Values(
      verdict_case{"min_properties_equal", R"({"minProperties": 1})", R"({"a": 1})", true},
      verdict_case{"min_properties_below", R"({"minProperties": 1})", "{}", false},
      verdict_case{"max_properties_above", R"({"maxProperties": 1})", R"({"a": 1, "b": 2})", false},
      verdict_case{"required_present", R"({"required": ["a", "b"]})", R"({"b": null, "a": 0})", true},
      verdict_case{"required_missing", R"({"required": ["a", "b"]})", R"({"a": 0})", false},
      verdict_case{"required_escaped_name", R"({"required": ["é"]})", R"({"é": 1})", true},
      verdict_case{"required_empty", R"({"required": []})", "{}", true},
      verdict_case{"required_not_object", R"({"required": ["a"]})", "[]", true},
      verdict_case{"pattern_match", R"({"patternProperties": {"^x-": {"type": "string"}}})", R"({"x-a": "s"})", true},
      verdict_case{"pattern_bad", R"({"patternProperties": {"^x-": {"type": "string"}}})", R"({"x-a": 1})", false},
      verdict_case{"pattern_unanchored", R"({"patternProperties": {"b": false}})", R"({"abc": 1})", false},
      verdict_case{"patterns_overlap", R"({"patternProperties": {"a": {"minimum": 2}, "b": {"maximum": 3}}})",
                   R"({"ab": 4})", false},
      verdict_case{"patterns_overlap_ok", R"({"patternProperties": {"a": {"minimum": 2}, "b": {"maximum": 3}}})",
                   R"({"ab": 3})", true},
      verdict_case{"properties_and_pattern",
                   R"({"properties": {"ab": {"type": "integer"}}, "patternProperties": {"a": {"minimum": 5}}})",
                   R"({"ab": 6})", true},
      verdict_case{"properties_and_pattern_bad",
                   R"({"properties": {"ab": {"type": "integer"}}, "patternProperties": {"a": {"minimum": 5}}})",
                   R"({"ab": 4})", false},
      verdict_case{"additional_false", R"({"properties": {"a": {}}, "additionalProperties": false})",
                   R"({"a": 1, "b": 2})", false},
      verdict_case{"additional_false_ok", R"({"properties": {"a": {}}, "additionalProperties": false})", R"({"a": 1})",
                   true},
      verdict_case{"additional_pattern_covers", R"({"patternProperties": {"^v": {}}, "additionalProperties": false})",
                   R"({"v1": 1, "v2": 2})", true},
      verdict_case{"additional_schema", R"({"properties": {"a": {}}, "additionalProperties": {"type": "boolean"}})",
                   R"({"a": 1, "b": true})", true},
      verdict_case{"additional_schema_bad", R"({"properties": {"a": {}}, "additionalProperties": {"type": "boolean"}})",
                   R"({"a": 1, "b": 2})", false},
      verdict_case{"additional_alone", R"({"additionalProperties": false})", "{}", true},
      verdict_case{"additional_alone_bad", R"({"additionalProperties": false})", R"({"a": 1})", false},
      verdict_case{"additional_not_object", R"({"additionalProperties": false})", "[1]", true},
      verdict_case{"dependency_array_ok", R"({"dependencies": {"a": ["b"]}})", R"({"a": 1, "b": 2})", true},
      verdict_case{"dependency_array_missing", R"({"dependencies": {"a": ["b"]}})", R"({"a": 1})", false},
      verdict_case{"dependency_array_untriggered", R"({"dependencies": {"a": ["b"]}})", R"({"c": 1})", true},
      verdict_case{"dependency_empty_array", R"({"dependencies": {"a": []}})", R"({"a": 1})", true},
      verdict_case{"dependency_schema_ok", R"({"dependencies": {"a": {"required": ["c"]}}})", R"({"a": 1, "c": 2})",
                   true},
      verdict_case{"dependency_schema_bad", R"({"dependencies": {"a": {"required": ["c"]}}})", R"({"a": 1})", false},
      verdict_case{"dependency_schema_untriggered", R"({"dependencies": {"a": false}})", R"({"b": 1})", true},
      verdict_case{"dependency_schema_false", R"({"dependencies": {"a": false}})", R"({"a": 1})", false},
      verdict_case{"property_names_ok", R"({"propertyNames": {"maxLength": 3}})", R"({"abc": 1, "d": 2})", true},
      verdict_case{"property_names_bad", R"({"propertyNames": {"maxLength": 3}})", R"({"abcd": 1})", false},
      verdict_case{"property_names_pattern", R"({"propertyNames": {"pattern": "^[a-z]+$"}})", R"({"aB": 1})", false},
      verdict_case{"property_names_false_empty", R"({"propertyNames": false})", "{}", true},
      verdict_case{"property_names_false", R"({"propertyNames": false})", R"({"a": 1})", false},
      verdict_case{"property_names_ref",
                   R"({"definitions": {"n": {"enum": ["a", "b"]}}, "propertyNames": {"$ref": "#/definitions/n"}})",
                   R"({"a": 1, "b": 2})", true},
      verdict_case{"property_names_recursive",
                   R"({"properties": {"o": {"$ref": "#"}}, "propertyNames": {"minLength": 1}})",
                   R"({"o": {"o": {"": 1}}})", false}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: Malformed object keywords.
   * Act: Compile.
   * Expect: invalid_schema (keyword_shape, or invalid_pattern for a bad patternProperties key).
   */
  TEST(P051_ObjectShape, RejectsMalformedKeywords)
  {
    for (const char* schema :
         {R"({"minProperties": -1})", R"({"maxProperties": "2"})", R"({"required": "a"})",
          R"({"required": ["a", "a"]})", R"({"required": [1]})", R"({"dependencies": {"a": [1]}})",
          R"({"dependencies": {"a": 3}})", R"({"dependencies": []})", R"({"additionalProperties": 1})",
          R"({"propertyNames": []})", R"({"patternProperties": []})"})
    {
      const auto outcome = run_schema(schema, "{}");
      EXPECT_EQ(outcome.reason, std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape}) << schema;
    }
    EXPECT_EQ(run_schema(R"({"patternProperties": {"(": {}}})", "{}").reason,
              std::optional<invalid_schema_reason>{invalid_schema_reason::invalid_pattern});
  }

  /*
   * Arrange: Object failures of several keywords at once.
   * Act: Validate collect-all.
   * Expect: propertyNames is reported once for the object, not per name; other keywords once each.
   */
  TEST(P051_ObjectShape, ReportsObjectKeywords)
  {
    const auto names = run_schema(R"({"propertyNames": {"maxLength": 1}})", R"({"ab": 1, "cd": 2})");
    ASSERT_FALSE(names.error.has_value());
    EXPECT_EQ(names.keywords, std::vector<schema_keyword>{schema_keyword::property_names});

    const auto several = run_schema(
      R"({"required": ["z"], "maxProperties": 1, "dependencies": {"a": ["q"]}, "additionalProperties": false})",
      R"({"a": 1, "b": 2})");
    ASSERT_FALSE(several.error.has_value());
    const std::vector<schema_keyword> expected{schema_keyword::max_properties, schema_keyword::required,
                                               schema_keyword::dependencies, schema_keyword::false_schema,
                                               schema_keyword::false_schema};
    EXPECT_EQ(several.keywords, expected);
  }

  /*
   * Arrange: Many members matched against patterns with a small regex budget.
   * Act: Validate.
   * Expect: resource_limit (max_regex_steps) instead of unbounded matching (J5-065).
   */
  TEST(P051_ObjectShape, ChargesPatternPropertiesToTheRegexBudget)
  {
    std::string object = "{";
    for (int index = 0; index < 50; ++index)
    {
      object += "\"key" + std::to_string(index) + "\": 1" + ((index == 49) ? "" : ", ");
    }
    object += "}";
    validation_budget small{};
    small.max_regex_steps = 20U;
    const auto limited = run_schema(R"({"patternProperties": {"^key[0-9]+$": {}}})", object, small);
    EXPECT_EQ(limited.error, std::optional<schema_error_code>{schema_error_code::resource_limit});
    EXPECT_EQ(schema_verdict(R"({"patternProperties": {"^key[0-9]+$": {}}})", object), std::optional<bool>{true});
  }
} // namespace
