#include "../../../tests/schema/schema_test_storage.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema/schema_budget.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/validation_options.hpp"
#include "schema/schema_work.hpp"
#include "schema/value_equality.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief A parsed JSON5 array whose first two elements are compared.
   * @par Requirements
   * P045; J5-061.
   */
  struct pair_document
  {
      schema_test_storage provider;
      std::string text;
      accounted_storage accounting;
      std::optional<parsed_document> parsed;

      explicit pair_document(const char* input)
          : text(input), accounting(provider.callbacks(), parse_options{}.limits.max_owned_bytes)
      {
        auto outcome = parse_document({text.data(), text.size()}, parse_options{}, accounting);
        if (outcome.has_value())
        {
          parsed.emplace(std::move(*outcome.value_if()));
        }
      }

      value_view element(std::size_t position) const
      {
        return *parsed->values.root().value_if()->element_at(position).value_if();
      }
  };

  /**
   * @brief Compare elements 0 and 1 of input with a fresh budget.
   * @par Requirements
   * P045; J5-061.
   */
  result<bool, schema_error> compare(const char* input, const validation_budget& budget = {}, std::size_t fail_on = 0U)
  {
    const pair_document document{input};
    EXPECT_TRUE(document.parsed.has_value()) << input;
    schema_test_storage scratch;
    scratch.fail_on = fail_on;
    schema_work work{budget, cancellation_callback{}};
    return values_equal(document.element(0U), document.element(1U), work, scratch.callbacks());
  }

  /* =================================== Test_P045_ExactEquality ==================================== */
  struct equality_case
  {
      const char* name;
      const char* input; // JSON5 array; elements 0 and 1 are compared.
      bool equal;
  };

  class P045_ExactEquality : public testing::TestWithParam<equality_case>
  {
  };

  /*
   * Arrange: Pairs of values with equivalent and different spellings, kinds and orders.
   * Act: Compare them in both directions.
   * Expect: Exact numeric equivalence, no bool/number confusion, ordered arrays and
   *         order-free objects (ADR 0006 decision 11); the verdict is symmetric.
   */
  TEST_P(P045_ExactEquality, ComparesByValue)
  {
    const auto& fixture = GetParam();
    const auto forward = compare(fixture.input);
    ASSERT_TRUE(forward.has_value()) << fixture.input;
    EXPECT_EQ(*forward.value_if(), fixture.equal) << fixture.input;

    const pair_document document{fixture.input};
    schema_test_storage scratch;
    schema_work work{validation_budget{}, cancellation_callback{}};
    const auto backward = values_equal(document.element(1U), document.element(0U), work, scratch.callbacks());
    ASSERT_TRUE(backward.has_value());
    EXPECT_EQ(*backward.value_if(), fixture.equal) << fixture.input;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P045_ExactEquality,
    testing::Values(
      equality_case{"integer_decimal", "[1, 1.0]", true}, equality_case{"integer_exponent", "[1, 1e0]", true},
      equality_case{"integer_hex", "[1, 0x1]", true}, equality_case{"fraction_exponent", "[0.1, 1e-1]", true},
      equality_case{"huge_integer", "[100000000000000000000, 1e20]", true},
      equality_case{"huge_neighbour", "[100000000000000000001, 1e20]", false},
      equality_case{"close_fraction", "[0.30000000000000001, 0.3]", false},
      equality_case{"negative_zero", "[-0, 0]", true}, equality_case{"sign", "[1, -1]", false},
      equality_case{"bool_vs_number", "[true, 1]", false}, equality_case{"false_vs_zero", "[false, 0]", false},
      equality_case{"null_vs_false", "[null, false]", false}, equality_case{"null_null", "[null, null]", true},
      equality_case{"strings", R"(["aé", "aé"])", true}, equality_case{"string_case", R"(["a", "A"])", false},
      equality_case{"string_vs_number", R"(["1", 1])", false},
      equality_case{"arrays_ordered", "[[1, 2], [1, 2]]", true},
      equality_case{"arrays_reordered", "[[1, 2], [2, 1]]", false},
      equality_case{"arrays_length", "[[1], [1, 1]]", false},
      equality_case{"objects_reordered", R"([{"a": 1, "b": [true]}, {b: [true], "a": 1.0}])", true},
      equality_case{"objects_other_name", R"([{"a": 1}, {"b": 1}])", false},
      equality_case{"objects_size", R"([{"a": 1}, {"a": 1, "b": 2}])", false},
      equality_case{"nested_difference", R"([{"a": [{"b": [1, 2, {"c": null}]}]}, {"a": [{"b": [1, 2, {"c": 0}]}]}])",
                    false},
      equality_case{"nested_equal", R"([{"a": [{"b": [1, 2, {"c": null}]}]}, {"a": [{"b": [1.0, 2, {"c": null}]}]}])",
                    true},
      equality_case{"empty_containers", "[[], {}]", false}),
    [](const testing::TestParamInfo<equality_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* =================================== Test_P045_EqualityBudget ==================================== */
  /*
   * Arrange: Two equal arrays of three numbers (four visited pairs).
   * Act: Compare with max_equality_steps 4 and 3.
   * Expect: 4 passes; 3 returns resource_limit (max_equality_steps).
   */
  TEST(P045_EqualityBudget, EqualityStepLimitIsInclusive)
  {
    validation_budget exact{};
    exact.max_equality_steps = 4U;
    validation_budget short_budget{};
    short_budget.max_equality_steps = 3U;
    const auto passed = compare("[[1, 2, 3], [1, 2, 3]]", exact);
    const auto failed = compare("[[1, 2, 3], [1, 2, 3]]", short_budget);
    ASSERT_TRUE(passed.has_value());
    EXPECT_TRUE(*passed.value_if());
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->limit_reason(), schema_limit_reason::max_equality_steps);
  }

  /*
   * Arrange: Two objects; name comparisons also consume work steps.
   * Act: Compare with a work-step cap swept upward.
   * Expect: resource_limit (max_work_steps) until enough steps are allowed.
   */
  TEST(P045_EqualityBudget, ChargesWorkSteps)
  {
    std::size_t limit = 0U;
    for (;; ++limit)
    {
      validation_budget budget{};
      budget.max_work_steps = limit;
      const auto outcome = compare(R"([{"a": 1, "b": 2, "c": 3}, {"c": 3, "b": 2, "a": 1}])", budget);
      if (outcome.has_value())
      {
        EXPECT_TRUE(*outcome.value_if());
        break;
      }
      ASSERT_EQ(outcome.error_if()->limit_reason(), schema_limit_reason::max_work_steps);
      ASSERT_LT(limit, 100U);
    }
    // 4 visited pairs plus 1 + 2 + 3 name comparisons.
    EXPECT_EQ(limit, 10U);
  }

  /*
   * Arrange: A comparison whose explicit stack needs storage.
   * Act: Fail the first scratch allocation.
   * Expect: allocation_failure; nothing remains allocated.
   */
  TEST(P045_EqualityBudget, ReportsScratchAllocationFailure)
  {
    const auto outcome = compare("[[1], [1]]", validation_budget{}, 1U);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error_if()->code(), schema_error_code::allocation_failure);
  }

  /*
   * Arrange: A deeply nested pair, deeper than any recursion-friendly depth.
   * Act: Compare.
   * Expect: Equal, using the explicit stack rather than recursion.
   */
  TEST(P045_EqualityBudget, HandlesDeepNestingIteratively)
  {
    std::string nested;
    for (int level = 0; level < 400; ++level)
    {
      nested += "[";
    }
    nested += "1";
    for (int level = 0; level < 400; ++level)
    {
      nested += "]";
    }
    const std::string input = "[" + nested + ", " + nested + "]";
    const auto outcome = compare(input.c_str());
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(*outcome.value_if());
  }
} // namespace
