#include "../../../tests/schema/schema_test_storage.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/assertions.hpp"
#include "schema/exact_number.hpp"
#include "schema/meta_schema.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_values.hpp"
#include "schema/schema_work.hpp"
#include "value/document_access.hpp"

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
   * @brief A parsed JSON5 document kept alive with its storage.
   * @par Requirements
   * P047; P048; P051; J5-057.
   */
  struct parsed_text
  {
      schema_test_storage provider;
      std::string text;
      accounted_storage accounting;
      std::optional<parsed_document> parsed;

      explicit parsed_text(const char* input)
          : text(input), accounting(provider.callbacks(), parse_options{}.limits.max_owned_bytes)
      {
        auto outcome = parse_document({text.data(), text.size()}, parse_options{}, accounting);
        EXPECT_TRUE(outcome.has_value()) << text;
        if (outcome.has_value())
        {
          parsed.emplace(std::move(*outcome.value_if()));
        }
      }

      value_view root() const
      {
        return *parsed->values.root().value_if();
      }
  };

  /**
   * @brief A compiled schema kept alive with its storage.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-057.
   */
  struct compiled_text
  {
      schema_test_storage result_provider;
      std::optional<compiled_schema> compiled;

      explicit compiled_text(const std::string& text)
      {
        schema_test_storage registry_provider;
        schema_test_storage scratch_provider;
        accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
        auto registry = schema_registry::create(storage, 1U);
        EXPECT_TRUE(registry.value_if()->add({text.data(), text.size()}).has_value()) << text;
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

  /* ================================ Test_P047_InvalidViews ================================ */
  /*
   * Arrange: Default-constructed (invalid) value views.
   * Act: Parse a type keyword, copy a value, copy dependency arrays and check a name list.
   * Expect: Each access failure is an error, never a result (J5-057).
   */
  TEST(P047_InvalidViews, SchemaValueHelpersReportAccessFailures)
  {
    schema_test_storage storage;
    document target = document_access::create(storage.callbacks(), 64U);
    std::size_t steps = 0U;
    EXPECT_FALSE(parse_type_keyword(value_view{}).has_value());
    EXPECT_FALSE(copy_value(value_view{}, target, storage.callbacks(), steps, 1000U).has_value());
    EXPECT_FALSE(copy_dependency_arrays(value_view{}, target, storage.callbacks(), steps, 1000U).has_value());
    EXPECT_FALSE(check_name_list(value_view{}).has_value());
  }

  /*
   * Arrange: A nested value and a copy step limit below its size.
   * Act: Copy it.
   * Expect: The copy stops with resource_limit; with enough steps it succeeds (J5-061, J5-065).
   */
  TEST(P047_InvalidViews, CopyHonoursTheStepLimit)
  {
    const parsed_text source{R"({"a": [1, {"b": "c"}, null, true], "d": 2.5})"};
    schema_test_storage storage;
    document target = document_access::create(storage.callbacks(), 64U);
    std::size_t steps = 0U;
    const auto limited = copy_value(source.root(), target, storage.callbacks(), steps, 2U);
    ASSERT_FALSE(limited.has_value());
    EXPECT_EQ(limited.error_if()->code(), schema_error_code::resource_limit);
    steps = 0U;
    EXPECT_TRUE(copy_value(source.root(), target, storage.callbacks(), steps, 1000U).has_value());
  }

  /*
   * Arrange: Copies into a target whose storage fails at each allocation in turn.
   * Act: Copy a nested value.
   * Expect: Every failure is allocation_failure and nothing leaks (J5-046, J5-061).
   */
  TEST(P047_InvalidViews, CopyAllocationFailureSweep)
  {
    const parsed_text source{R"({"a": [1, {"b": "c"}, null, true], "d": 2.5, "e": {}})"};
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      schema_test_storage storage;
      storage.fail_on = fail_on;
      document target = document_access::create(storage.callbacks(), 64U);
      std::size_t steps = 0U;
      const auto copied = copy_value(source.root(), target, storage.callbacks(), steps, 1000U);
      if (copied.has_value())
      {
        break;
      }
      ASSERT_EQ(copied.error_if()->code(), schema_error_code::allocation_failure) << fail_on;
      ASSERT_LT(fail_on, 200U);
    }
  }

  /*
   * Arrange: Non-finite numbers (JSON5 Infinity and NaN).
   * Act: Ask is_positive and is_multiple_of.
   * Expect: Both refuse non-finite operands with an error instead of a verdict (J5-059).
   */
  TEST(P048_ExactNumbers, RejectNonFiniteOperands)
  {
    const parsed_text values{"[Infinity, NaN, 2]"};
    const auto number = [&values](std::size_t index)
    {
      return *values.root().element_at(index).value_if()->as_number().value_if();
    };
    schema_test_storage scratch;
    EXPECT_FALSE(is_positive(*number(0U)).has_value());
    EXPECT_FALSE(is_positive(*number(1U)).has_value());
    EXPECT_FALSE(is_multiple_of(*number(0U), *number(2U), scratch.callbacks()).has_value());
    EXPECT_FALSE(is_multiple_of(*number(2U), *number(1U), scratch.callbacks()).has_value());
    const auto positive = is_positive(*number(2U));
    ASSERT_TRUE(positive.has_value());
    EXPECT_TRUE(*positive.value_if());
  }

  /*
   * Arrange: Compiled schemas using each assertion keyword family and an invalid instance view.
   * Act: Run check_assertions on the root node.
   * Expect: The access failure is an error, never a keyword mask (J5-057).
   */
  TEST(P051_InvalidViews, AssertionsReportAccessFailures)
  {
    const char* const schemas[] = {R"({"type": "integer"})",
                                   R"({"enum": [1, 2]})",
                                   R"({"const": {"a": 1}})",
                                   R"({"minimum": 1, "multipleOf": 2})",
                                   R"({"minLength": 1, "pattern": "a"})",
                                   R"({"minItems": 1, "uniqueItems": true})",
                                   R"({"minProperties": 1, "required": ["a"], "dependencies": {"a": ["b"]}})"};
    for (const char* const text : schemas)
    {
      const compiled_text schema{text};
      ASSERT_TRUE(schema.compiled.has_value());
      const schema_node& node =
        compiled_schema_access::node(*schema.compiled, compiled_schema_access::root(*schema.compiled));
      schema_test_storage scratch;
      schema_work work{validation_budget{}, cancellation_callback{}};
      EXPECT_FALSE(check_assertions(*schema.compiled, node, value_view{}, work, scratch.callbacks()).has_value())
        << text;
    }
  }

  /* =============================== Test_P054_EmbeddedMetaSchema =============================== */
  /*
   * Arrange: The embedded Draft-07 meta-schema text.
   * Act: Check its pinned size and identity, register it as both schema and instance, compile it
   *      and validate it against itself.
   * Expect: The byte-identical upstream copy (4,979 bytes, $id of Draft-07) compiles and is valid
   *         against itself (J5-053, J5-054).
   */
  TEST(P054_EmbeddedMetaSchema, IsPinnedAndValidatesItself)
  {
    const std::string_view text = draft07_meta_schema();
    EXPECT_EQ(text.size(), 4979U);
    EXPECT_NE(text.find(R"("$id": "http://json-schema.org/draft-07/schema#")"), std::string_view::npos);
    const compiled_text meta{std::string{text}};
    ASSERT_TRUE(meta.compiled.has_value());
    const parsed_text instance{std::string{text}.c_str()};
    schema_test_storage work;
    schema_test_storage report;
    validation_options options{};
    options.work_storage = work.callbacks();
    options.report_storage = report.callbacks();
    const auto verdict = validate(*meta.compiled, instance.parsed->values, options);
    ASSERT_TRUE(verdict.has_value());
    EXPECT_TRUE(verdict.value_if()->valid());
  }
} // namespace
