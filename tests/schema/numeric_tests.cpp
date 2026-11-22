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

  /* =================================== Test_P048_Bounds ==================================== */
  class P048_Bounds : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: Every bound keyword below, at and above its limit, with equivalent spellings,
   *          huge and close values and non-number instances.
   * Act: Validate.
   * Expect: minimum/maximum are inclusive, exclusiveMinimum/exclusiveMaximum exclusive;
   *         comparisons are exact; bounds do not apply to non-numbers (J5-057, J5-059).
   */
  TEST_P(P048_Bounds, AreExact)
  {
    EXPECT_EQ(schema_verdict(GetParam().schema, GetParam().instance), std::optional<bool>{GetParam().valid})
      << GetParam().schema << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P048_Bounds,
    testing::Values(
      verdict_case{"minimum_below", R"({"minimum": 2})", "1.9", false},
      verdict_case{"minimum_equal", R"({"minimum": 2})", "2.0", true},
      verdict_case{"minimum_above", R"({"minimum": 2})", "3", true},
      verdict_case{"maximum_below", R"({"maximum": 2})", "1", true},
      verdict_case{"maximum_equal", R"({"maximum": 2})", "0x2", true},
      verdict_case{"maximum_above", R"({"maximum": 2})", "2.0000000000000000001", false},
      verdict_case{"exclusive_minimum_below", R"({"exclusiveMinimum": 2})", "1", false},
      verdict_case{"exclusive_minimum_equal", R"({"exclusiveMinimum": 2})", "2e0", false},
      verdict_case{"exclusive_minimum_above", R"({"exclusiveMinimum": 2})", "2.0000000000000000001", true},
      verdict_case{"exclusive_maximum_below", R"({"exclusiveMaximum": 2})", "1.9999999999999999999", true},
      verdict_case{"exclusive_maximum_equal", R"({"exclusiveMaximum": 2})", "20e-1", false},
      verdict_case{"exclusive_maximum_above", R"({"exclusiveMaximum": 2})", "3", false},
      verdict_case{"negative_bounds", R"({"minimum": -5, "maximum": -1})", "-5", true},
      verdict_case{"negative_below", R"({"minimum": -5})", "-5.1", false},
      verdict_case{"negative_zero_minimum", R"({"minimum": 0})", "-0", true},
      verdict_case{"huge_maximum", R"({"maximum": 1e300})", "1e301", false},
      verdict_case{"huge_minimum", R"({"minimum": 100000000000000000000})", "99999999999999999999", false},
      verdict_case{"not_a_number", R"({"minimum": 5, "maximum": 1})", R"("x")", true},
      verdict_case{"fraction_bound", R"({"exclusiveMaximum": 0.3})", "0.30000000000000001", false}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* =================================== Test_P048_MultipleOf ==================================== */
  class P048_MultipleOf : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: Integer, decimal-fraction, exponent, hexadecimal and huge values and divisors.
   * Act: Validate.
   * Expect: Exact divisibility without floating rounding: 0.3 is a multiple of 0.1 (J5-059).
   */
  TEST_P(P048_MultipleOf, IsExact)
  {
    EXPECT_EQ(schema_verdict(GetParam().schema, GetParam().instance), std::optional<bool>{GetParam().valid})
      << GetParam().schema << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P048_MultipleOf,
    testing::Values(verdict_case{"integer_yes", R"({"multipleOf": 3})", "9", true},
                    verdict_case{"integer_no", R"({"multipleOf": 3})", "10", false},
                    verdict_case{"zero", R"({"multipleOf": 7})", "0", true},
                    verdict_case{"negative", R"({"multipleOf": 3})", "-9", true},
                    verdict_case{"tenth_yes", R"({"multipleOf": 0.1})", "0.3", true},
                    verdict_case{"tenth_no", R"({"multipleOf": 0.1})", "0.35", false},
                    verdict_case{"hundredth", R"({"multipleOf": 0.01})", "19.99", true},
                    verdict_case{"fraction_divisor", R"({"multipleOf": 1.5})", "4.5", true},
                    verdict_case{"fraction_divisor_no", R"({"multipleOf": 1.5})", "4", false},
                    verdict_case{"exponent_value", R"({"multipleOf": 5})", "1e3", true},
                    verdict_case{"exponent_divisor", R"({"multipleOf": 2e-2})", "0.06", true},
                    verdict_case{"hex_value", R"({"multipleOf": 4})", "0x10", true},
                    verdict_case{"hex_value_no", R"({"multipleOf": 0.3})", "0xF", true},
                    verdict_case{"hex_value_odd", R"({"multipleOf": 2})", "0xF", false},
                    verdict_case{"huge_value", R"({"multipleOf": 7})", "700000000000000000000000000000000000007", true},
                    verdict_case{"huge_value_no", R"({"multipleOf": 7})", "700000000000000000000000000000000000008",
                                 false},
                    verdict_case{"tiny_divisor", R"({"multipleOf": 1e-30})", "0.000000000000000000000000000003", true},
                    verdict_case{"value_smaller", R"({"multipleOf": 10})", "3", false},
                    verdict_case{"integer_float_divisor", R"({"multipleOf": 0.0001})", "1e-5", false},
                    verdict_case{"not_a_number", R"({"multipleOf": 2})", "true", true}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* =================================== Test_P048_NumericShape ==================================== */
  /*
   * Arrange: Numeric keywords with non-number values and multipleOf values not above zero.
   * Act: Compile.
   * Expect: invalid_schema (keyword_shape).
   */
  TEST(P048_NumericShape, RejectsMalformedKeywords)
  {
    for (const char* schema : {R"({"minimum": "1"})", R"({"maximum": null})", R"({"exclusiveMinimum": true})",
                               R"({"exclusiveMaximum": [1]})", R"({"multipleOf": 0})", R"({"multipleOf": -2})",
                               R"({"multipleOf": 0.0})", R"({"multipleOf": "2"})"})
    {
      const auto outcome = run_schema(schema, "1");
      EXPECT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::invalid_schema}) << schema;
      EXPECT_EQ(outcome.reason, std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape}) << schema;
    }
  }

  /*
   * Arrange: An instance failing four numeric keywords at once.
   * Act: Validate collect-all.
   * Expect: One diagnostic per failed keyword, in keyword order.
   */
  TEST(P048_NumericShape, ReportsEachFailedKeyword)
  {
    const auto outcome =
      run_schema(R"({"multipleOf": 2, "maximum": 1, "exclusiveMaximum": 1, "minimum": 0, "exclusiveMinimum": 5})", "3");
    ASSERT_FALSE(outcome.error.has_value());
    const std::vector<schema_keyword> expected{schema_keyword::multiple_of, schema_keyword::maximum,
                                               schema_keyword::exclusive_maximum, schema_keyword::exclusive_minimum};
    EXPECT_EQ(outcome.keywords, expected);
  }

  /*
   * Arrange: A divisibility check whose scaled numbers need more work than the default limit.
   * Act: Validate.
   * Expect: resource_limit, never a guessed verdict.
   */
  TEST(P048_NumericShape, ReportsExpensiveDivisibilityAsResourceLimit)
  {
    const auto outcome = run_schema(R"({"multipleOf": 1e-999999})", "1e999999");
    EXPECT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::resource_limit});
  }
} // namespace
