#include "../../../tests/schema/schema_test_storage.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"
#include "adaptive_json5/schema/schema_registry.hpp"
#include "schema/schema_graph.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::schema_test_storage;

  using resource = std::pair<std::string, std::string>;

  /**
   * @brief Registry storage, result storage and scratch storage for one compilation.
   * @par Requirements
   * P044; J5-066.
   * @note Members are declared so the providers outlive everything allocated from them.
   */
  struct compile_fixture
  {
      schema_test_storage registry_provider;
      schema_test_storage result_provider;
      schema_test_storage scratch_provider;
      accounted_storage registry_storage{registry_provider.callbacks(), 1U << 24U};
      std::optional<result<compiled_schema, schema_error>> outcome;

      /**
       * @brief Register resources (the texts are destroyed afterwards), compile, drop the registry.
       * @par Requirements
       * P044; J5-066.
       */
      void compile(std::vector<resource> resources, std::string_view root,
                   schema_compile_limits limits = schema_compile_limits{})
      {
        auto registry = schema_registry::create(registry_storage, resources.size());
        ASSERT_TRUE(registry.has_value());
        for (auto& entry : resources)
        {
          const auto added = registry.value_if()->add(entry.first, {entry.second.data(), entry.second.size()});
          ASSERT_TRUE(added.has_value()) << entry.second;
          entry.second.assign(entry.second.size(), '#');
        }
        compile_options options{};
        options.limits = limits;
        options.result_storage = result_provider.callbacks();
        options.scratch_storage = scratch_provider.callbacks();
        outcome.emplace(compile_schema(*registry.value_if(), root, schema_dialect::draft_07, options));
        EXPECT_EQ(scratch_provider.live_bytes, 0U);
      }

      const compiled_schema& schema() const
      {
        return *outcome->value_if();
      }

      const schema_error& failure() const
      {
        return *outcome->error_if();
      }

      /**
       * @brief Return the ref target of the n-th node that has a ref, in node order.
       * @par Requirements
       * P044.
       */
      std::size_t ref_target(std::size_t ordinal) const
      {
        std::size_t seen = 0U;
        for (std::size_t index = 0U; index < schema().node_count(); ++index)
        {
          const auto& node = compiled_schema_access::node(schema(), index);
          if (node.ref != c_no_node)
          {
            if (seen == ordinal)
            {
              return node.ref;
            }
            ++seen;
          }
        }
        return c_no_node;
      }

      /**
       * @brief Return the step keyword and child position of a node.
       * @par Requirements
       * P044.
       */
      std::string describe(std::size_t index) const
      {
        if (index == c_no_node)
        {
          return "<none>";
        }
        const auto& node = compiled_schema_access::node(schema(), index);
        std::string text{keyword_name(node.step_keyword)};
        if (node.step == step_kind::name)
        {
          text += "/";
          for (std::size_t unit = 0U; unit < node.step_name.count; ++unit)
          {
            text += static_cast<char>(compiled_schema_access::unit(schema(), node.step_name.first + unit));
          }
        }
        else if (node.step == step_kind::index)
        {
          text += "/" + std::to_string(node.step_index);
        }
        else
        {
        }
        return text;
      }
  };

  constexpr std::string_view c_root{"http://example.com/root.json"};

  /* =================================== Test_P044_References ==================================== */
  /*
   * Arrange: A root that refers to its own definition by pointer.
   * Act: Compile; the registry and texts are destroyed before inspection.
   * Expect: The ref resolves to the definitions/a node; the compiled schema stays usable.
   */
  TEST(P044_References, ResolvesLocalPointer)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root}, R"({"definitions": {"a": true}, "allOf": [{"$ref": "#/definitions/a"}]})"}},
                    c_root);
    ASSERT_TRUE(fixture.outcome->has_value()) << static_cast<int>(fixture.failure().code());
    EXPECT_EQ(fixture.describe(fixture.ref_target(0U)), "definitions/a");
    EXPECT_EQ(fixture.schema().resource_count(), 1U);
    EXPECT_EQ(fixture.registry_provider.live_bytes, 0U);
  }

  /*
   * Arrange: Pointer tokens with ~1, ~0 and %25 in definition names.
   * Act: Compile.
   * Expect: Each ref finds the definition with the decoded name.
   */
  TEST(P044_References, DecodesEscapedPointerTokens)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root},
                      R"({"definitions": {"a/b": true, "c~d": true, "e%f": true},
                          "allOf": [{"$ref": "#/definitions/a~1b"}, {"$ref": "#/definitions/c~0d"},
                                    {"$ref": "#/definitions/e%25f"}]})"}},
                    c_root);
    ASSERT_TRUE(fixture.outcome->has_value());
    EXPECT_EQ(fixture.describe(fixture.ref_target(0U)), "definitions/a/b");
    EXPECT_EQ(fixture.describe(fixture.ref_target(1U)), "definitions/c~d");
    EXPECT_EQ(fixture.describe(fixture.ref_target(2U)), "definitions/e%f");
  }

  /*
   * Arrange: An embedded resource with a relative $id and a ref to it from the root.
   * Act: Compile.
   * Expect: The ref resolves against the root base to the embedded resource's definition.
   */
  TEST(P044_References, ResolvesNestedId)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root},
                      R"({"$id": "http://example.com/root.json",
                          "definitions": {"inner": {"$id": "nested/other.json",
                                                    "definitions": {"b": {"type": "integer"}}}},
                          "allOf": [{"$ref": "nested/other.json#/definitions/b"},
                                    {"$ref": "http://example.com/nested/other.json"}]})"}},
                    c_root);
    ASSERT_TRUE(fixture.outcome->has_value()) << static_cast<int>(fixture.failure().code());
    EXPECT_EQ(fixture.describe(fixture.ref_target(0U)), "definitions/b");
    EXPECT_EQ(fixture.describe(fixture.ref_target(1U)), "definitions/inner");
  }

  /*
   * Arrange: A plain-name $id and a ref to it (ADR 0006 O1).
   * Act: Compile.
   * Expect: The ref finds the node with the plain name.
   */
  TEST(P044_References, ResolvesPlainName)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root}, R"({"definitions": {"x": {"$id": "#foo"}}, "not": {"$ref": "#foo"}})"}},
                    c_root);
    ASSERT_TRUE(fixture.outcome->has_value());
    EXPECT_EQ(fixture.describe(fixture.ref_target(0U)), "definitions/x");
  }

  /*
   * Arrange: A $id beside $ref that would change the base URI (ADR 0006 O2).
   * Act: Compile.
   * Expect: The sibling $id is ignored, so the ref resolves against the root base.
   */
  TEST(P044_References, IgnoresIdBesideRef)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root},
                      R"({"definitions": {"b": true,
                                          "a": {"$id": "http://other.example/", "$ref": "#/definitions/b"}}})"}},
                    c_root);
    ASSERT_TRUE(fixture.outcome->has_value()) << static_cast<int>(fixture.failure().code());
    EXPECT_EQ(fixture.describe(fixture.ref_target(0U)), "definitions/b");
  }

  /*
   * Arrange: Two registered resources; the root refers to the other with a relative URI.
   * Act: Compile.
   * Expect: The ref resolves into the second resource.
   */
  TEST(P044_References, ResolvesRegisteredResource)
  {
    compile_fixture fixture;
    fixture.compile({{"http://example.com/a/root.json", R"({"$ref": "../b/types.json#/definitions/n"})"},
                     {"HTTP://EXAMPLE.COM/b/types.json#", R"({"definitions": {"n": {"type": "number"}}})"}},
                    "http://example.com/a/root.json");
    ASSERT_TRUE(fixture.outcome->has_value()) << static_cast<int>(fixture.failure().code());
    const std::size_t target = fixture.ref_target(0U);
    EXPECT_EQ(fixture.describe(target), "definitions/n");
    EXPECT_EQ(compiled_schema_access::node(fixture.schema(), target).resource, 1U);
    EXPECT_EQ(fixture.schema().resource_count(), 2U);
  }

  /*
   * Arrange: A root URI with a pointer fragment.
   * Act: Compile.
   * Expect: The root is the named subschema.
   */
  TEST(P044_References, SelectsRootByFragment)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root}, R"({"definitions": {"a": {}}})"}},
                    "http://example.com/root.json#/definitions/a");
    ASSERT_TRUE(fixture.outcome->has_value());
    EXPECT_EQ(fixture.describe(compiled_schema_access::root(fixture.schema())), "definitions/a");
  }

  /* =================================== Test_P044_CompileErrors ==================================== */
  struct error_case
  {
      const char* name;
      const char* text;
      schema_error_code code;
      std::optional<invalid_schema_reason> reason;
  };

  class P044_CompileErrors : public testing::TestWithParam<error_case>
  {
  };

  /*
   * Arrange: Schemas with one compile error each.
   * Act: Compile.
   * Expect: The documented code and sub-reason, a source range, and no leaked storage.
   */
  TEST_P(P044_CompileErrors, ReportsTheError)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root}, GetParam().text}}, c_root);
    ASSERT_FALSE(fixture.outcome->has_value());
    EXPECT_EQ(fixture.failure().code(), GetParam().code);
    EXPECT_EQ(fixture.failure().invalid_schema_reason(), GetParam().reason);
    EXPECT_TRUE(fixture.failure().source_range().has_value());
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P044_CompileErrors,
    testing::Values(
      error_case{"missing_pointer", R"({"$ref": "#/definitions/missing"})", schema_error_code::unresolved_reference,
                 std::nullopt},
      error_case{"missing_resource", R"({"$ref": "other.json"})", schema_error_code::unresolved_reference,
                 std::nullopt},
      error_case{"pointer_into_keyword", R"({"definitions": {"a": {}}, "not": {"$ref": "#/definitions"}})",
                 schema_error_code::unresolved_reference, std::nullopt},
      error_case{"bad_escape", R"({"$ref": "#/a~2"})", schema_error_code::unresolved_reference, std::nullopt},
      error_case{"self_cycle", R"({"$ref": "#"})", schema_error_code::nonprogressing_reference, std::nullopt},
      error_case{"two_step_cycle",
                 R"({"definitions": {"a": {"$ref": "#/definitions/b"}, "b": {"$ref": "#/definitions/a"}}})",
                 schema_error_code::nonprogressing_reference, std::nullopt},
      error_case{"other_dialect", R"({"$schema": "http://json-schema.org/draft-06/schema#"})",
                 schema_error_code::unsupported_dialect, std::nullopt},
      error_case{"https_dialect", R"({"$schema": "https://json-schema.org/draft-07/schema#"})",
                 schema_error_code::unsupported_dialect, std::nullopt},
      error_case{"dialect_not_string", R"({"$schema": 7})", schema_error_code::invalid_schema,
                 invalid_schema_reason::keyword_shape},
      error_case{"root_number", "3", schema_error_code::invalid_schema, invalid_schema_reason::keyword_shape},
      error_case{"empty_all_of", R"({"allOf": []})", schema_error_code::invalid_schema,
                 invalid_schema_reason::keyword_shape},
      error_case{"not_number", R"({"not": 5})", schema_error_code::invalid_schema,
                 invalid_schema_reason::keyword_shape},
      error_case{"properties_array", R"({"properties": []})", schema_error_code::invalid_schema,
                 invalid_schema_reason::keyword_shape},
      error_case{"ref_number", R"({"$ref": 1})", schema_error_code::invalid_schema,
                 invalid_schema_reason::keyword_shape},
      error_case{"id_number", R"({"$id": 1})", schema_error_code::invalid_schema,
                 invalid_schema_reason::keyword_shape}),
    [](const testing::TestParamInfo<error_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: A subschema $schema naming another draft (ignored, ADR 0006 decision 2) and
   *          Draft-07 spellings at the root.
   * Act: Compile.
   * Expect: Success.
   */
  TEST(P044_CompileDialect, AcceptsDraft07AndIgnoresSubschemaSchema)
  {
    for (const char* text : {R"({"$schema": "http://json-schema.org/draft-07/schema#"})",
                             R"({"$schema": "http://json-schema.org/draft-07/schema"})",
                             R"({"not": {"$schema": "http://json-schema.org/draft-04/schema#"}})"})
    {
      compile_fixture fixture;
      fixture.compile({{std::string{c_root}, text}}, c_root);
      EXPECT_TRUE(fixture.outcome->has_value()) << text;
    }
  }

  /*
   * Arrange: A relative and an unknown root URI.
   * Act: Compile.
   * Expect: unresolved_reference.
   */
  TEST(P044_CompileDialect, RejectsUnknownRoot)
  {
    for (const char* root : {"root.json", "http://example.com/other.json"})
    {
      compile_fixture fixture;
      fixture.compile({{std::string{c_root}, "{}"}}, root);
      ASSERT_FALSE(fixture.outcome->has_value()) << root;
      EXPECT_EQ(fixture.failure().code(), schema_error_code::unresolved_reference);
    }
  }

  /* =================================== Test_P044_CompileLimits ==================================== */
  /*
   * Arrange: A schema with five nodes.
   * Act: Compile with max_schema_nodes 5 and 4.
   * Expect: 5 passes; 4 returns resource_limit (max_schema_nodes).
   */
  TEST(P044_CompileLimits, NodeLimitIsInclusive)
  {
    constexpr const char* c_text = R"({"allOf": [true, {"not": false}], "properties": {"a": {}}})";
    schema_compile_limits exact{};
    exact.max_schema_nodes = 5U;
    schema_compile_limits short_limits{};
    short_limits.max_schema_nodes = 4U;
    compile_fixture passing;
    passing.compile({{std::string{c_root}, c_text}}, c_root, exact);
    ASSERT_TRUE(passing.outcome->has_value());
    EXPECT_EQ(passing.schema().node_count(), 5U);
    compile_fixture failing;
    failing.compile({{std::string{c_root}, c_text}}, c_root, short_limits);
    ASSERT_FALSE(failing.outcome->has_value());
    EXPECT_EQ(failing.failure().limit_reason(), schema_limit_reason::max_schema_nodes);
  }

  /*
   * Arrange: A small schema and a compile-step limit swept upward from zero.
   * Act: Compile at each limit.
   * Expect: Every failure is resource_limit (max_compile_steps) until the first success,
   *         after which larger limits also succeed.
   */
  TEST(P044_CompileLimits, StepLimitFailsCleanlyUntilEnough)
  {
    constexpr const char* c_text = R"({"definitions": {"a": true}, "allOf": [{"$ref": "#/definitions/a"}]})";
    std::size_t limit = 0U;
    for (;; ++limit)
    {
      schema_compile_limits limits{};
      limits.max_compile_steps = limit;
      compile_fixture fixture;
      fixture.compile({{std::string{c_root}, c_text}}, c_root, limits);
      if (fixture.outcome->has_value())
      {
        break;
      }
      ASSERT_EQ(fixture.failure().limit_reason(), schema_limit_reason::max_compile_steps) << limit;
      ASSERT_LT(limit, 1000U);
    }
    EXPECT_GT(limit, 0U);
  }

  /*
   * Arrange: A schema and allocation failures injected at every scratch and result allocation.
   * Act: Compile with failure n = 1, 2, ... until compilation succeeds.
   * Expect: Each failure is allocation_failure, and no storage remains allocated.
   */
  TEST(P044_CompileLimits, AllocationFailureSweep)
  {
    constexpr const char* c_text =
      R"({"$id": "http://example.com/root.json", "definitions": {"a": {"$id": "#a"}},
            "properties": {"p": {"$ref": "#a"}}, "items": [true, false]})";
    for (bool scratch : {true, false})
    {
      for (std::size_t fail_on = 1U;; ++fail_on)
      {
        compile_fixture fixture;
        (scratch ? fixture.scratch_provider : fixture.result_provider).fail_on = fail_on;
        fixture.compile({{std::string{c_root}, c_text}}, c_root);
        if (fixture.outcome->has_value())
        {
          break;
        }
        ASSERT_EQ(fixture.failure().code(), schema_error_code::allocation_failure) << fail_on;
        ASSERT_LT(fail_on, 500U);
      }
    }
  }

  /*
   * Arrange: A schema with two unknown keywords and one known annotation.
   * Act: Compile.
   * Expect: Both unknown keywords are counted; known keywords are not.
   */
  TEST(P044_CompileGraph, CountsUnknownKeywords)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root}, R"({"title": "t", "x-custom": 1, "properites": {}})"}}, c_root);
    ASSERT_TRUE(fixture.outcome->has_value());
    EXPECT_EQ(fixture.schema().unknown_keyword_count(), 2U);
  }

  /*
   * Arrange: A compiled schema.
   * Act: Move it into a new object.
   * Expect: The moved-to schema keeps the graph; the result storage is released at the end.
   */
  TEST(P044_CompileGraph, MovesAndReleases)
  {
    compile_fixture fixture;
    fixture.compile({{std::string{c_root}, R"({"not": {"not": true}})"}}, c_root);
    ASSERT_TRUE(fixture.outcome->has_value());
    {
      compiled_schema moved{std::move(*fixture.outcome->value_if())};
      EXPECT_EQ(moved.node_count(), 3U);
      EXPECT_GT(fixture.result_provider.live_bytes, 0U);
    }
    EXPECT_EQ(fixture.result_provider.live_bytes, 0U);
  }
} // namespace
