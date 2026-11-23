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
   * @brief Which storage provider receives the injected allocation failure.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-046; J5-051.
   */
  enum class failing_provider
  {
    none,
    result,
    scratch,
    work,
    report
  };

  /**
   * @brief Outcome of one compile-and-validate run with one injected failure.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-046; J5-051.
   */
  struct failure_run
  {
      std::optional<schema_error_code> error{};
      bool valid{};
  };

  /**
   * @brief Compile schema_text and validate instance_text, failing allocation fail_on of one provider.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-046; J5-051.
   * @note Every provider checks on destruction that all its allocations were released.
   */
  failure_run run_failing(const std::string& schema_text, const std::string& instance_text, failing_provider which,
                          std::size_t fail_on)
  {
    failure_run run{};
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    schema_test_storage work_provider;
    schema_test_storage report_provider;
    result_provider.fail_on = (which == failing_provider::result) ? fail_on : 0U;
    scratch_provider.fail_on = (which == failing_provider::scratch) ? fail_on : 0U;
    work_provider.fail_on = (which == failing_provider::work) ? fail_on : 0U;
    report_provider.fail_on = (which == failing_provider::report) ? fail_on : 0U;
    std::optional<compiled_schema> compiled;
    {
      accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
      auto registry = schema_registry::create(storage, 1U);
      const auto added = registry.value_if()->add({schema_text.data(), schema_text.size()});
      EXPECT_TRUE(added.has_value()) << schema_text;
      compile_options options{};
      options.result_storage = result_provider.callbacks();
      options.scratch_storage = scratch_provider.callbacks();
      auto result = compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
      EXPECT_EQ(scratch_provider.live_bytes, 0U);
      if (!result.has_value())
      {
        run.error = result.error_if()->code();
        return run;
      }
      compiled.emplace(std::move(*result.value_if()));
    }
    schema_test_storage instance_provider;
    accounted_storage accounting{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto instance = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, accounting);
    EXPECT_TRUE(instance.has_value()) << instance_text;
    if (!instance.has_value())
    {
      run.error = schema_error_code::internal_error;
      return run;
    }
    validation_options options{};
    options.work_storage = work_provider.callbacks();
    options.report_storage = report_provider.callbacks();
    options.mode = evaluation_mode::collect_all;
    const auto report = validate(*compiled, instance.value_if()->values, options);
    EXPECT_EQ(work_provider.live_bytes, 0U);
    if (!report.has_value())
    {
      run.error = report.error_if()->code();
      return run;
    }
    run.valid = report.value_if()->valid();
    return run;
  }

  /**
   * @brief One sweep case: a schema, an instance and the verdict without failures.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-046; J5-051.
   */
  struct sweep_case
  {
      const char* name;
      const char* schema;
      const char* instance;
      bool valid;
  };

  /**
   * @brief Fail allocation 1, 2, ... of one provider until the run succeeds.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-046; J5-051.
   * @note Each failing run must report allocation_failure; the final run must give the reference verdict.
   */
  void sweep(const sweep_case& item, failing_provider which)
  {
    const failure_run reference = run_failing(item.schema, item.instance, failing_provider::none, 0U);
    ASSERT_FALSE(reference.error.has_value()) << item.schema;
    ASSERT_EQ(reference.valid, item.valid) << item.schema;
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      const failure_run run = run_failing(item.schema, item.instance, which, fail_on);
      if (!run.error.has_value())
      {
        EXPECT_EQ(run.valid, item.valid) << item.schema << " fail_on " << fail_on;
        break;
      }
      ASSERT_EQ(run.error, std::optional<schema_error_code>{schema_error_code::allocation_failure})
        << item.schema << " fail_on " << fail_on;
      ASSERT_LT(fail_on, 2000U) << item.schema;
    }
  }

  /* =============================== Test_P051_AllocationFailures =============================== */
  class P051_AllocationFailures : public testing::TestWithParam<sweep_case>
  {
  };

  /*
   * Arrange: Schemas using every P047-P051 assertion keyword and matching or failing instances.
   * Act: Fail each compile result-storage allocation in turn.
   * Expect: Every failure is allocation_failure, nothing leaks, and the unfailed run keeps its verdict
   *         (J5-046, J5-051).
   */
  TEST_P(P051_AllocationFailures, CompileResultStorage)
  {
    sweep(GetParam(), failing_provider::result);
  }

  /*
   * Arrange: As above.
   * Act: Fail each compile scratch-storage allocation in turn.
   * Expect: Every failure is allocation_failure and scratch storage is fully released (J5-046, J5-051).
   */
  TEST_P(P051_AllocationFailures, CompileScratchStorage)
  {
    sweep(GetParam(), failing_provider::scratch);
  }

  /*
   * Arrange: As above.
   * Act: Fail each validation work-storage allocation in turn.
   * Expect: Every failure is allocation_failure and work storage is fully released (J5-046, J5-051).
   */
  TEST_P(P051_AllocationFailures, ValidationWorkStorage)
  {
    sweep(GetParam(), failing_provider::work);
  }

  /*
   * Arrange: As above.
   * Act: Fail each report-storage allocation in turn.
   * Expect: Every failure is allocation_failure and nothing leaks (J5-046, J5-051).
   */
  TEST_P(P051_AllocationFailures, ReportStorage)
  {
    sweep(GetParam(), failing_provider::report);
  }

  INSTANTIATE_TEST_SUITE_P(
    Keywords, P051_AllocationFailures,
    testing::Values(
      sweep_case{"TypeEnumConst",
                 R"({"type": ["object", "integer"], "enum": [{"a": [1, "x", null, true]}, 2.0, "s"],
                     "const": {"a": [1, "x", null, true]}})",
                 R"({"a": [1, "x", null, true]})", true},
      sweep_case{"TypeMismatch", R"({"type": "integer", "enum": [1.5, [1, {"b": false}]], "const": 2})", "1.5",
                 false},
      sweep_case{"Numeric",
                 R"({"minimum": -1e3, "maximum": 1e300, "exclusiveMinimum": 0.5, "exclusiveMaximum": 0x7FFF,
                     "multipleOf": 0.25})",
                 "12.75", true},
      sweep_case{"NumericFails", R"({"minimum": 10, "exclusiveMaximum": 3, "multipleOf": 0.3})", "1.1", false},
      sweep_case{"Strings",
                 R"({"minLength": 2, "maxLength": 40,
                     "pattern": "^(?:[a-z]+|\\d{2,4})\\b[\\s\\S]*?(x|y|z)+$"})",
                 R"("abc 12 xyz")", true},
      sweep_case{"StringClasses", R"({"pattern": "[^\\W_]{1,3}[\\-a-f\\u0041]*\\B.$", "maxLength": 1})",
                 R"("ab-cé")", false},
      sweep_case{"Arrays",
                 R"({"items": [{"type": "integer"}, {"const": "x"}], "additionalItems": {"type": "string"},
                     "minItems": 1, "maxItems": 6, "uniqueItems": true, "contains": {"pattern": "^q"}})",
                 R"([1, "x", "a", "q1"])", true},
      sweep_case{"ArraysFail", R"({"items": {"minimum": 0}, "uniqueItems": true, "contains": false, "maxItems": 1})",
                 R"([{"a": [1]}, {"a": [1]}, -1])", false},
      sweep_case{"Objects",
                 R"({"properties": {"a": {"type": "integer"}}, "patternProperties": {"^b+$": {"maxLength": 3},
                     "c$": true}, "additionalProperties": {"type": "boolean"}, "required": ["a", "bb"],
                     "minProperties": 2, "maxProperties": 6, "propertyNames": {"pattern": "^[a-z]+$"},
                     "dependencies": {"a": ["bb"], "bb": {"required": ["a"]}}})",
                 R"({"a": 1, "bb": "xyz", "abc": 2, "z": false})", true},
      sweep_case{"ObjectsFail",
                 R"({"required": ["q"], "propertyNames": {"maxLength": 1}, "additionalProperties": false,
                     "patternProperties": {"x": {"type": "null"}}, "dependencies": {"x": ["y", "z"]},
                     "maxProperties": 1})",
                 R"({"x": 1, "long": {}})", false},
      sweep_case{"Combined",
                 R"({"definitions": {"n": {"type": "number", "multipleOf": 2}},
                     "properties": {"list": {"items": {"$ref": "#/definitions/n"}, "contains": {"const": 4}}},
                     "anyOf": [{"required": ["list"]}, {"minProperties": 3}],
                     "not": {"propertyNames": {"pattern": "^_"}}})",
                 R"({"list": [2, 4, 6]})", true}),
    [](const testing::TestParamInfo<sweep_case>& info) { return std::string{info.param.name}; });
} // namespace
