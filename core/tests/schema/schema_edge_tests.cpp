#include "../../../tests/schema/schema_test_storage.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/json_pointer.hpp"
#include "schema/pod_vector.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_work.hpp"
#include "schema/uri.hpp"
#include "schema/value_equality.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief Resolve reference against base, or "<error>".
   * @par Requirements
   * P044; J5-055.
   */
  std::string resolve(std::string_view base, std::string_view reference)
  {
    schema_test_storage storage;
    pod_vector<char> out{storage.callbacks()};
    const auto resolved = resolve_uri(base, reference, out, storage.callbacks());
    return resolved.has_value() ? std::string{text_of(out, 0U, out.size())} : std::string{"<error>"};
  }

  /**
   * @brief Compile one schema text under the default base URI; return the error code or nullopt.
   * @par Requirements
   * P044; J5-054; J5-055.
   */
  std::optional<schema_error_code> compile_error(const std::string& text,
                                                 std::string_view root = c_default_schema_base_uri)
  {
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    accounted_storage storage{registry_provider.callbacks(), 1U << 20U};
    auto registry = schema_registry::create(storage, 1U);
    EXPECT_TRUE(registry.value_if()->add({text.data(), text.size()}).has_value()) << text;
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    const auto compiled = compile_schema(*registry.value_if(), root, schema_dialect::draft_07, options);
    if (compiled.has_value())
    {
      return std::nullopt;
    }
    return compiled.error_if()->code();
  }

  /* =================================== Test_P044_EdgeCases ==================================== */
  /*
   * Arrange: Growth from empty, normal and overflowing capacities; a zero-byte copy.
   * Act: Ask for the next capacity and copy.
   * Expect: 8, doubling, nullopt on overflow; the empty copy touches nothing.
   */
  TEST(P044_EdgeCases, VectorGrowthBounds)
  {
    EXPECT_EQ(next_capacity(0U, 4U), std::optional<std::size_t>{8U});
    EXPECT_EQ(next_capacity(8U, 4U), std::optional<std::size_t>{16U});
    EXPECT_FALSE(next_capacity(std::numeric_limits<std::size_t>::max() / 4U, 4U).has_value());
    EXPECT_FALSE(next_capacity(std::numeric_limits<std::size_t>::max(), 1U).has_value());
    copy_bytes(nullptr, nullptr, 0U);
    char source[2] = {'a', 'b'};
    char destination[2] = {};
    copy_bytes(destination, source, 2U);
    EXPECT_EQ(std::string(destination, 2U), "ab");
  }

  /*
   * Arrange: References with a scheme and dot segments, bases without a path or slash, ports
   *          and an IPv6 host.
   * Act: Resolve them.
   * Expect: RFC 3986 section 5.2 results; only the host part is lowercased.
   */
  TEST(P044_EdgeCases, UriCornerCases)
  {
    EXPECT_EQ(resolve("http://a/b", "foo:../x/./y"), "foo:x/y");
    EXPECT_EQ(resolve("http://a/b", "foo:./x"), "foo:x");
    EXPECT_EQ(resolve("http://a/b", "foo:."), "foo:");
    EXPECT_EQ(resolve("http://a/b", "foo:.."), "foo:");
    EXPECT_EQ(resolve("http://a", "g"), "http://a/g");
    EXPECT_EQ(resolve("urn:x", "y"), "urn:y");
    EXPECT_EQ(resolve("HTTP://U:P@Host.EXAMPLE:8080/p", ""), "http://U:P@host.example:8080/p");
    EXPECT_EQ(resolve("http://[::ABC]/p", ""), "http://[::abc]/p");
    EXPECT_EQ(resolve("http://[::ABC]:80/p", ""), "http://[::abc]:80/p");
    EXPECT_EQ(resolve("a+b.c-1:x", "y"), "a+b.c-1:y");
    EXPECT_TRUE(split_uri("Z9:x").has_scheme);
    EXPECT_FALSE(split_uri("9z:x").has_scheme);
    EXPECT_FALSE(split_uri("a_b:x").has_scheme);
    EXPECT_EQ(resolve("http://a/b?q", "#f"), "http://a/b?q#f");
    EXPECT_EQ(resolve("http://a/b?q", "?r"), "http://a/b?r");
  }

  /*
   * Arrange: Pointer tokens with upper- and lower-case hex escapes, non-hex digits and
   *          non-digit indexes.
   * Act: Decode them.
   * Expect: Both hex cases decode; malformed escapes fail; indexes need digits only.
   */
  TEST(P044_EdgeCases, PointerEscapes)
  {
    schema_test_storage storage;
    pod_vector<char> token{storage.callbacks()};
    std::string_view rest = "/%41%62%7E0/%4G";
    ASSERT_TRUE(next_pointer_token(rest, token).has_value());
    EXPECT_EQ(std::string(text_of(token, 0U, token.size())), "Ab~");
    EXPECT_FALSE(next_pointer_token(rest, token).has_value());
    rest = "/%g0";
    EXPECT_FALSE(next_pointer_token(rest, token).has_value());
    rest = "/%F";
    EXPECT_FALSE(next_pointer_token(rest, token).has_value());
    rest = "";
    EXPECT_FALSE(next_pointer_token(rest, token).has_value());
    EXPECT_FALSE(pointer_index("1a").has_value());
    EXPECT_FALSE(pointer_index("a").has_value());
  }

  /*
   * Arrange: $id forms (plain name with a path, pointer-like fragment, absolute with name)
   *          and references to them.
   * Act: Compile.
   * Expect: Plain names resolve wherever they are declared; a pointer-like $id fragment is
   *         no plain name.
   */
  TEST(P044_EdgeCases, IdForms)
  {
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "other.json#foo"}}, "not": {"$ref": "other.json#foo"}})"),
              std::nullopt);
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "http://x.example/y.json#bar"}},
                                "not": {"$ref": "http://x.example/y.json#bar"}})"),
              std::nullopt);
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "#/x"}}, "not": {"$ref": "#/x"}})"),
              std::optional<schema_error_code>{schema_error_code::unresolved_reference});
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "http://x.example/y.json#"}},
                                "not": {"$ref": "http://x.example/y.json"}})"),
              std::nullopt);
  }

  /*
   * Arrange: Pointers through array keywords, non-ASCII and astral names, malformed UTF-8 and
   *          bad indexes; dependencies with array and schema values; keywords under a $ref node.
   * Act: Compile.
   * Expect: Valid pointers resolve, invalid ones are unresolved references, shapes are checked.
   */
  TEST(P044_EdgeCases, PointerTargets)
  {
    EXPECT_EQ(compile_error(R"({"allOf": [true, {"not": {"$ref": "#/allOf/0"}}]})"), std::nullopt);
    EXPECT_EQ(compile_error(R"({"allOf": [true, {"not": {"$ref": "#/allOf/01"}}]})"),
              std::optional<schema_error_code>{schema_error_code::unresolved_reference});
    EXPECT_EQ(compile_error(R"({"allOf": [true, {"not": {"$ref": "#/allOf/7"}}]})"),
              std::optional<schema_error_code>{schema_error_code::unresolved_reference});
    EXPECT_EQ(compile_error(R"({"definitions": {"é": true, "😀": true}, "allOf": [{"$ref": "#/definitions/%C3%A9"},
                                {"$ref": "#/definitions/%F0%9F%98%80"}]})"),
              std::nullopt);
    for (const char* bad : {"%FF", "%C3", "%F0%9F", "%C3%28", "é2", "😀2", "x"})
    {
      const std::string text =
        std::string{R"({"definitions": {"é": true, "😀": true}, "not": {"$ref": "#/definitions/)"} + bad + R"("}})";
      EXPECT_EQ(compile_error(text), std::optional<schema_error_code>{schema_error_code::unresolved_reference}) << bad;
    }
    EXPECT_EQ(compile_error(R"({"not": {"$ref": "#/not"}, "items": [true]})"),
              std::optional<schema_error_code>{schema_error_code::nonprogressing_reference});
    EXPECT_EQ(compile_error(R"({"dependencies": {"a": ["b"], "c": {"$ref": "#/dependencies/c"}}})"),
              std::optional<schema_error_code>{schema_error_code::nonprogressing_reference});
    EXPECT_EQ(compile_error(R"({"dependencies": {"a": ["b"], "c": true}, "not": {"$ref": "#/dependencies/c"}})"),
              std::nullopt);
    EXPECT_EQ(compile_error(R"({"definitions": []})"),
              std::optional<schema_error_code>{schema_error_code::invalid_schema});
    EXPECT_EQ(compile_error(R"({"definitions": {"t": true},
                                "$ref": "#/definitions/t", "items": [true], "allOf": [true], "not": true,
                                "properties": {"a": true}, "if": true, "then": true, "else": true})"),
              std::nullopt);
    EXPECT_EQ(compile_error("{}", "http://example.com/unknown.json#foo"),
              std::optional<schema_error_code>{schema_error_code::unresolved_reference});
    EXPECT_EQ(compile_error("{}", std::string{c_default_schema_base_uri} + "#/missing"),
              std::optional<schema_error_code>{schema_error_code::unresolved_reference});
  }

  /*
   * Arrange: Invalid (default) value views and strings of different length.
   * Act: Compare them.
   * Expect: Access failures are errors, never a verdict; different lengths are unequal.
   */
  TEST(P045_EdgeCases, EqualityOnInvalidViews)
  {
    schema_test_storage scratch;
    schema_work work{validation_budget{}, cancellation_callback{}};
    EXPECT_FALSE(values_equal(value_view{}, value_view{}, work, scratch.callbacks()).has_value());

    schema_test_storage provider;
    const std::string text = R"(["a", "ab", {"k": "a"}, {"k": "ab"}])";
    accounted_storage accounting{provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto parsed = parse_document({text.data(), text.size()}, parse_options{}, accounting);
    ASSERT_TRUE(parsed.has_value());
    const auto root = parsed.value_if()->values.root();
    const auto element = [&root](std::size_t index)
    {
      return *root.value_if()->element_at(index).value_if();
    };
    const auto equal = values_equal(element(0U), element(1U), work, scratch.callbacks());
    ASSERT_TRUE(equal.has_value());
    EXPECT_FALSE(*equal.value_if());
    const auto nested = values_equal(element(2U), element(3U), work, scratch.callbacks());
    ASSERT_TRUE(nested.has_value());
    EXPECT_FALSE(*nested.value_if());
    EXPECT_FALSE(values_equal(element(0U), value_view{}, work, scratch.callbacks()).has_value());
    EXPECT_FALSE(values_equal(value_view{}, element(0U), work, scratch.callbacks()).has_value());
  }

  /* =================================== Test_P046_EdgeCases ==================================== */
  /*
   * Arrange: A schema with an astral and a non-ASCII property name, and a diagnostic whose
   *          schema node is out of range.
   * Act: Validate and format the schema locations.
   * Expect: Names are written as UTF-8 pointer tokens; an out-of-range node yields "#".
   */
  TEST(P046_EdgeCases, SchemaLocationNames)
  {
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    accounted_storage storage{registry_provider.callbacks(), 1U << 20U};
    auto registry = schema_registry::create(storage, 1U);
    const std::string text = R"({"properties": {"é😀": {"items": [true, false]}}})";
    ASSERT_TRUE(registry.value_if()->add({text.data(), text.size()}).has_value());
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    const auto compiled =
      compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    ASSERT_TRUE(compiled.has_value());

    schema_test_storage instance_provider;
    const std::string instance_text = R"({"é😀": [1, 2, 3]})";
    accounted_storage accounting{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto instance = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, accounting);
    ASSERT_TRUE(instance.has_value());
    schema_test_storage work;
    schema_test_storage report_storage;
    validation_options run{};
    run.work_storage = work.callbacks();
    run.report_storage = report_storage.callbacks();
    const auto outcome = validate(*compiled.value_if(), instance.value_if()->values, run);
    ASSERT_TRUE(outcome.has_value());
    ASSERT_EQ(outcome.value_if()->diagnostic_count(), 1U);
    char buffer[64] = {};
    const std::size_t size =
      format_schema_location(*compiled.value_if(), *outcome.value_if()->diagnostic(0U), buffer, sizeof(buffer));
    EXPECT_EQ(std::string(buffer, size), "#/properties/é😀/items/1");

    schema_diagnostic foreign{};
    foreign.schema_node = 999U;
    EXPECT_EQ(format_schema_location(*compiled.value_if(), foreign, buffer, sizeof(buffer)), 1U);
  }
  /**
   * @brief Validate instance_text against schema_text with default options.
   * @par Requirements
   * P046.
   * @return The report's (valid, diagnostic count, schema location of the first diagnostic,
   * instance location of the first diagnostic), or nullopt on an operational error.
   */
  struct run_summary
  {
      bool valid{};
      std::size_t count{};
      std::string schema_location{};
      std::string instance_location{};
      std::optional<schema_error_code> error{};
  };

  run_summary run_schema(const std::string& schema_text, const std::string& instance_text)
  {
    run_summary summary{};
    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    accounted_storage storage{registry_provider.callbacks(), 1U << 20U};
    auto registry = schema_registry::create(storage, 1U);
    EXPECT_TRUE(registry.value_if()->add({schema_text.data(), schema_text.size()}).has_value()) << schema_text;
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    const auto compiled =
      compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    if (!compiled.has_value())
    {
      summary.error = compiled.error_if()->code();
      return summary;
    }
    schema_test_storage instance_provider;
    accounted_storage accounting{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto instance = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, accounting);
    EXPECT_TRUE(instance.has_value()) << instance_text;
    schema_test_storage work;
    schema_test_storage report_storage;
    validation_options run{};
    run.work_storage = work.callbacks();
    run.report_storage = report_storage.callbacks();
    const auto outcome = validate(*compiled.value_if(), instance.value_if()->values, run);
    if (!outcome.has_value())
    {
      summary.error = outcome.error_if()->code();
      return summary;
    }
    summary.valid = outcome.value_if()->valid();
    summary.count = outcome.value_if()->diagnostic_count();
    if (summary.count != 0U)
    {
      char buffer[128] = {};
      const auto& diagnostic = *outcome.value_if()->diagnostic(0U);
      summary.schema_location =
        std::string(buffer, format_schema_location(*compiled.value_if(), diagnostic, buffer, sizeof(buffer)));
      schema_test_storage pointer_scratch;
      const auto pointer = format_instance_location(instance.value_if()->values, diagnostic, buffer, sizeof(buffer),
                                                    pointer_scratch.callbacks());
      if (pointer.has_value())
      {
        summary.instance_location = std::string(buffer, *pointer.value_if());
      }
    }
    return summary;
  }

  /*
   * Arrange: Code units just outside the surrogate ranges, a high surrogate followed by one,
   *          and a valid pair.
   * Act: Validate against true.
   * Expect: Only unpaired surrogates are unsupported instances.
   */
  TEST(P043_EdgeCases, SurrogateBoundaries)
  {
    EXPECT_FALSE(run_schema("true", R"(["", "퟿"])").error.has_value());
    EXPECT_EQ(run_schema("true", R"(["\uD800"])").error,
              std::optional<schema_error_code>{schema_error_code::unsupported_instance});
    EXPECT_EQ(run_schema("true", R"(["\uD800\uD800"])").error,
              std::optional<schema_error_code>{schema_error_code::unsupported_instance});
    EXPECT_FALSE(run_schema("true", R"(["􏿿"])").error.has_value());
  }

  /*
   * Arrange: Escape digits and scheme characters on both sides of their ranges.
   * Act: Decode and split.
   * Expect: Only hex digits decode; only ASCII letters start a scheme.
   */
  TEST(P044_EdgeCases, CharacterRanges)
  {
    schema_test_storage storage;
    pod_vector<char> token{storage.callbacks()};
    for (const char* bad : {"/%!0", "/%@0", "/%:0", "/%`0", "/%g0"})
    {
      std::string_view rest = bad;
      EXPECT_FALSE(next_pointer_token(rest, token).has_value()) << bad;
    }
    std::string_view digits = "/%30%39%61%66%41%46";
    ASSERT_TRUE(next_pointer_token(digits, token).has_value());
    EXPECT_EQ(std::string(text_of(token, 0U, token.size())), "09afAF");
    EXPECT_FALSE(split_uri("{a:x").has_scheme);
    EXPECT_FALSE(split_uri("_a:x").has_scheme);
    EXPECT_FALSE(split_uri("@a:x").has_scheme);
    EXPECT_TRUE(split_uri("zA9+.-:x").has_scheme);
    EXPECT_FALSE(split_uri("a~b:x").has_scheme);
    EXPECT_FALSE(split_uri("a/b:x").has_scheme);
    EXPECT_EQ(resolve("http://u:p@h/p", ""), "http://u:p@h/p");
  }

  /*
   * Arrange: $id values that are not plain names (authority, query, empty or pointer
   *          fragment) and single-schema keywords under normal and $ref nodes.
   * Act: Compile.
   * Expect: Only "#name" forms register plain names; every keyword shape is checked.
   */
  TEST(P044_EdgeCases, IdAndKeywordForms)
  {
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "//other.example/s.json#n"}},
                                "not": {"$ref": "//other.example/s.json#n"}})"),
              std::nullopt);
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "?q#n"}}, "not": {"$ref": "?q#n"}})"), std::nullopt);
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "#"}}, "not": true})"), std::nullopt);
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$id": "http://x.example/y.json#/p"}},
                                "not": {"$ref": "http://x.example/y.json"}})"),
              std::nullopt);
    EXPECT_EQ(compile_error(R"({"additionalItems": true, "additionalProperties": false, "contains": {},
                                "propertyNames": true, "patternProperties": {"a": true}})"),
              std::nullopt);
    EXPECT_EQ(compile_error(R"({"allOf": {}})"), std::optional<schema_error_code>{schema_error_code::invalid_schema});
    EXPECT_EQ(compile_error(R"({"anyOf": []})"), std::optional<schema_error_code>{schema_error_code::invalid_schema});
    EXPECT_EQ(compile_error(R"({"definitions": {"a": []}})"),
              std::optional<schema_error_code>{schema_error_code::invalid_schema});
    EXPECT_EQ(compile_error(R"({"definitions": {"t": true}, "$ref": "#/definitions/t", "items": true})"), std::nullopt);
    EXPECT_EQ(
      compile_error(R"({"definitions": {"€": true, "😀": true}, "allOf": [{"$ref": "#/definitions/%E2%82%AC"}]})"),
      std::nullopt);
    for (const char* bad : {"%F0%9F%98%81", "%E2%82%AD", "%E2%82", "%80"})
    {
      const std::string text =
        std::string{R"({"definitions": {"€": true, "😀": true}, "not": {"$ref": "#/definitions/)"} + bad + R"("}})";
      EXPECT_EQ(compile_error(text), std::optional<schema_error_code>{schema_error_code::unresolved_reference}) << bad;
    }
  }

  /*
   * Arrange: Failures at a single-schema keyword, at a multi-digit array index and under
   *          a three-byte property name.
   * Act: Validate and format both locations.
   * Expect: RFC 6901 pointers with multi-digit indexes and UTF-8 names.
   */
  TEST(P046_EdgeCases, LocationForms)
  {
    const auto single = run_schema(R"({"items": false})", "[1]");
    EXPECT_EQ(single.schema_location, "#/items");
    EXPECT_EQ(single.instance_location, "/0");

    std::string tuple = R"({"items": [)";
    std::string array = "[";
    for (int index = 0; index < 12; ++index)
    {
      tuple += (index == 11) ? "false" : "true, ";
      array += (index == 11) ? "0" : "0, ";
    }
    const auto many = run_schema(tuple + "]}", array + "]");
    EXPECT_EQ(many.schema_location, "#/items/11");
    EXPECT_EQ(many.instance_location, "/11");

    const auto named = run_schema(R"({"properties": {"€": {"not": {}}}})", R"({"€": 1})");
    EXPECT_EQ(named.schema_location, "#/properties/€");
    EXPECT_EQ(named.instance_location, "/€");
  }

  /*
   * Arrange: A registry whose storage ceiling is too small for the text, and one whose
   *          provider fails.
   * Act: Register schema text.
   * Expect: resource_limit and allocation_failure keep their categories, not syntax errors.
   */
  TEST(P044_EdgeCases, RegistrationStorageFailures)
  {
    const std::string text = R"({"properties": {"a": {"items": [true, false, {"not": {}}]}}})";
    {
      schema_test_storage provider;
      accounted_storage storage{provider.callbacks(), 4096U};
      auto registry = schema_registry::create(storage, 1U);
      ASSERT_TRUE(registry.has_value());
      std::string big = "[";
      for (int index = 0; index < 2000; ++index)
      {
        big += "\"abcdefgh\",";
      }
      big += "1]";
      const auto added = registry.value_if()->add({big.data(), big.size()});
      ASSERT_FALSE(added.has_value());
      EXPECT_EQ(added.error_if()->code(), schema_error_code::resource_limit);
    }
    bool saw_allocation_failure = false;
    for (std::size_t fail_on = 2U; fail_on < 40U; ++fail_on)
    {
      schema_test_storage provider;
      provider.fail_on = fail_on;
      accounted_storage storage{provider.callbacks(), 1U << 20U};
      auto registry = schema_registry::create(storage, 1U);
      ASSERT_TRUE(registry.has_value());
      const auto added = registry.value_if()->add({text.data(), text.size()});
      if (added.has_value())
      {
        break;
      }
      EXPECT_EQ(added.error_if()->code(), schema_error_code::allocation_failure) << fail_on;
      saw_allocation_failure = true;
    }
    EXPECT_TRUE(saw_allocation_failure);
  }
  /*
   * Arrange: A $ref chain that enters a cycle not containing its start.
   * Act: Compile.
   * Expect: nonprogressing_reference, found from a node on the cycle.
   */
  TEST(P044_EdgeCases, CycleNotContainingTheStart)
  {
    EXPECT_EQ(compile_error(R"({"definitions": {"a": {"$ref": "#/definitions/b"}, "b": {"$ref": "#/definitions/c"},
                                                "c": {"$ref": "#/definitions/b"}}})"),
              std::optional<schema_error_code>{schema_error_code::nonprogressing_reference});
  }

  /*
   * Arrange: Many failing items in collect-all mode, and the instance-location formatter, each
   *          with allocation failures injected at every request.
   * Act: Validate and format until the operation succeeds.
   * Expect: Every failure is allocation_failure and all storage is released.
   */
  TEST(P046_EdgeCases, ReportAndFormatterAllocationSweeps)
  {
    const std::string schema_text = R"({"properties": {"a/b": {"items": false}}})";
    std::string instance_text = R"({"a/b": [)";
    for (int index = 0; index < 20; ++index)
    {
      instance_text += (index == 19) ? "0" : "0, ";
    }
    instance_text += "]}";

    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    accounted_storage storage{registry_provider.callbacks(), 1U << 20U};
    auto registry = schema_registry::create(storage, 1U);
    ASSERT_TRUE(registry.value_if()->add({schema_text.data(), schema_text.size()}).has_value());
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    const auto compiled =
      compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    ASSERT_TRUE(compiled.has_value());
    schema_test_storage instance_provider;
    accounted_storage accounting{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto instance = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, accounting);
    ASSERT_TRUE(instance.has_value());

    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      schema_test_storage work;
      schema_test_storage report_storage;
      report_storage.fail_on = fail_on;
      validation_options run{};
      run.work_storage = work.callbacks();
      run.report_storage = report_storage.callbacks();
      run.mode = evaluation_mode::collect_all;
      const auto outcome = validate(*compiled.value_if(), instance.value_if()->values, run);
      if (!outcome.has_value())
      {
        ASSERT_EQ(outcome.error_if()->code(), schema_error_code::allocation_failure) << fail_on;
        ASSERT_LT(fail_on, 50U);
        continue;
      }
      ASSERT_EQ(outcome.value_if()->diagnostic_count(), 20U);
      for (std::size_t format_fail = 1U;; ++format_fail)
      {
        schema_test_storage pointer_scratch;
        pointer_scratch.fail_on = format_fail;
        char buffer[64] = {};
        const auto pointer = format_instance_location(instance.value_if()->values, *outcome.value_if()->diagnostic(19U),
                                                      buffer, sizeof(buffer), pointer_scratch.callbacks());
        if (pointer.has_value())
        {
          EXPECT_EQ(std::string(buffer, *pointer.value_if()), "/a~1b/19");
          break;
        }
        ASSERT_EQ(pointer.error_if()->code(), schema_error_code::allocation_failure) << format_fail;
        ASSERT_LT(format_fail, 50U);
      }
      break;
    }
  }

  /*
   * Arrange: URI resolution with a scratch or output provider that fails at each request.
   * Act: Resolve a reference that needs a merged path.
   * Expect: allocation_failure until enough storage is available.
   */
  TEST(P044_EdgeCases, UriAllocationSweep)
  {
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      schema_test_storage storage;
      storage.fail_on = fail_on;
      pod_vector<char> out{storage.callbacks()};
      const auto resolved = resolve_uri("http://user@Example.com/a/b/c?q", "../d/./e?x#f", out, storage.callbacks());
      if (resolved.has_value())
      {
        EXPECT_EQ(std::string(text_of(out, 0U, out.size())), "http://user@example.com/a/d/e?x#f");
        break;
      }
      ASSERT_EQ(resolved.error_if()->code(), schema_error_code::allocation_failure) << fail_on;
      ASSERT_LT(fail_on, 50U);
    }
  }
  /*
   * Arrange: An empty $ref (the current document), an empty tuple items, and an astral
   *          pointer token that is longer than every candidate name.
   * Act: Compile.
   * Expect: The empty reference resolves to the root; an empty items array fails the Draft-07
   *         meta-schema (schemaArray minItems 1); the astral token resolves nothing.
   */
  TEST(P044_EdgeCases, EmptyReferencesAndShortNames)
  {
    EXPECT_EQ(compile_error(R"({"items": [true], "properties": {"a": {"not": {"$ref": ""}}}})"), std::nullopt);
    EXPECT_EQ(compile_error(R"({"items": []})"), std::optional<schema_error_code>{schema_error_code::invalid_schema});
    EXPECT_EQ(compile_error(R"({"definitions": {"a": true, "": true}, "not": {"$ref": "#/definitions/%F0%9F%98%80"}})"),
              std::optional<schema_error_code>{schema_error_code::unresolved_reference});
    EXPECT_EQ(compile_error(R"({"definitions": {"😀x": true}, "not": {"$ref": "#/definitions/%F0%9F%98%80"}})"),
              std::optional<schema_error_code>{schema_error_code::unresolved_reference});
  }

  /*
   * Arrange: A failure under a property name.
   * Act: Format both locations into a zero-capacity buffer.
   * Expect: Only the needed sizes are reported; nothing is written.
   */
  TEST(P046_EdgeCases, ZeroCapacityFormatting)
  {
    const auto summary = run_schema(R"({"properties": {"name": {"items": false}}})", R"({"name": [1]})");
    EXPECT_EQ(summary.schema_location, "#/properties/name/items");
    EXPECT_EQ(summary.instance_location, "/name/0");

    schema_test_storage registry_provider;
    schema_test_storage result_provider;
    schema_test_storage scratch_provider;
    accounted_storage storage{registry_provider.callbacks(), 1U << 20U};
    auto registry = schema_registry::create(storage, 1U);
    const std::string text = R"({"properties": {"name": {"items": false}}})";
    ASSERT_TRUE(registry.value_if()->add({text.data(), text.size()}).has_value());
    compile_options options{};
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    const auto compiled =
      compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    ASSERT_TRUE(compiled.has_value());
    schema_test_storage instance_provider;
    const std::string instance_text = R"({"name": [1]})";
    accounted_storage accounting{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto instance = parse_document({instance_text.data(), instance_text.size()}, parse_options{}, accounting);
    ASSERT_TRUE(instance.has_value());
    schema_test_storage work;
    schema_test_storage report_storage;
    validation_options run{};
    run.work_storage = work.callbacks();
    run.report_storage = report_storage.callbacks();
    const auto outcome = validate(*compiled.value_if(), instance.value_if()->values, run);
    ASSERT_TRUE(outcome.has_value());
    const auto& diagnostic = *outcome.value_if()->diagnostic(0U);
    char guard = 'x';
    EXPECT_EQ(format_schema_location(*compiled.value_if(), diagnostic, &guard, 0U), 23U);
    schema_test_storage pointer_scratch;
    const auto pointer =
      format_instance_location(instance.value_if()->values, diagnostic, &guard, 0U, pointer_scratch.callbacks());
    ASSERT_TRUE(pointer.has_value());
    EXPECT_EQ(*pointer.value_if(), 7U);
    EXPECT_EQ(guard, 'x');
  }
  /*
   * Arrange: References whose output grows (8, then 16 characters) exactly where the
   *          authority, the query or the fragment starts, with the growth failing.
   * Act: Resolve them with separate output and scratch storage.
   * Expect: allocation_failure at each component boundary; nothing is leaked.
   */
  TEST(P044_EdgeCases, UriComponentBoundaryFailures)
  {
    for (const char* reference :
         {"abcdefg://h/p", "http:/ab?q", "http:/abcdefghij#f", "http:/abcdefghijk?q#f", "abcdefghijklmno://h"})
    {
      schema_test_storage out_storage;
      schema_test_storage scratch;
      out_storage.fail_on = 2U;
      pod_vector<char> out{out_storage.callbacks()};
      const auto resolved = resolve_uri("http://base/", reference, out, scratch.callbacks());
      ASSERT_FALSE(resolved.has_value()) << reference;
      EXPECT_EQ(resolved.error_if()->code(), schema_error_code::allocation_failure) << reference;
    }
  }
} // namespace
