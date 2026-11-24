#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_test_storage.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief Result of registering and compiling schema texts.
   * @par Requirements
   * P054; J5-054; J5-063.
   */
  struct compile_outcome
  {
      std::optional<schema_error_code> error{};
      std::optional<invalid_schema_reason> reason{};
      std::optional<schema_limit_reason> limit{};
      std::optional<source_span> range{};
      bool registered{true};
  };

  /**
   * @brief Register every text (the first as the root) and compile.
   * @par Requirements
   * P054; J5-054; J5-063.
   */
  compile_outcome compile_texts(const std::vector<std::string>& texts, std::size_t scratch_fail_on = 0U,
                                const schema_compile_limits& limits = {})
  {
    compile_outcome outcome{};
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    scratch_provider.fail_on = scratch_fail_on;
    accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
    auto registry = schema_registry::create(storage, texts.size());
    for (std::size_t index = 0U; index < texts.size(); ++index)
    {
      const std::string uri = "urn:test:" + std::to_string(index);
      const auto added = (index == 0U) ? registry.value_if()->add({texts[0].data(), texts[0].size()})
                                       : registry.value_if()->add(uri, {texts[index].data(), texts[index].size()});
      if (!added.has_value())
      {
        outcome.registered = false;
        outcome.error = added.error_if()->code();
        outcome.reason = added.error_if()->invalid_schema_reason();
        return outcome;
      }
    }
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    options.limits = limits;
    const auto compiled =
      compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    EXPECT_EQ(scratch_provider.live_bytes, 0U);
    if (!compiled.has_value())
    {
      outcome.error = compiled.error_if()->code();
      outcome.reason = compiled.error_if()->invalid_schema_reason();
      outcome.limit = compiled.error_if()->limit_reason();
      outcome.range = compiled.error_if()->source_range();
    }
    return outcome;
  }

  /**
   * @brief One compile case: schema text and the expected error and reason (none = compiles).
   * @par Requirements
   * P054; J5-054; J5-063.
   */
  struct compile_case
  {
      const char* name;
      const char* schema;
      std::optional<schema_error_code> error;
      std::optional<invalid_schema_reason> reason;
  };

  constexpr std::optional<schema_error_code> c_compiles{};
  constexpr std::optional<schema_error_code> c_invalid{schema_error_code::invalid_schema};
  constexpr std::optional<invalid_schema_reason> c_shape{invalid_schema_reason::keyword_shape};

  /* ================================= Test_P054_MetaSchema ================================= */
  class P054_MetaSchema : public testing::TestWithParam<compile_case>
  {
  };

  /*
   * Arrange: Schemas whose only defect is one the Draft-07 meta-schema rejects (metadata types,
   *          enum uniqueness, empty schema arrays), valid metadata and unknown keywords, and
   *          schemas with each other failure class.
   * Act: Register and compile.
   * Expect: Meta-schema failures are invalid_schema / keyword_shape; dialect, reference and
   *         pattern failures keep their own codes and reasons; valid schemas compile (J5-054,
   *         J5-063).
   */
  TEST_P(P054_MetaSchema, SeparatesFailureClasses)
  {
    const compile_case& item = GetParam();
    const compile_outcome outcome = compile_texts({item.schema});
    EXPECT_EQ(outcome.error, item.error) << item.schema;
    if (item.reason.has_value())
    {
      EXPECT_EQ(outcome.reason, item.reason) << item.schema;
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P054_MetaSchema,
    testing::Values(
      compile_case{"TitleNotString", R"({"title": 5})", c_invalid, c_shape},
      compile_case{"DescriptionNotString", R"({"description": []})", c_invalid, c_shape},
      compile_case{"CommentNotString", R"({"$comment": {}})", c_invalid, c_shape},
      compile_case{"ReadOnlyNotBoolean", R"({"readOnly": "yes"})", c_invalid, c_shape},
      compile_case{"WriteOnlyNotBoolean", R"({"writeOnly": 0})", c_invalid, c_shape},
      compile_case{"ExamplesNotArray", R"({"examples": {}})", c_invalid, c_shape},
      compile_case{"ContentMediaTypeNotString", R"({"contentMediaType": 1})", c_invalid, c_shape},
      compile_case{"ContentEncodingNotString", R"({"contentEncoding": true})", c_invalid, c_shape},
      compile_case{"EnumDuplicates", R"({"enum": [1, 1.0]})", c_invalid, c_shape},
      compile_case{"EnumEmpty", R"({"enum": []})", c_invalid, c_shape},
      compile_case{"EmptyAllOf", R"({"allOf": []})", c_invalid, c_shape},
      compile_case{"EmptyTuple", R"({"items": []})", c_invalid, c_shape},
      compile_case{"NestedMetadata", R"({"properties": {"a": {"items": {"title": false}}}})", c_invalid, c_shape},
      compile_case{"DefinitionsMetadata", R"({"definitions": {"x": {"examples": 3}}})", c_invalid, c_shape},
      compile_case{"IdNotString", R"({"$id": 1})", c_invalid, std::nullopt},
      compile_case{"ValidMetadata",
                   R"({"title": "t", "description": "d", "$comment": "c", "readOnly": true, "writeOnly": false,
                       "examples": [1, {}], "default": null, "contentMediaType": "text/plain",
                       "contentEncoding": "base64", "format": "anything"})",
                   c_compiles, std::nullopt},
      compile_case{"UnknownKeywords", R"({"x-vendor": {"type": 5}, "maximun": "no"})", c_compiles, std::nullopt},
      compile_case{"RequiredEmpty", R"({"required": []})", c_compiles, std::nullopt},
      compile_case{"BooleanSchemas", "true", c_compiles, std::nullopt},
      compile_case{"OtherDialectWins",
                   R"({"$schema": "http://json-schema.org/draft-04/schema#", "exclusiveMinimum": true})",
                   schema_error_code::unsupported_dialect, std::nullopt},
      compile_case{"UnresolvedReference", R"({"$ref": "#/definitions/missing"})",
                   schema_error_code::unresolved_reference, std::nullopt},
      compile_case{"MalformedPattern", R"({"pattern": "(a"})", c_invalid, invalid_schema_reason::invalid_pattern},
      compile_case{"UnsupportedPattern", R"({"pattern": "(?<=a)b"})", c_invalid,
                   invalid_schema_reason::unsupported_pattern},
      compile_case{"NotAnObject", "[]", c_invalid, c_shape}),
    [](const testing::TestParamInfo<compile_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: A schema whose only meta-schema defect is a nested title value.
   * Act: Compile.
   * Expect: The error range is the offending value in the registered text (J5-054, J5-064).
   */
  TEST(P054_MetaSchemaDetails, ReportsTheOffendingValue)
  {
    const std::string text = R"({"properties": {"a": {"title": 42}}})";
    const compile_outcome outcome = compile_texts({text});
    ASSERT_EQ(outcome.error, c_invalid);
    ASSERT_TRUE(outcome.range.has_value());
    EXPECT_EQ(text.substr(outcome.range->byte_offset, outcome.range->byte_length), "42");
  }

  /*
   * Arrange: A valid root and an invalid second registered resource.
   * Act: Compile.
   * Expect: Every registered resource is checked, not only the root (J5-054, J5-055).
   */
  TEST(P054_MetaSchemaDetails, ChecksEveryRegisteredResource)
  {
    EXPECT_EQ(compile_texts({R"({"$ref": "urn:test:1"})", R"({"type": "string"})"}).error, c_compiles);
    EXPECT_EQ(compile_texts({R"({"$ref": "urn:test:1"})", R"({"title": 1})"}).error, c_invalid);
  }

  /*
   * Arrange: A schema compiled with scratch allocation n failing, and with every step limit.
   * Act: Compile for n = 1, 2, ... and for limits 1, 2, ... until it succeeds.
   * Expect: allocation_failure or resource_limit (max_compile_steps), never another error, with
   *         scratch storage released; then the schema compiles (J5-046, J5-047, J5-065).
   */
  TEST(P054_MetaSchemaDetails, FailuresDuringTheMetaCheck)
  {
    const std::string text = R"({"title": "x", "properties": {"a": {"enum": [1, 2], "examples": [1]}}})";
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      const compile_outcome outcome = compile_texts({text}, fail_on);
      if (!outcome.error.has_value())
      {
        break;
      }
      ASSERT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::allocation_failure}) << fail_on;
      ASSERT_LT(fail_on, 5000U);
    }
    for (std::size_t steps = 1U;; steps += 7U)
    {
      schema_compile_limits limits{};
      limits.max_compile_steps = steps;
      const compile_outcome outcome = compile_texts({text}, 0U, limits);
      if (!outcome.error.has_value())
      {
        break;
      }
      ASSERT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::resource_limit}) << steps;
      ASSERT_EQ(outcome.limit, std::optional<schema_limit_reason>{schema_limit_reason::max_compile_steps}) << steps;
      ASSERT_LT(steps, 100000U);
    }
  }
} // namespace
