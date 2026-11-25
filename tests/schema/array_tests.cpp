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

  /* =================================== Test_P050_Arrays ==================================== */
  class P050_Arrays : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: Empty and tuple arrays, nested schemas, deep duplicates and contains cases.
   * Act: Validate.
   * Expect: Draft-07 items/additionalItems/minItems/maxItems/uniqueItems/contains semantics
   *         (J5-057, J5-061).
   */
  TEST_P(P050_Arrays, MatchDraft07)
  {
    EXPECT_EQ(schema_verdict(GetParam().schema, GetParam().instance), std::optional<bool>{GetParam().valid})
      << GetParam().schema << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P050_Arrays,
    testing::Values(
      verdict_case{"min_items_equal", R"({"minItems": 2})", "[1, 2]", true},
      verdict_case{"min_items_below", R"({"minItems": 2})", "[1]", false},
      verdict_case{"max_items_equal", R"({"maxItems": 2})", "[1, 2]", true},
      verdict_case{"max_items_above", R"({"maxItems": 2})", "[1, 2, 3]", false},
      verdict_case{"empty_min_zero", R"({"minItems": 0, "maxItems": 0})", "[]", true},
      verdict_case{"items_not_array", R"({"minItems": 3})", R"("ab")", true},
      verdict_case{"additional_false", R"({"items": [{}, {}], "additionalItems": false})", "[1, 2]", true},
      verdict_case{"additional_false_extra", R"({"items": [{}, {}], "additionalItems": false})", "[1, 2, 3]", false},
      verdict_case{"additional_schema", R"({"items": [true], "additionalItems": {"type": "string"}})",
                   R"([1, "a", "b"])", true},
      verdict_case{"additional_schema_bad", R"({"items": [true], "additionalItems": {"type": "string"}})",
                   R"([1, "a", 2])", false},
      verdict_case{"additional_ignored_single_items", R"({"items": {}, "additionalItems": false})", "[1, 2]", true},
      verdict_case{"additional_ignored_no_items", R"({"additionalItems": false})", "[1, 2]", true},
      verdict_case{"tuple_shorter_instance", R"({"items": [{"type": "integer"}, {"type": "string"}]})", "[1]", true},
      verdict_case{"tuple_nested", R"({"items": [{"items": {"minimum": 0}}]})", "[[1, -1]]", false},
      verdict_case{"unique_ok", R"({"uniqueItems": true})", R"([1, "1", true, null, [1], {"a": 1}])", true},
      verdict_case{"unique_numbers", R"({"uniqueItems": true})", "[1, 1.0]", false},
      verdict_case{"unique_hex", R"({"uniqueItems": true})", "[16, 0x10]", false},
      verdict_case{"unique_objects_reordered", R"({"uniqueItems": true})",
                   R"([{"a": 1, "b": [2]}, {"b": [2.0], "a": 1}])", false},
      verdict_case{"unique_arrays_order", R"({"uniqueItems": true})", "[[1, 2], [2, 1]]", true},
      verdict_case{"unique_bool_number", R"({"uniqueItems": true})", "[1, true, 0, false]", true},
      verdict_case{"unique_false_allows", R"({"uniqueItems": false})", "[1, 1]", true},
      verdict_case{"unique_empty", R"({"uniqueItems": true})", "[]", true},
      verdict_case{"unique_deep", R"({"uniqueItems": true})",
                   R"([{"a": [{"b": [1, {"c": null}]}]}, {"a": [{"b": [1e0, {"c": null}]}]}])", false},
      verdict_case{"contains_match", R"({"contains": {"minimum": 5}})", "[1, 6, 2]", true},
      verdict_case{"contains_none", R"({"contains": {"minimum": 5}})", "[1, 2]", false},
      verdict_case{"contains_empty", R"({"contains": {}})", "[]", false},
      verdict_case{"contains_false", R"({"contains": false})", "[1]", false},
      verdict_case{"contains_not_array", R"({"contains": false})", R"({"a": 1})", true},
      verdict_case{"contains_with_items", R"({"items": {"type": "integer"}, "contains": {"const": 3}})", "[1, 3]",
                   true}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: Malformed array keywords.
   * Act: Compile.
   * Expect: invalid_schema (keyword_shape).
   */
  TEST(P050_ArrayShape, RejectsMalformedKeywords)
  {
    for (const char* schema : {R"({"minItems": -1})", R"({"maxItems": 2.5})", R"({"uniqueItems": 1})",
                               R"({"additionalItems": 3})", R"({"contains": "x"})"})
    {
      const auto outcome = run_schema(schema, "[]");
      EXPECT_EQ(outcome.reason, std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape}) << schema;
    }
  }

  /*
   * Arrange: contains whose element checks fail, and failing array assertions together.
   * Act: Validate collect-all.
   * Expect: Only contains itself is reported, not the element failures; assertion keywords are
   *         reported each once.
   */
  TEST(P050_ArrayShape, ReportsArrayKeywords)
  {
    const auto contains = run_schema(R"({"contains": {"type": "string"}})", "[1, 2, 3]");
    ASSERT_FALSE(contains.error.has_value());
    EXPECT_EQ(contains.keywords, std::vector<schema_keyword>{schema_keyword::contains});

    const auto several = run_schema(R"({"maxItems": 1, "uniqueItems": true, "minItems": 5})", "[1, 1]");
    ASSERT_FALSE(several.error.has_value());
    const std::vector<schema_keyword> expected{schema_keyword::max_items, schema_keyword::min_items,
                                               schema_keyword::unique_items};
    EXPECT_EQ(several.keywords, expected);
  }

  /*
   * Arrange: uniqueItems over many equal-looking values and a small equality budget.
   * Act: Validate.
   * Expect: resource_limit (max_equality_steps) instead of an unbounded comparison (J5-065).
   */
  TEST(P050_ArrayShape, ChargesUniqueItemsToTheEqualityBudget)
  {
    std::string array = "[";
    for (int index = 0; index < 40; ++index)
    {
      array += "[0, 0, " + std::to_string(index) + "]" + ((index == 39) ? "" : ", ");
    }
    array += "]";
    validation_budget small{};
    small.max_equality_steps = 100U;
    const auto limited = run_schema(R"({"uniqueItems": true})", array, small);
    EXPECT_EQ(limited.error, std::optional<schema_error_code>{schema_error_code::resource_limit});
    EXPECT_EQ(schema_verdict(R"({"uniqueItems": true})", array), std::optional<bool>{true});
  }
} // namespace
