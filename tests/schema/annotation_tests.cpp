#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema.hpp"
#include "schema_test_storage.hpp"

#include <algorithm>
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
   * @brief Convert an ASCII decoded string to std::string for comparisons.
   * @par Requirements
   * P053.
   */
  std::string ascii(const decoded_string& text)
  {
    std::string out;
    for (std::size_t index = 0U; index < text.size(); ++index)
    {
      out.push_back(static_cast<char>(*text.unit_at(index).value_if()));
    }
    return out;
  }

  /**
   * @brief A compiled schema, a parsed instance and the last validation outcome.
   * @par Requirements
   * P053; J5-058.
   */
  struct annotated_run
  {
      schema_test_storage result_provider;
      schema_test_storage instance_provider;
      std::string instance_text;
      std::optional<compiled_schema> compiled;
      std::optional<accounted_storage> accounting;
      std::optional<parsed_document> instance;

      annotated_run(const std::string& schema_text, std::string instance_input)
          : instance_text(std::move(instance_input))
      {
        schema_test_storage registry_provider;
        schema_test_storage scratch_provider;
        accounted_storage storage{registry_provider.callbacks(), 1U << 22U};
        auto registry = schema_registry::create(storage, 1U);
        EXPECT_TRUE(registry.value_if()->add({schema_text.data(), schema_text.size()}).has_value()) << schema_text;
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

      /**
       * @brief Validate and describe each annotation as "<instance pointer>:<member names>".
       * @par Requirements
       * P053; J5-058; J5-064.
       */
      std::optional<std::vector<std::string>> annotations(evaluation_mode mode = evaluation_mode::collect_all,
                                                          bool* valid = nullptr)
      {
        schema_test_storage work;
        schema_test_storage report_storage;
        validation_options options{};
        options.work_storage = work.callbacks();
        options.report_storage = report_storage.callbacks();
        options.mode = mode;
        options.collect_annotations = true;
        const auto report = validate(*compiled, instance->values, options);
        EXPECT_EQ(work.live_bytes, 0U);
        if (!report.has_value())
        {
          return std::nullopt;
        }
        if (valid != nullptr)
        {
          *valid = report.value_if()->valid();
        }
        std::vector<std::string> out;
        for (std::size_t index = 0U; index < report.value_if()->annotation_count(); ++index)
        {
          const schema_annotation& record = *report.value_if()->annotation(index);
          EXPECT_EQ(record.keyword, schema_keyword::none);
          char pointer[64] = {};
          schema_test_storage scratch;
          const auto size =
            format_instance_location(instance->values, record, pointer, sizeof(pointer), scratch.callbacks());
          EXPECT_TRUE(size.has_value());
          std::string line{pointer, size.has_value() ? *size.value_if() : 0U};
          line += ':';
          const auto values = annotation_values(*compiled, record);
          EXPECT_TRUE(values.has_value());
          auto members = values.value_if()->children();
          std::vector<std::string> names;
          for (;;)
          {
            const auto member = members.value_if()->next();
            if (!member.has_value() || !member.value_if()->has_value())
            {
              break;
            }
            names.push_back(ascii(**(*member.value_if())->member_name().value_if()));
          }
          std::sort(names.begin(), names.end());
          for (const std::string& name : names)
          {
            line += name + ',';
          }
          out.push_back(line);
        }
        std::sort(out.begin(), out.end());
        return out;
      }
  };

  using lines = std::vector<std::string>;

  /**
   * @brief Annotations of a valid collect-all run.
   * @par Requirements
   * P053; J5-058.
   */
  lines annotations_of(const char* schema, const char* instance)
  {
    annotated_run run{schema, instance};
    bool valid = false;
    const auto found = run.annotations(evaluation_mode::collect_all, &valid);
    EXPECT_TRUE(found.has_value()) << schema;
    return found.value_or(lines{});
  }

  /* ================================== Test_P053_Annotations ================================== */
  /*
   * Arrange: Annotation keywords at the root and in properties/items subschemas.
   * Act: Validate with collect_annotations.
   * Expect: One record per annotated schema node and instance value, located by instance
   *         pointer, with the node's annotation keywords (J5-058, J5-064).
   */
  TEST(P053_Annotations, CollectsEveryStandardKeyword)
  {
    EXPECT_EQ(annotations_of(R"({"title": "T", "description": "D", "default": {}, "examples": [1], "readOnly": true,
                               "writeOnly": false, "$comment": "c", "format": "email", "contentEncoding": "base64",
                               "contentMediaType": "text/plain", "type": "object"})",
                             "{}"),
              (lines{":$comment,contentEncoding,contentMediaType,default,description,examples,format,readOnly,"
                     "title,writeOnly,"}));
    EXPECT_EQ(
      annotations_of(R"({"properties": {"a": {"title": "A", "items": {"description": "I"}}}})", R"({"a": [1, 2]})"),
      (lines{"/a/0:description,", "/a/1:description,", "/a:title,"}));
  }

  /*
   * Arrange: Annotated branches that pass and fail inside anyOf, oneOf, allOf and if/then/else.
   * Act: Validate with collect_annotations.
   * Expect: Only passing branches contribute; every passing anyOf branch is evaluated; the
   *         untaken then/else branch contributes nothing (J5-058).
   */
  TEST(P053_Annotations, OnlyPassingBranchesContribute)
  {
    EXPECT_EQ(
      annotations_of(R"({"anyOf": [{"title": "s", "type": "string"}, {"description": "n"}, {"$comment": "x"}]})", "1"),
      (lines{":$comment,", ":description,"}));
    EXPECT_EQ(annotations_of(R"({"oneOf": [{"title": "s", "type": "string"}, {"description": "n"}]})", "1"),
              (lines{":description,"}));
    EXPECT_EQ(annotations_of(R"({"allOf": [{"title": "a"}, {"description": "b"}]})", "1"),
              (lines{":description,", ":title,"}));
    EXPECT_EQ(annotations_of(R"({"if": {"title": "cond", "type": "number"}, "then": {"description": "then"},
                               "else": {"$comment": "else"}})",
                             "1"),
              (lines{":description,", ":title,"}));
    EXPECT_EQ(annotations_of(R"({"if": {"title": "cond", "type": "number"}, "then": {"description": "then"},
                               "else": {"$comment": "else"}})",
                             R"("x")"),
              (lines{":$comment,"}));
    EXPECT_EQ(annotations_of(R"({"contains": {"title": "c", "minimum": 5}})", "[1, 7]"), (lines{"/1:title,"}));
  }

  /*
   * Arrange: Annotations inside not, propertyNames and beside $ref.
   * Act: Validate with collect_annotations.
   * Expect: not and propertyNames never contribute; $ref siblings are ignored (J5-058).
   */
  TEST(P053_Annotations, SuppressedContexts)
  {
    EXPECT_EQ(annotations_of(R"({"not": {"title": "x", "type": "string"}})", "1"), lines{});
    EXPECT_EQ(annotations_of(R"({"not": {"not": {"title": "x"}}})", "1"), lines{});
    EXPECT_EQ(annotations_of(R"({"propertyNames": {"title": "n"}})", R"({"a": 1})"), lines{});
    EXPECT_EQ(annotations_of(R"({"definitions": {"d": {"title": "D"}},
                               "properties": {"a": {"$ref": "#/definitions/d", "description": "ignored"}}})",
                             R"({"a": 1})"),
              (lines{"/a:title,"}));
  }

  /*
   * Arrange: An invalid instance, a run without collect_annotations and fail-fast mode.
   * Act: Validate.
   * Expect: An invalid report has no annotations; without the option none are kept; fail-fast
   *         and collect-all agree on valid instances (J5-058, J5-063).
   */
  TEST(P053_Annotations, ValidReportsOnly)
  {
    annotated_run invalid{R"({"title": "T", "properties": {"a": {"description": "A", "type": "string"}}})",
                          R"({"a": 1})"};
    bool valid = true;
    EXPECT_EQ(invalid.annotations(evaluation_mode::collect_all, &valid), std::optional<lines>{lines{}});
    EXPECT_FALSE(valid);

    annotated_run fast{R"({"title": "T", "anyOf": [{"description": "A"}, {"$comment": "B"}]})", "1"};
    EXPECT_EQ(fast.annotations(evaluation_mode::fail_fast), fast.annotations(evaluation_mode::collect_all));

    schema_test_storage work;
    schema_test_storage report_storage;
    validation_options options{};
    options.work_storage = work.callbacks();
    options.report_storage = report_storage.callbacks();
    const auto report = validate(*fast.compiled, fast.instance->values, options);
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(report.value_if()->valid());
    EXPECT_EQ(report.value_if()->annotation_count(), 0U);
    EXPECT_EQ(report.value_if()->annotation(0U), nullptr);
  }

  /*
   * Arrange: default and examples for missing members, readOnly on a present member.
   * Act: Validate with collect_annotations.
   * Expect: The instance is unchanged: no member is inserted and no value converted; properties
   *         annotations apply only to present members (J5-058).
   */
  TEST(P053_Annotations, NeverMutatesTheInstance)
  {
    annotated_run run{R"({"properties": {"a": {"readOnly": true}, "b": {"default": 7, "examples": [7]}},
                          "required": ["a"]})",
                      R"({"a": "1"})"};
    const auto before = run.instance->values.root().value_if()->container_size();
    EXPECT_EQ(run.annotations(), std::optional<lines>{lines{"/a:readOnly,"}});
    const auto after = run.instance->values.root().value_if()->container_size();
    ASSERT_TRUE(before.has_value());
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(*before.value_if(), *after.value_if());
    const auto a = run.instance->values.root().value_if()->find_member("a");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(*a.value_if()->kind().value_if(), value_kind::string);
    EXPECT_FALSE(run.instance->values.root().value_if()->find_member("b").has_value());
  }

  /* ============================== Test_P053_ExtensionKeywords ============================== */
  /*
   * Arrange: A custom keyword, a misspelled known keyword and additionalProperties: false.
   * Act: Validate instances with and without an instance property of the same name.
   * Expect: Unknown schema keywords are annotations, never assertions; an unknown instance
   *         property is still rejected by additionalProperties (J5-062).
   */
  TEST(P053_ExtensionKeywords, AreNotInstanceProperties)
  {
    constexpr const char* c_schema =
      R"({"x-custom": {"any": "value"}, "maximun": 5, "properties": {"a": true}, "additionalProperties": false})";
    EXPECT_EQ(annotations_of(c_schema, R"({"a": 10})"), (lines{":maximun,x-custom,"}));
    annotated_run unknown_property{c_schema, R"({"x-custom": 1})"};
    bool valid = true;
    EXPECT_EQ(unknown_property.annotations(evaluation_mode::collect_all, &valid), std::optional<lines>{lines{}});
    EXPECT_FALSE(valid);
  }

  /*
   * Arrange: format and content keywords with non-matching instances.
   * Act: Validate without any assertion option.
   * Expect: They are annotations only and never fail validation (J5-069).
   */
  TEST(P053_ExtensionKeywords, FormatAndContentAreAnnotations)
  {
    EXPECT_EQ(
      annotations_of(R"({"format": "email", "contentEncoding": "base64", "contentMediaType": "application/json"})",
                     R"("not an email, not base64 {")"),
      (lines{":contentEncoding,contentMediaType,format,"}));
    EXPECT_EQ(annotations_of(R"({"format": "no-such-format"})", "12"), (lines{":format,"}));
  }

  /* ================================ Test_P053_Failures ================================ */
  /*
   * Arrange: An annotated schema, with report-storage allocation n failing.
   * Act: Validate with collect_annotations for n = 1, 2, ... until it succeeds.
   * Expect: allocation_failure with nothing allocated, then the complete annotation list
   *         (J5-046, J5-058).
   */
  TEST(P053_Failures, ReportAllocationFailureSweep)
  {
    annotated_run run{R"({"title": "T", "items": {"description": "I", "anyOf": [{"$comment": "c"}, true]}})",
                      "[1, 2, 3]"};
    for (std::size_t fail_on = 1U;; ++fail_on)
    {
      schema_test_storage work;
      schema_test_storage report_storage;
      report_storage.fail_on = fail_on;
      validation_options options{};
      options.work_storage = work.callbacks();
      options.report_storage = report_storage.callbacks();
      options.collect_annotations = true;
      const auto report = validate(*run.compiled, run.instance->values, options);
      EXPECT_EQ(work.live_bytes, 0U);
      if (report.has_value())
      {
        EXPECT_EQ(report.value_if()->annotation_count(), 7U);
        break;
      }
      ASSERT_EQ(report.error_if()->code(), schema_error_code::allocation_failure) << fail_on;
      ASSERT_LT(fail_on, 100U);
    }
  }

  /*
   * Arrange: Records that name no annotated node.
   * Act: Read their values.
   * Expect: internal_error, never a view (J5-063).
   */
  TEST(P053_Failures, ForeignRecordsAreRejected)
  {
    annotated_run run{R"({"title": "T", "properties": {"a": true}})", R"({"a": 1})"};
    schema_annotation record{};
    record.schema_node = 1000000U;
    EXPECT_FALSE(annotation_values(*run.compiled, record).has_value());
    record.schema_node = 1U; // properties/a: no annotation keywords.
    EXPECT_FALSE(annotation_values(*run.compiled, record).has_value());
  }
} // namespace
