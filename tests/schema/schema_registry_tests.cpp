#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_test_storage.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief Register text under uri into a fresh registry of the given capacity.
   * @par Requirements
   * P044; J5-055.
   */
  struct registry_fixture
  {
      schema_test_storage provider;
      accounted_storage storage{provider.callbacks(), 1U << 20U};
      result<schema_registry, schema_error> registry = schema_registry::create(storage, 2U);

      result<void, schema_error> add(std::string_view uri, std::string_view text)
      {
        return registry.value_if()->add(uri, {text.data(), text.size()});
      }
  };

  /* =================================== Test_P044_Registration ==================================== */
  /*
   * Arrange: An empty registry.
   * Act: Register under relative, absolute and equivalent-but-differently-spelled URIs.
   * Expect: relative_uri and duplicate_uri (after normalization) fail and leave the registry unchanged.
   */
  TEST(P044_Registration, RejectsRelativeAndDuplicateUris)
  {
    registry_fixture fixture;
    ASSERT_TRUE(fixture.registry.has_value());
    const auto relative = fixture.add("schemas/a.json", "{}");
    const auto first = fixture.add("http://Example.com/a/./b.json#", "{}");
    const auto duplicate = fixture.add("HTTP://example.COM/a/b.json", "true");

    ASSERT_FALSE(relative.has_value());
    EXPECT_EQ(relative.error_if()->code(), schema_error_code::invalid_registration);
    EXPECT_EQ(relative.error_if()->registration_reason(), registration_reason::relative_uri);
    EXPECT_TRUE(first.has_value());
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error_if()->registration_reason(), registration_reason::duplicate_uri);
    EXPECT_EQ(fixture.registry.value_if()->size(), 1U);
  }

  /*
   * Arrange: Texts that are not strict JSON.
   * Act: Register them.
   * Expect: invalid_schema (syntax) with the parser's error and location unchanged.
   */
  TEST(P044_Registration, ReportsStrictSyntaxErrors)
  {
    for (const char* text : {"{a: 1}", "{\"a\": 1, // c\n}", "{\"a\": 1, \"a\": 2}", "[1,]"})
    {
      registry_fixture fixture;
      const auto added = fixture.add("http://example.com/s.json", text);
      ASSERT_FALSE(added.has_value()) << text;
      EXPECT_EQ(added.error_if()->code(), schema_error_code::invalid_schema) << text;
      EXPECT_EQ(added.error_if()->invalid_schema_reason(), invalid_schema_reason::syntax) << text;
      ASSERT_TRUE(added.error_if()->parser_error().has_value()) << text;
      EXPECT_TRUE(added.error_if()->parser_error()->has_byte_offset()) << text;
    }
  }

  /*
   * Arrange: Strict JSON holding an unpaired surrogate in a value and in a name (ADR 0006 O5).
   * Act: Register it.
   * Expect: invalid_schema (domain) at the offending string.
   */
  TEST(P044_Registration, RejectsUnpairedSurrogates)
  {
    for (const char* text : {R"({"title": "\ud800"})", R"({"properties": {"\udc00": {}}})"})
    {
      registry_fixture fixture;
      const auto added = fixture.add("http://example.com/s.json", text);
      ASSERT_FALSE(added.has_value()) << text;
      EXPECT_EQ(added.error_if()->invalid_schema_reason(), invalid_schema_reason::domain) << text;
      EXPECT_TRUE(added.error_if()->source_range().has_value()) << text;
    }
  }

  /*
   * Arrange: A registry of capacity 2.
   * Act: Register three resources, the first with the default base URI.
   * Expect: The third fails with resource_limit (max_resources).
   */
  TEST(P044_Registration, EnforcesCapacityAndDefaultUri)
  {
    registry_fixture fixture;
    const std::string text = "{}";
    const auto default_uri = fixture.registry.value_if()->add({text.data(), text.size()});
    const auto second = fixture.add("http://example.com/b.json", "{}");
    const auto third = fixture.add("http://example.com/c.json", "{}");
    const auto again = fixture.add(c_default_schema_base_uri, "{}");

    EXPECT_TRUE(default_uri.has_value());
    EXPECT_TRUE(second.has_value());
    ASSERT_FALSE(third.has_value());
    EXPECT_EQ(third.error_if()->limit_reason(), schema_limit_reason::max_resources);
    EXPECT_FALSE(again.has_value());
    EXPECT_EQ(fixture.registry.value_if()->capacity(), 2U);
  }

  /*
   * Arrange: A registry with one resource.
   * Act: Move it, then let both objects go out of scope.
   * Expect: The moved-to registry owns the resource; all storage is released once.
   */
  TEST(P044_Registration, MovesAndReleases)
  {
    schema_test_storage provider;
    {
      accounted_storage storage{provider.callbacks(), 1U << 20U};
      auto registry = schema_registry::create(storage, 1U);
      ASSERT_TRUE(registry.has_value());
      const std::string text = R"({"type": "string"})";
      ASSERT_TRUE(registry.value_if()->add("http://example.com/a.json", {text.data(), text.size()}).has_value());
      schema_registry moved{std::move(*registry.value_if())};
      EXPECT_EQ(moved.size(), 1U);
      EXPECT_EQ(registry.value_if()->size(), 0U);
      EXPECT_FALSE(storage.faulted());
    }
    EXPECT_EQ(provider.live_bytes, 0U);
  }

  /*
   * Arrange: A registry, a compiled schema and then the registry and text destroyed.
   * Act: Query the compiled schema.
   * Expect: It is independent of the registry (J5-066) and reports its node count.
   */
  TEST(P044_Registration, CompiledSchemaOutlivesRegistry)
  {
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    std::optional<compiled_schema> kept;
    {
      schema_test_storage registry_provider;
      accounted_storage storage{registry_provider.callbacks(), 1U << 20U};
      auto registry = schema_registry::create(storage, 1U);
      std::string text = R"({"definitions": {"a": {}}, "$ref": "#/definitions/a"})";
      ASSERT_TRUE(registry.value_if()->add({text.data(), text.size()}).has_value());
      compile_options options{};
      options.result_storage = result_provider.callbacks();
      options.scratch_storage = scratch_provider.callbacks();
      auto compiled =
        compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
      ASSERT_TRUE(compiled.has_value());
      kept.emplace(std::move(*compiled.value_if()));
      text.assign(text.size(), 'x');
    }
    ASSERT_TRUE(kept.has_value());
    EXPECT_EQ(kept->node_count(), 2U);
    EXPECT_EQ(kept->resource_count(), 1U);
    EXPECT_EQ(scratch_provider.live_bytes, 0U);
    kept.reset();
  }
} // namespace
