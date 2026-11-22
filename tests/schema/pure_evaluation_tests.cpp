#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_test_storage.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>

/* ==================================== Fixtures ===================================== */
namespace
{
  bool g_counting = false;
  std::size_t g_default_heap_calls = 0;
} // namespace

namespace adaptive_json5_test
{
  /**
   * @brief Count global heap use while the observation window is open.
   * @par Requirements
   * P046; J5-067.
   */
  void record_heap_allocation() noexcept
  {
    if (g_counting)
    {
      ++g_default_heap_calls;
    }
  }
} // namespace adaptive_json5_test
/* ================================== End Fixtures =================================== */

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::schema_test_storage;

  /* =================================== Test_P046_PureEvaluation ==================================== */
  /*
   * Arrange: Two registered resources with references, an instance, and caller storage only.
   * Act: Register, compile and validate (valid and invalid instances, both modes) while
   *      counting global operator new calls.
   * Expect: No global heap use: all storage comes through the caller's callbacks, and the
   *         only external schema arrives through the registry (J5-067). The source-level
   *         I/O check is J5_042_PureMemory.ProductionIncludesOnlyLanguageSupportHeaders,
   *         which also scans core/src/schema and includes/adaptive_json5/schema.
   */
  TEST(P046_PureEvaluation, UsesOnlyCallerStorage)
  {
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    schema_test_storage instance_provider;
    schema_test_storage work_provider;
    schema_test_storage report_provider;
    const std::string root = R"({"properties": {"a": {"$ref": "types.json#/definitions/item"}}})";
    const std::string types = R"({"definitions": {"item": {"items": {"$ref": "#/definitions/element"}},
                                                    "element": {"properties": {"bad": false},
                                                                "items": {"anyOf": [{"$ref": "#/definitions/element"}]}}}})";
    const std::string valid_text = R"({"a": [[], [[]]]})";
    const std::string invalid_text = R"({"a": [[{"bad": 1}]]})";
    std::size_t heap_calls = 0U;
    bool valid = false;
    bool invalid = true;
    {
      accounted_storage registry_storage{registry_provider.callbacks(), 1U << 20U};
      accounted_storage instance_storage{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};

      g_default_heap_calls = 0U;
      g_counting = true;
      auto registry = schema_registry::create(registry_storage, 2U);
      const bool registered =
        registry.has_value() &&
        registry.value_if()->add("http://example.com/root.json", {root.data(), root.size()}).has_value() &&
        registry.value_if()->add("http://example.com/types.json", {types.data(), types.size()}).has_value();
      compile_options compile{};
      compile.result_storage = result_provider.callbacks();
      compile.scratch_storage = scratch_provider.callbacks();
      auto compiled =
        compile_schema(*registry.value_if(), "http://example.com/root.json", schema_dialect::draft_07, compile);
      auto valid_document = parse_document({valid_text.data(), valid_text.size()}, parse_options{}, instance_storage);
      auto invalid_document =
        parse_document({invalid_text.data(), invalid_text.size()}, parse_options{}, instance_storage);
      validation_options options{};
      options.work_storage = work_provider.callbacks();
      options.report_storage = report_provider.callbacks();
      if (registered && compiled.has_value() && valid_document.has_value() && invalid_document.has_value())
      {
        const auto first = validate(*compiled.value_if(), valid_document.value_if()->values, options);
        options.mode = evaluation_mode::collect_all;
        const auto second = validate(*compiled.value_if(), invalid_document.value_if()->values, options);
        valid = first.has_value() && first.value_if()->valid();
        invalid = !second.has_value() || second.value_if()->valid();
      }
      g_counting = false;
      heap_calls = g_default_heap_calls;
      ASSERT_TRUE(registered);
      ASSERT_TRUE(compiled.has_value());
    }
    EXPECT_EQ(heap_calls, 0U);
    EXPECT_TRUE(valid);
    EXPECT_FALSE(invalid);
  }
} // namespace
