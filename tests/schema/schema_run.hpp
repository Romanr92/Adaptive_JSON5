#ifndef ADAPTIVE_JSON5_TEST_SCHEMA_RUN_HPP_
#define ADAPTIVE_JSON5_TEST_SCHEMA_RUN_HPP_

#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_test_storage.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace adaptive_json5_test
{
  /**
   * @brief Outcome of compiling one schema text and validating one JSON5 instance with it.
   * @par Requirements
   * P047; J5-057; J5-063.
   * @note error holds the compile or validate failure; otherwise valid and keywords (the failed
   * keyword of every diagnostic, in collect-all order) describe the report.
   */
  struct schema_outcome
  {
      std::optional<adaptive_json5::schema_error_code> error{};
      std::optional<adaptive_json5::invalid_schema_reason> reason{};
      bool valid{};
      std::vector<adaptive_json5::schema_keyword> keywords{};
  };

  /**
   * @brief Compile schema_text and validate instance_text in collect-all mode.
   * @par Requirements
   * P047; J5-057.
   */
  inline schema_outcome run_schema(const std::string& schema_text, const std::string& instance_text,
                                   const adaptive_json5::validation_budget& budget = {})
  {
    using namespace adaptive_json5;
    schema_outcome outcome{};
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    std::optional<compiled_schema> compiled;
    {
      accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
      auto registry = schema_registry::create(storage, 1U);
      const auto added = registry.value_if()->add({schema_text.data(), schema_text.size()});
      if (!added.has_value())
      {
        outcome.error = added.error_if()->code();
        outcome.reason = added.error_if()->invalid_schema_reason();
        return outcome;
      }
      compile_options options{};
      options.result_storage = result_provider.callbacks();
      options.scratch_storage = scratch_provider.callbacks();
      auto result = compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
      if (!result.has_value())
      {
        outcome.error = result.error_if()->code();
        outcome.reason = result.error_if()->invalid_schema_reason();
        return outcome;
      }
      compiled.emplace(std::move(*result.value_if()));
    }
    schema_test_storage instance_provider;
    accounted_storage accounting{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto instance = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, accounting);
    EXPECT_TRUE(instance.has_value()) << instance_text;
    if (!instance.has_value())
    {
      return outcome;
    }
    schema_test_storage work;
    schema_test_storage report_storage;
    validation_options run{};
    run.work_storage = work.callbacks();
    run.report_storage = report_storage.callbacks();
    run.mode = evaluation_mode::collect_all;
    run.budget = budget;
    const auto report = validate(*compiled, instance.value_if()->values, run);
    EXPECT_EQ(work.live_bytes, 0U);
    if (!report.has_value())
    {
      outcome.error = report.error_if()->code();
      return outcome;
    }
    outcome.valid = report.value_if()->valid();
    for (std::size_t index = 0U; index < report.value_if()->diagnostic_count(); ++index)
    {
      outcome.keywords.push_back(report.value_if()->diagnostic(index)->keyword);
    }
    return outcome;
  }

  /**
   * @brief Verdict of schema_text for instance_text, or nullopt on a compile/validate error.
   * @par Requirements
   * P047; J5-057.
   */
  inline std::optional<bool> schema_verdict(const std::string& schema_text, const std::string& instance_text)
  {
    const schema_outcome outcome = run_schema(schema_text, instance_text);
    if (outcome.error.has_value())
    {
      return std::nullopt;
    }
    return outcome.valid;
  }
} // namespace adaptive_json5_test

#endif /* ADAPTIVE_JSON5_TEST_SCHEMA_RUN_HPP_ */
