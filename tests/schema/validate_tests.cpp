#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_test_storage.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief A compiled schema, kept after its registry is gone.
   * @par Requirements
   * P046; J5-066.
   */
  struct schema_fixture
  {
      schema_test_storage result_provider;
      std::optional<compiled_schema> compiled;

      explicit schema_fixture(const std::string& text)
      {
        schema_test_storage registry_provider;
        schema_test_storage scratch_provider;
        accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
        auto registry = schema_registry::create(storage, 1U);
        const auto added = registry.value_if()->add({text.data(), text.size()});
        EXPECT_TRUE(added.has_value()) << text;
        compile_options options{};
        options.result_storage = result_provider.callbacks();
        options.scratch_storage = scratch_provider.callbacks();
        auto outcome =
          compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
        EXPECT_TRUE(outcome.has_value()) << text;
        if (outcome.has_value())
        {
          compiled.emplace(std::move(*outcome.value_if()));
        }
      }
  };

  /**
   * @brief A parsed JSON5 instance (duplicates preserved).
   * @par Requirements
   * P046; J5-059.
   */
  struct instance_fixture
  {
      schema_test_storage provider;
      std::string text;
      accounted_storage accounting;
      std::optional<parsed_document> parsed;

      explicit instance_fixture(std::string input)
          : text(std::move(input)), accounting(provider.callbacks(), parse_options{}.limits.max_owned_bytes)
      {
        auto outcome = parse_document({text.data(), text.size()}, parse_options{}, accounting);
        EXPECT_TRUE(outcome.has_value()) << text;
        if (outcome.has_value())
        {
          parsed.emplace(std::move(*outcome.value_if()));
        }
      }
  };

  /**
   * @brief Work and report storage plus options for one validation.
   * @par Requirements
   * P046; J5-065.
   */
  struct run_fixture
  {
      schema_test_storage work_provider;
      schema_test_storage report_provider;
      validation_options options{};

      run_fixture()
      {
        options.work_storage = work_provider.callbacks();
        options.report_storage = report_provider.callbacks();
      }
  };

  /**
   * @brief Validate instance against schema with the given options.
   * @par Requirements
   * P046.
   */
  result<validation_report, schema_error> check(const schema_fixture& schema, const instance_fixture& instance,
                                                run_fixture& run)
  {
    auto outcome = validate(*schema.compiled, instance.parsed->values, run.options);
    EXPECT_EQ(run.work_provider.live_bytes, 0U);
    return outcome;
  }

  /**
   * @brief Return the verdict of schema for instance with default options, or nullopt on error.
   * @par Requirements
   * P046.
   */
  std::optional<bool> verdict(const char* schema_text, const char* instance_text)
  {
    const schema_fixture schema{schema_text};
    const instance_fixture instance{instance_text};
    run_fixture run;
    const auto outcome = check(schema, instance, run);
    if (!outcome.has_value())
    {
      return std::nullopt;
    }
    return outcome.value_if()->valid();
  }

  /* =================================== Test_P046_Applicators ==================================== */
  struct verdict_case
  {
      const char* name;
      const char* schema;
      const char* instance;
      bool valid;
  };

  class P046_Applicators : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: Schemas using boolean schemas, $ref and the P046 applicators.
   * Act: Validate the instance.
   * Expect: The Draft-07 verdict.
   */
  TEST_P(P046_Applicators, MatchesDraft07)
  {
    const auto& fixture = GetParam();
    EXPECT_EQ(verdict(fixture.schema, fixture.instance), std::optional<bool>{fixture.valid})
      << fixture.schema << " / " << fixture.instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P046_Applicators,
    testing::Values(verdict_case{"true_schema", "true", "[1, {}]", true},
                    verdict_case{"false_schema", "false", "null", false},
                    verdict_case{"empty_schema", "{}", "{a: [1]}", true},
                    verdict_case{"all_of_pass", R"({"allOf": [true, {}]})", "1", true},
                    verdict_case{"all_of_fail", R"({"allOf": [true, false]})", "1", false},
                    verdict_case{"any_of_pass", R"({"anyOf": [false, true]})", "1", true},
                    verdict_case{"any_of_fail", R"({"anyOf": [false, false]})", "1", false},
                    verdict_case{"one_of_one", R"({"oneOf": [false, true, false]})", "1", true},
                    verdict_case{"one_of_two", R"({"oneOf": [true, true]})", "1", false},
                    verdict_case{"one_of_none", R"({"oneOf": [false]})", "1", false},
                    verdict_case{"not_true", R"({"not": true})", "1", false},
                    verdict_case{"not_false", R"({"not": false})", "1", true},
                    verdict_case{"if_then_pass", R"({"if": true, "then": true, "else": false})", "1", true},
                    verdict_case{"if_then_fail", R"({"if": true, "then": false})", "1", false},
                    verdict_case{"if_else_fail", R"({"if": false, "else": false})", "1", false},
                    verdict_case{"if_else_absent", R"({"if": false, "then": false})", "1", true},
                    verdict_case{"then_without_if", R"({"then": false})", "1", true},
                    verdict_case{"properties_fail", R"({"properties": {"a": false}})", R"({"a": 1})", false},
                    verdict_case{"properties_other", R"({"properties": {"a": false}})", R"({"b": 1})", true},
                    verdict_case{"properties_non_object", R"({"properties": {"a": false}})", "[1]", true},
                    verdict_case{"properties_escaped_name", R"({"properties": {"é": false}})", R"({"é": 1})", false},
                    verdict_case{"items_single", R"({"items": {"not": true}})", "[]", true},
                    verdict_case{"items_single_fail", R"({"items": false})", "[1]", false},
                    verdict_case{"items_array", R"({"items": [true, false]})", "[1]", true},
                    verdict_case{"items_array_fail", R"({"items": [true, false]})", "[1, 2, 3]", false},
                    verdict_case{"items_non_array", R"({"items": false})", R"({"a": 1})", true},
                    verdict_case{"ref_definition", R"({"definitions": {"f": false}, "$ref": "#/definitions/f"})", "1",
                                 false},
                    verdict_case{"ref_ignores_siblings",
                                 R"({"definitions": {"t": true}, "$ref": "#/definitions/t", "not": true})", "1", true},
                    verdict_case{"nested", R"({"properties": {"a": {"items": {"anyOf": [{"not": true}, true]}}}})",
                                 R"({"a": [1, [2]]})", true},
                    verdict_case{"recursive_valid", R"({"properties": {"next": {"$ref": "#"}, "stop": false}})",
                                 R"({"next": {"next": {"next": {}}}})", true},
                    verdict_case{"recursive_invalid", R"({"properties": {"next": {"$ref": "#"}, "stop": false}})",
                                 R"({"next": {"next": {"stop": 1}}})", false}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* =================================== Test_P046_Diagnostics ==================================== */
  /*
   * Arrange: A failing property nested under allOf, with an escaped property name.
   * Act: Validate fail-fast and format the schema location.
   * Expect: One false_schema diagnostic at the instance value and schema node, with an
   *         RFC 6901-escaped location; the formatter reports the full size when truncated.
   */
  TEST(P046_Diagnostics, LocatesTheFailure)
  {
    const schema_fixture schema{R"({"allOf": [true, {"properties": {"a/b~": false}}]})"};
    const instance_fixture instance{R"({"x": 0, "a/b~": [1]})"};
    run_fixture run;
    const auto outcome = check(schema, instance, run);
    ASSERT_TRUE(outcome.has_value());
    const auto& report = *outcome.value_if();
    EXPECT_FALSE(report.valid());
    EXPECT_FALSE(report.truncated());
    ASSERT_EQ(report.diagnostic_count(), 1U);
    const auto& diagnostic = *report.diagnostic(0U);
    EXPECT_EQ(diagnostic.keyword, schema_keyword::false_schema);
    ASSERT_TRUE(diagnostic.has_instance_span);
    EXPECT_EQ(diagnostic.instance_span.byte_offset, 17U);
    EXPECT_EQ(diagnostic.instance_span.byte_length, 3U);
    EXPECT_TRUE(diagnostic.has_schema_span);
    EXPECT_EQ(diagnostic.schema_resource, 0U);
    EXPECT_EQ(report.diagnostic(1U), nullptr);

    char buffer[64] = {};
    const std::size_t size = format_schema_location(*schema.compiled, diagnostic, buffer, sizeof(buffer));
    EXPECT_EQ(std::string(buffer, size), "#/allOf/1/properties/a~1b~0");
    char small[5] = {'.', '.', '.', '.', '.'};
    EXPECT_EQ(format_schema_location(*schema.compiled, diagnostic, small, 4U), size);
    EXPECT_EQ(std::string(small, 5U), "#/al.");
  }

  /*
   * Arrange: Failing branches inside anyOf, oneOf and not.
   * Act: Validate collect-all.
   * Expect: Only the combinator is reported, never the suppressed branch failures.
   */
  TEST(P046_Diagnostics, ReportsCombinatorsNotTheirBranches)
  {
    const schema_fixture schema{R"({"allOf": [{"anyOf": [false, false]}, {"oneOf": [true, true]}, {"not": {}}]})"};
    const instance_fixture instance{"1"};
    run_fixture run;
    run.options.mode = evaluation_mode::collect_all;
    const auto outcome = check(schema, instance, run);
    ASSERT_TRUE(outcome.has_value());
    const auto& report = *outcome.value_if();
    ASSERT_EQ(report.diagnostic_count(), 3U);
    EXPECT_EQ(report.diagnostic(0U)->keyword, schema_keyword::any_of);
    EXPECT_EQ(report.diagnostic(1U)->keyword, schema_keyword::one_of);
    EXPECT_EQ(report.diagnostic(2U)->keyword, schema_keyword::not_schema);
    char buffer[32] = {};
    const std::size_t size = format_schema_location(*schema.compiled, *report.diagnostic(1U), buffer, sizeof(buffer));
    EXPECT_EQ(std::string(buffer, size), "#/allOf/1");
  }

  /*
   * Arrange: Three failing items.
   * Act: Validate fail-fast, collect-all, and collect-all with max_errors 2.
   * Expect: 1, 3 and 2 (truncated) diagnostics; the verdict is invalid in every mode.
   */
  TEST(P046_Diagnostics, FailFastAndBoundedCollectAll)
  {
    const schema_fixture schema{R"({"items": false})"};
    const instance_fixture instance{"[1, 2, 3]"};
    run_fixture fast;
    run_fixture all;
    all.options.mode = evaluation_mode::collect_all;
    run_fixture bounded;
    bounded.options.mode = evaluation_mode::collect_all;
    bounded.options.budget.max_errors = 2U;
    run_fixture exact;
    exact.options.mode = evaluation_mode::collect_all;
    exact.options.budget.max_errors = 3U;

    const auto first = check(schema, instance, fast);
    const auto every = check(schema, instance, all);
    const auto limited = check(schema, instance, bounded);
    const auto full = check(schema, instance, exact);

    ASSERT_TRUE(first.has_value() && every.has_value() && limited.has_value() && full.has_value());
    EXPECT_EQ(first.value_if()->diagnostic_count(), 1U);
    EXPECT_FALSE(first.value_if()->truncated());
    EXPECT_EQ(every.value_if()->diagnostic_count(), 3U);
    EXPECT_EQ(limited.value_if()->diagnostic_count(), 2U);
    EXPECT_TRUE(limited.value_if()->truncated());
    EXPECT_FALSE(limited.value_if()->valid());
    EXPECT_EQ(full.value_if()->diagnostic_count(), 3U);
    EXPECT_FALSE(full.value_if()->truncated());
    EXPECT_EQ(every.value_if()->diagnostic(2U)->instance_span.byte_offset, 7U);
  }

  /*
   * Arrange: A valid instance.
   * Act: Validate.
   * Expect: Valid, no diagnostics, nothing allocated from report storage.
   */
  TEST(P046_Diagnostics, ValidInstanceNeedsNoReportStorage)
  {
    const schema_fixture schema{R"({"properties": {"a": {"anyOf": [false, true]}}})"};
    const instance_fixture instance{R"({"a": 1})"};
    run_fixture run;
    const auto outcome = check(schema, instance, run);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(outcome.value_if()->valid());
    EXPECT_EQ(outcome.value_if()->diagnostic_count(), 0U);
    EXPECT_EQ(run.report_provider.allocations, 0U);
  }

  /* =================================== Test_P046_Outcomes ==================================== */
  /*
   * Arrange: Instances with a duplicate name, NaN and an unpaired surrogate, against false.
   * Act: Validate.
   * Expect: unsupported_instance before any assertion, never an invalid report (J5-059, J5-063).
   */
  TEST(P046_Outcomes, UnsupportedInstanceComesBeforeAssertions)
  {
    for (const char* text : {R"({"a": 1, "a": 2})", "[NaN]", R"(["\ud800"])"})
    {
      const schema_fixture schema{"false"};
      const instance_fixture instance{text};
      run_fixture run;
      const auto outcome = check(schema, instance, run);
      ASSERT_FALSE(outcome.has_value()) << text;
      EXPECT_EQ(outcome.error_if()->code(), schema_error_code::unsupported_instance) << text;
    }
  }

  /*
   * Arrange: A schema whose allOf refers back to the root for the same instance.
   * Act: Validate.
   * Expect: nonprogressing_reference, not an invalid instance (J5-056).
   */
  TEST(P046_Outcomes, DetectsNonprogressingReference)
  {
    for (const char* text : {R"({"allOf": [{"$ref": "#"}]})",
                             R"({"definitions": {"a": {"anyOf": [{"$ref": "#/definitions/b"}]},
                                                 "b": {"not": {"$ref": "#/definitions/a"}}},
                                 "$ref": "#/definitions/a"})"})
    {
      const schema_fixture schema{text};
      const instance_fixture instance{"[]"};
      run_fixture run;
      const auto outcome = check(schema, instance, run);
      ASSERT_FALSE(outcome.has_value()) << text;
      EXPECT_EQ(outcome.error_if()->code(), schema_error_code::nonprogressing_reference) << text;
    }
  }

  /*
   * Arrange: A recursive schema and an instance nested 100 levels deep.
   * Act: Validate.
   * Expect: Valid; recursion with instance progress is not a loop and needs no native stack.
   */
  TEST(P046_Outcomes, RecursionWithInstanceProgress)
  {
    std::string instance_text = "1";
    for (int level = 0; level < 100; ++level)
    {
      instance_text = "[" + instance_text + "]";
    }
    const schema_fixture schema{R"({"items": {"$ref": "#"}})"};
    const instance_fixture instance{instance_text};
    run_fixture run;
    const auto outcome = check(schema, instance, run);
    ASSERT_TRUE(outcome.has_value());
    EXPECT_TRUE(outcome.value_if()->valid());
  }

  /**
   * @brief Cancel on the n-th poll.
   * @par Requirements
   * P046; J5-049.
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

  /*
   * Arrange: A schema whose evaluation takes many steps, and cancellation at every step count.
   * Act: Validate.
   * Expect: cancelled, with work and report storage released, until the run completes.
   */
  TEST(P046_Outcomes, CancellationReleasesStorage)
  {
    const schema_fixture schema{R"({"items": {"anyOf": [false, {"not": false}]}, "allOf": [false]})"};
    const instance_fixture instance{"[1, 2, 3]"};
    for (std::size_t limit = 0U;; ++limit)
    {
      cancel_after cancel{0U, limit};
      run_fixture run;
      run.options.cancellation = cancellation_callback{&cancel_after::poll, &cancel};
      const auto outcome = check(schema, instance, run);
      if (outcome.has_value())
      {
        EXPECT_FALSE(outcome.value_if()->valid());
        break;
      }
      ASSERT_EQ(outcome.error_if()->code(), schema_error_code::cancelled) << limit;
      EXPECT_EQ(run.report_provider.live_bytes, 0U);
      ASSERT_LT(limit, 1000U);
    }
  }

  /*
   * Arrange: An assertion failure found first, then a report-storage allocation failure.
   * Act: Validate.
   * Expect: allocation_failure; no partial report (ADR 0006 decision 3).
   */
  TEST(P046_Outcomes, OperationalFailureWins)
  {
    const schema_fixture schema{R"({"items": false})"};
    const instance_fixture instance{"[1]"};
    run_fixture run;
    run.report_provider.fail_on = 1U;
    const auto outcome = check(schema, instance, run);
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error_if()->code(), schema_error_code::allocation_failure);
  }

  /*
   * Arrange: A schema and work-storage failures injected at every allocation.
   * Act: Validate with failure n = 1, 2, ... until it succeeds.
   * Expect: Each failure is allocation_failure and leaves nothing allocated.
   */
  TEST(P046_Outcomes, WorkAllocationFailureSweep)
  {
    const schema_fixture schema{R"({"properties": {"a": {"items": {"$ref": "#"}}}, "allOf": [true]})"};
    const instance_fixture instance{R"({"a": [{"a": [{}]}, {}]})"};
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      run_fixture run;
      run.work_provider.fail_on = fail_on;
      const auto outcome = check(schema, instance, run);
      if (outcome.has_value())
      {
        EXPECT_TRUE(outcome.value_if()->valid());
        break;
      }
      ASSERT_EQ(outcome.error_if()->code(), schema_error_code::allocation_failure) << fail_on;
      ASSERT_LT(fail_on, 200U);
    }
  }

  /* =================================== Test_P046_Budgets ==================================== */
  /**
   * @brief Return the limit reason of validating with one budget field changed, or nullopt on success.
   * @par Requirements
   * P046; J5-065.
   */
  std::optional<schema_limit_reason> limited(const char* schema_text, const char* instance_text,
                                             const validation_budget& budget)
  {
    const schema_fixture schema{schema_text};
    const instance_fixture instance{instance_text};
    run_fixture run;
    run.options.budget = budget;
    const auto outcome = check(schema, instance, run);
    if (outcome.has_value())
    {
      return std::nullopt;
    }
    EXPECT_EQ(outcome.error_if()->code(), schema_error_code::resource_limit);
    return outcome.error_if()->limit_reason();
  }

  /*
   * Arrange: Schemas that need exactly N frames deep, N `$ref` steps and N branch evaluations.
   * Act: Validate with each limit at N and N - 1.
   * Expect: Every limit is inclusive and reports its own reason.
   */
  TEST(P046_Budgets, LimitsAreInclusive)
  {
    validation_budget depth{};
    depth.max_depth = 3U; // root, properties/a, its items: 3 frames.
    EXPECT_EQ(limited(R"({"properties": {"a": {"items": true}}})", R"({"a": [1]})", depth), std::nullopt);
    depth.max_depth = 2U;
    EXPECT_EQ(limited(R"({"properties": {"a": {"items": true}}})", R"({"a": [1]})", depth),
              std::optional<schema_limit_reason>{schema_limit_reason::max_depth});

    validation_budget refs{};
    refs.max_reference_steps = 3U;
    EXPECT_EQ(limited(R"({"items": {"$ref": "#"}})", "[[[1]]]", refs), std::nullopt);
    refs.max_reference_steps = 2U;
    EXPECT_EQ(limited(R"({"items": {"$ref": "#"}})", "[[[1]]]", refs),
              std::optional<schema_limit_reason>{schema_limit_reason::max_reference_steps});

    validation_budget branches{};
    branches.max_branch_evaluations = 4U; // allOf 2, anyOf 1 (stops at the first pass), not 1.
    constexpr const char* c_branchy = R"({"allOf": [true, true], "anyOf": [true, true], "not": false})";
    EXPECT_EQ(limited(c_branchy, "1", branches), std::nullopt);
    branches.max_branch_evaluations = 3U;
    EXPECT_EQ(limited(c_branchy, "1", branches),
              std::optional<schema_limit_reason>{schema_limit_reason::max_branch_evaluations});

    validation_budget work{};
    work.max_work_steps = 1U;
    EXPECT_EQ(limited("true", "[1, 2]", work), std::optional<schema_limit_reason>{schema_limit_reason::max_work_steps});
  }

  /*
   * Arrange: One compiled schema and four threads, each with its own work and report storage.
   * Act: Validate the same and different instances concurrently.
   * Expect: Every thread gets the sequential verdict (J5-050, J5-066).
   */
  TEST(P046_Budgets, ConcurrentValidationsShareTheSchema)
  {
    const schema_fixture schema{R"({"properties": {"a": {"items": {"anyOf": [{"$ref": "#"}, true]}}, "b": false}})"};
    const instance_fixture valid_instance{R"({"a": [{"a": [{}]}, 1]})"};
    const instance_fixture invalid_instance{R"({"a": [{"b": 1}], "b": 2})"};
    std::vector<int> verdicts(8U, -1);
    std::vector<std::thread> threads;
    for (std::size_t index = 0U; index < verdicts.size(); ++index)
    {
      threads.emplace_back(
        [&schema, &valid_instance, &invalid_instance, &verdicts, index]()
        {
          schema_test_storage work_provider;
          schema_test_storage report_provider;
          validation_options options{};
          options.work_storage = work_provider.callbacks();
          options.report_storage = report_provider.callbacks();
          const auto& instance = ((index % 2U) == 0U) ? valid_instance : invalid_instance;
          for (int repeat = 0; repeat < 50; ++repeat)
          {
            const auto outcome = validate(*schema.compiled, instance.parsed->values, options);
            verdicts[index] = outcome.has_value() ? (outcome.value_if()->valid() ? 1 : 0) : 2;
          }
        });
    }
    for (auto& thread : threads)
    {
      thread.join();
    }
    for (std::size_t index = 0U; index < verdicts.size(); ++index)
    {
      EXPECT_EQ(verdicts[index], ((index % 2U) == 0U) ? 1 : 0) << index;
    }
  }
} // namespace
