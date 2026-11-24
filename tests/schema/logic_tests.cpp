#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_run.hpp"
#include "schema_test_storage.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::run_schema;
  using adaptive_json5_test::schema_outcome;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief One logic case: schema, instance and the Draft-07 verdict.
   * @par Requirements
   * P052; J5-057.
   */
  struct logic_case
  {
      const char* name;
      const char* schema;
      const char* instance;
      bool valid;
  };

  /* ===================================== Test_P052_Logic ===================================== */
  class P052_Logic : public testing::TestWithParam<logic_case>
  {
  };

  /*
   * Arrange: allOf/anyOf/oneOf/not and if/then/else with zero, one and several matching branches,
   *          nested conditions and combinations.
   * Act: Validate in collect-all and in fail-fast mode.
   * Expect: The Draft-07 verdict in both modes (J5-057).
   */
  TEST_P(P052_Logic, MatchesDraft07InBothModes)
  {
    const logic_case& item = GetParam();
    const schema_outcome all = run_schema(item.schema, item.instance);
    const schema_outcome fast = run_schema(item.schema, item.instance, validation_budget{}, evaluation_mode::fail_fast);
    ASSERT_FALSE(all.error.has_value()) << item.schema;
    ASSERT_FALSE(fast.error.has_value()) << item.schema;
    EXPECT_EQ(all.valid, item.valid) << item.schema << " / " << item.instance;
    EXPECT_EQ(fast.valid, item.valid) << item.schema << " / " << item.instance;
    EXPECT_EQ(all.keywords.empty(), item.valid) << item.schema;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P052_Logic,
    testing::Values(
      // allOf: every branch must pass.
      logic_case{"AllOfAllPass", R"({"allOf": [{"type": "integer"}, {"minimum": 1}]})", "2", true},
      logic_case{"AllOfOneFails", R"({"allOf": [{"type": "integer"}, {"minimum": 3}]})", "2", false},
      logic_case{"AllOfAllFail", R"({"allOf": [{"type": "string"}, {"minimum": 3}]})", "2", false},
      logic_case{"AllOfSingle", R"({"allOf": [true]})", "null", true},
      // anyOf: at least one branch.
      logic_case{"AnyOfZero", R"({"anyOf": [{"type": "string"}, {"minimum": 3}]})", "2", false},
      logic_case{"AnyOfOne", R"({"anyOf": [{"type": "string"}, {"minimum": 1}]})", "2", true},
      logic_case{"AnyOfMultiple", R"({"anyOf": [{"type": "integer"}, {"minimum": 1}]})", "2", true},
      logic_case{"AnyOfLastOnly", R"({"anyOf": [false, false, true]})", "2", true},
      // oneOf: exactly one branch.
      logic_case{"OneOfZero", R"({"oneOf": [{"type": "string"}, {"minimum": 3}]})", "2", false},
      logic_case{"OneOfOne", R"({"oneOf": [{"type": "string"}, {"minimum": 1}]})", "2", true},
      logic_case{"OneOfTwo", R"({"oneOf": [{"type": "integer"}, {"minimum": 1}]})", "2", false},
      logic_case{"OneOfThree", R"({"oneOf": [true, true, true]})", "2", false},
      logic_case{"OneOfLastOnly", R"({"oneOf": [false, false, true]})", "2", true},
      // not.
      logic_case{"NotFalse", R"({"not": false})", "1", true}, logic_case{"NotTrue", R"({"not": true})", "1", false},
      logic_case{"NotNested", R"({"not": {"not": {"type": "integer"}}})", "1", true},
      logic_case{"NotOfAnyOf", R"({"not": {"anyOf": [{"type": "string"}, {"minimum": 5}]}})", "1", true},
      // if/then/else.
      logic_case{"IfThenTaken", R"({"if": {"type": "integer"}, "then": {"minimum": 5}})", "3", false},
      logic_case{"IfThenPasses", R"({"if": {"type": "integer"}, "then": {"minimum": 1}})", "3", true},
      logic_case{"IfElseTaken", R"({"if": {"type": "string"}, "else": {"minimum": 5}})", "3", false},
      logic_case{"IfElsePasses", R"({"if": {"type": "string"}, "else": {"minimum": 1}})", "3", true},
      logic_case{"IfFailsWithoutElse", R"({"if": {"type": "string"}, "then": false})", "3", true},
      logic_case{"IfPassesWithoutThen", R"({"if": true, "else": false})", "3", true},
      logic_case{"ThenWithoutIf", R"({"then": false, "else": false})", "3", true},
      logic_case{"IfAloneNeverFails", R"({"if": false})", "3", true},
      logic_case{"NestedConditions",
                 R"({"if": {"type": "object"}, "then": {"if": {"required": ["a"]}, "then": {"required": ["b"]},
                     "else": {"maxProperties": 0}}, "else": {"type": "integer"}})",
                 R"({"a": 1})", false},
      logic_case{"NestedConditionsPass",
                 R"({"if": {"type": "object"}, "then": {"if": {"required": ["a"]}, "then": {"required": ["b"]},
                     "else": {"maxProperties": 0}}, "else": {"type": "integer"}})",
                 R"({"a": 1, "b": 2})", true},
      logic_case{"ConditionInBranches",
                 R"({"oneOf": [{"if": {"minimum": 10}, "then": false}, {"if": {"minimum": 10}, "else": false}]})", "12",
                 true},
      logic_case{"CombinedKeywords",
                 R"({"allOf": [{"type": "integer"}], "anyOf": [{"minimum": 1}], "oneOf": [{"maximum": 5}, false],
                     "not": {"const": 3}, "if": true, "then": {"multipleOf": 2}})",
                 "4", true},
      logic_case{"CombinedKeywordsNotFails",
                 R"({"allOf": [{"type": "integer"}], "anyOf": [{"minimum": 1}], "oneOf": [{"maximum": 5}, false],
                     "not": {"const": 4}})",
                 "4", false},
      logic_case{"BranchesOverItems", R"({"items": {"anyOf": [{"type": "string"}, {"oneOf": [{"minimum": 0}]}]}})",
                 R"(["a", 1, -1])", false},
      logic_case{
        "ReferencedBranches",
        R"({"definitions": {"pos": {"minimum": 0}}, "oneOf": [{"$ref": "#/definitions/pos"}, {"type": "string"}]})",
        "1", true}),
    [](const testing::TestParamInfo<logic_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* ================================= Test_P052_Diagnostics ================================= */
  /**
   * @brief Return the keywords of the collect-all diagnostics for schema and instance.
   * @par Requirements
   * P052; J5-063; J5-064.
   */
  std::vector<schema_keyword> keywords_of(const char* schema, const char* instance)
  {
    const schema_outcome outcome = run_schema(schema, instance);
    EXPECT_FALSE(outcome.error.has_value()) << schema;
    return outcome.keywords;
  }

  /*
   * Arrange: Failing combinators and conditions.
   * Act: Validate in collect-all mode.
   * Expect: anyOf, oneOf and not report themselves (their branches are suppressed); allOf and the
   *         chosen then/else branch report the branch's own keywords; the if condition reports
   *         nothing (J5-063, J5-064).
   */
  TEST(P052_Diagnostics, ReportTheFailedKeyword)
  {
    using kw = schema_keyword;
    EXPECT_EQ(keywords_of(R"({"anyOf": [{"type": "string"}, {"minimum": 3}]})", "2"), std::vector<kw>{kw::any_of});
    EXPECT_EQ(keywords_of(R"({"oneOf": [true, true]})", "2"), std::vector<kw>{kw::one_of});
    EXPECT_EQ(keywords_of(R"({"oneOf": [false, false]})", "2"), std::vector<kw>{kw::one_of});
    EXPECT_EQ(keywords_of(R"({"not": {"type": "integer"}})", "2"), std::vector<kw>{kw::not_schema});
    EXPECT_EQ(keywords_of(R"({"allOf": [{"type": "string"}, {"minimum": 3}]})", "2"),
              (std::vector<kw>{kw::type, kw::minimum}));
    EXPECT_EQ(keywords_of(R"({"if": {"type": "integer"}, "then": {"minimum": 5}})", "2"), std::vector<kw>{kw::minimum});
    EXPECT_EQ(keywords_of(R"({"if": {"type": "string"}, "else": {"maximum": 1}})", "2"), std::vector<kw>{kw::maximum});
    EXPECT_EQ(keywords_of(R"({"if": {"minimum": 5}})", "2"), std::vector<kw>{});
    // not runs before if/then/else (evaluation order), so its diagnostic comes first.
    EXPECT_EQ(keywords_of(R"({"if": true, "then": {"anyOf": [false]}, "not": true})", "2"),
              (std::vector<kw>{kw::not_schema, kw::any_of}));
  }

  /* =================================== Test_P052_Failures =================================== */
  /**
   * @brief Return the error code of validating with one budget, or nullopt on success.
   * @par Requirements
   * P052; J5-063; J5-065.
   */
  std::optional<schema_error_code> limited(const char* schema, const char* instance, const validation_budget& budget)
  {
    return run_schema(schema, instance, budget).error;
  }

  /*
   * Arrange: Branch-heavy schemas whose verdict would be known before the budget is spent, with
   *          the branch, equality, regex and work budgets each set too low for the branches.
   * Act: Validate.
   * Expect: resource_limit, never a valid or invalid verdict; with the default budget the run
   *         completes (J5-063, J5-065).
   */
  TEST(P052_Failures, BudgetsInsideBranchesAreOperational)
  {
    validation_budget branches{};
    branches.max_branch_evaluations = 2U;
    EXPECT_EQ(limited(R"({"anyOf": [false, false, {"oneOf": [true]}]})", "1", branches),
              std::optional<schema_error_code>{schema_error_code::resource_limit});

    validation_budget equality{};
    equality.max_equality_steps = 2U;
    EXPECT_EQ(limited(R"({"not": {"const": [1, 2, 3, 4]}})", "[1, 2, 3, 4]", equality),
              std::optional<schema_error_code>{schema_error_code::resource_limit});

    validation_budget regex{};
    regex.max_regex_steps = 3U;
    EXPECT_EQ(limited(R"({"if": {"pattern": "a+b"}, "then": false})", R"("aaaaaaaaaaaaaaaa")", regex),
              std::optional<schema_error_code>{schema_error_code::resource_limit});

    validation_budget work{};
    work.max_work_steps = 4U;
    EXPECT_EQ(limited(R"({"oneOf": [{"allOf": [true, true]}, {"not": {"anyOf": [false, true]}}]})", "1", work),
              std::optional<schema_error_code>{schema_error_code::resource_limit});

    EXPECT_EQ(
      limited(R"({"oneOf": [{"allOf": [true, true]}, {"not": {"anyOf": [false, true]}}]})", "1", validation_budget{}),
      std::nullopt);
  }

  /**
   * @brief Cancellation callback that cancels after a number of polls.
   * @par Requirements
   * P052; J5-049.
   */
  struct cancel_after
  {
      std::size_t polls{};
      std::size_t limit{};

      static bool poll(void* context) noexcept
      {
        auto& self = *static_cast<cancel_after*>(context);
        ++self.polls;
        return self.polls > self.limit;
      }
  };

  /**
   * @brief A compiled schema and parsed instance for repeated validation.
   * @par Requirements
   * P052; J5-063.
   */
  struct logic_fixture
  {
      schema_test_storage result_provider;
      schema_test_storage instance_provider;
      std::string instance_text;
      std::optional<compiled_schema> compiled;
      std::optional<accounted_storage> accounting;
      std::optional<parsed_document> instance;

      logic_fixture(const std::string& schema_text, std::string instance_input)
          : instance_text(std::move(instance_input))
      {
        schema_test_storage registry_provider;
        schema_test_storage scratch_provider;
        accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
        auto registry = schema_registry::create(storage, 1U);
        EXPECT_TRUE(registry.value_if()->add({schema_text.data(), schema_text.size()}).has_value());
        compile_options options{};
        options.result_storage = result_provider.callbacks();
        options.scratch_storage = scratch_provider.callbacks();
        auto outcome =
          compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
        EXPECT_TRUE(outcome.has_value()) << schema_text;
        if (outcome.has_value())
        {
          compiled.emplace(std::move(*outcome.value_if()));
        }
        accounting.emplace(instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes);
        auto parsed = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, *accounting);
        EXPECT_TRUE(parsed.has_value()) << instance_text;
        if (parsed.has_value())
        {
          instance.emplace(std::move(*parsed.value_if()));
        }
      }
  };

  constexpr const char* c_nested_schema =
    R"({"items": {"anyOf": [{"oneOf": [{"minimum": 5}, {"type": "string"}]}, {"not": {"type": "integer"}},
        {"if": {"minimum": 0}, "then": {"maximum": 2}, "else": false}]},
        "allOf": [{"if": {"minItems": 3}, "then": {"contains": {"const": 9}}}]})";
  constexpr const char* c_nested_instance = "[1, 7, 2.5, 3, -1]";

  /*
   * Arrange: Nested combinators and conditions over an array, with cancellation after every poll
   *          count.
   * Act: Validate in collect-all mode.
   * Expect: cancelled with nothing allocated until the run completes; the completed run is invalid
   *         (J5-049, J5-063).
   */
  TEST(P052_Failures, CancellationDuringBranches)
  {
    const logic_fixture fixture{c_nested_schema, c_nested_instance};
    ASSERT_TRUE(fixture.compiled.has_value());
    for (std::size_t limit = 0U;; ++limit)
    {
      cancel_after cancel{0U, limit};
      schema_test_storage work;
      schema_test_storage report;
      validation_options options{};
      options.work_storage = work.callbacks();
      options.report_storage = report.callbacks();
      options.mode = evaluation_mode::collect_all;
      options.cancellation = cancellation_callback{&cancel_after::poll, &cancel};
      const auto outcome = validate(*fixture.compiled, fixture.instance->values, options);
      EXPECT_EQ(work.live_bytes, 0U);
      if (outcome.has_value())
      {
        EXPECT_FALSE(outcome.value_if()->valid());
        break;
      }
      ASSERT_EQ(outcome.error_if()->code(), schema_error_code::cancelled) << limit;
      EXPECT_EQ(report.live_bytes, 0U) << limit;
      ASSERT_LT(limit, 5000U);
    }
  }

  /*
   * Arrange: The same nested schema, with work or report allocation n failing.
   * Act: Validate in collect-all mode for n = 1, 2, ... until it succeeds.
   * Expect: allocation_failure without a partial report and with nothing allocated; the
   *         successful run is invalid (J5-046, J5-063).
   */
  TEST(P052_Failures, AllocationFailureDuringBranches)
  {
    const logic_fixture fixture{c_nested_schema, c_nested_instance};
    ASSERT_TRUE(fixture.compiled.has_value());
    for (const bool fail_report : {false, true})
    {
      for (std::size_t fail_on = 1U;; ++fail_on)
      {
        schema_test_storage work;
        schema_test_storage report;
        (fail_report ? report : work).fail_on = fail_on;
        validation_options options{};
        options.work_storage = work.callbacks();
        options.report_storage = report.callbacks();
        options.mode = evaluation_mode::collect_all;
        const auto outcome = validate(*fixture.compiled, fixture.instance->values, options);
        EXPECT_EQ(work.live_bytes, 0U);
        if (outcome.has_value())
        {
          EXPECT_FALSE(outcome.value_if()->valid());
          break;
        }
        ASSERT_EQ(outcome.error_if()->code(), schema_error_code::allocation_failure) << fail_on;
        EXPECT_EQ(report.live_bytes, 0U);
        ASSERT_LT(fail_on, 500U);
      }
    }
  }

  /*
   * Arrange: Every branch budget from 1 upwards for the nested schema.
   * Act: Validate.
   * Expect: resource_limit (max_branch_evaluations) until the budget suffices, then the same
   *         invalid verdict; the budget is never exceeded silently (J5-065).
   */
  TEST(P052_Failures, BranchBudgetSweep)
  {
    const logic_fixture fixture{c_nested_schema, c_nested_instance};
    ASSERT_TRUE(fixture.compiled.has_value());
    for (std::size_t limit = 0U;; ++limit)
    {
      schema_test_storage work;
      schema_test_storage report;
      validation_options options{};
      options.work_storage = work.callbacks();
      options.report_storage = report.callbacks();
      options.budget.max_branch_evaluations = limit;
      const auto outcome = validate(*fixture.compiled, fixture.instance->values, options);
      if (outcome.has_value())
      {
        EXPECT_FALSE(outcome.value_if()->valid());
        EXPECT_GT(limit, 0U);
        break;
      }
      ASSERT_EQ(outcome.error_if()->code(), schema_error_code::resource_limit) << limit;
      ASSERT_EQ(outcome.error_if()->limit_reason(), schema_limit_reason::max_branch_evaluations) << limit;
      ASSERT_LT(limit, 1000U);
    }
  }
} // namespace
