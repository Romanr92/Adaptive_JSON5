#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "parser/parser_attachment.hpp"
#include "parser/parser_duplicate.hpp"
#include "parser/parser_engine.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <tuple>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;
  /* =================================== Test_J5_032 ==================================== */
  /**
   * @brief Independent source examples and duplicate diagnostics for P036.
   * @par Requirements
   * P036; J5-032; J5-033.
   */
  struct duplicate_case
  {
      const char* name;     // Input case identifier.
      const char* input;    // Input bytes containing one complete root object or array.
      syntax_mode mode;     // Input syntax selection.
      bool duplicate;       // Expected decoded-name collision within one object.
      source_span first;    // Expected first occurrence in original byte units.
      source_span repeated; // Expected rejected occurrence in original byte units.
      std::size_t records;  // Expected retained records when preservation succeeds.
      std::size_t children; // Expected direct root children when preservation succeeds.
  };
  class J5_032_DuplicatePolicy : public testing::TestWithParam<std::tuple<duplicate_case, bool>>
  {
  };
  /*
   * Arrange: Independently specified names, scopes, spans and both duplicate policies.
   * Act: Parse through accounted storage, then overwrite the source on rejection.
   * Expect: Preservation retains all records; rejection returns both exact source spans.
   */
  TEST_P(J5_032_DuplicatePolicy, AppliesDecodedEqualityWithinEachObject)
  {
    /* Arrange */
    const auto fixture = std::get<0>(GetParam());
    const bool reject = std::get<1>(GetParam());
    std::string input = fixture.input;
    document_provider provider;
    parse_options options{fixture.mode};
    options.duplicate_names = reject ? duplicate_name_policy::reject_duplicates : duplicate_name_policy::preserve_all;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    {
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      /* Expect */
      // A duplicate in a nested object must report original document coordinates, not substring offsets.
      if (reject && fixture.duplicate)
      {
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.value_if(), nullptr);
        const auto& error = *parsed.error_if();
        EXPECT_EQ(error.code, error_code::duplicate_policy);
        EXPECT_TRUE(error.has_byte_offset);
        EXPECT_EQ(error.byte_offset, fixture.repeated.byte_offset);
        ASSERT_TRUE(error.duplicate_names.has_value());
        std::fill(input.begin(), input.end(), '?');
        EXPECT_EQ(error.duplicate_names->first.byte_offset, fixture.first.byte_offset);
        EXPECT_EQ(error.duplicate_names->first.byte_length, fixture.first.byte_length);
        EXPECT_EQ(error.duplicate_names->repeated.byte_offset, fixture.repeated.byte_offset);
        EXPECT_EQ(error.duplicate_names->repeated.byte_length, fixture.repeated.byte_length);
      }
      else
      {
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(parsed.value_if()->consumed_bytes, input.size());
        EXPECT_EQ(parsed.value_if()->values.size(), fixture.records);
        const auto root = parsed.value_if()->values.root();
        ASSERT_TRUE(root.has_value());
        const auto count = root.value_if()->container_size();
        ASSERT_TRUE(count.has_value());
        EXPECT_EQ(*count.value_if(), fixture.children);
      }
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  INSTANTIATE_TEST_SUITE_P(
    P036, J5_032_DuplicatePolicy,
    testing::Combine(
      testing::Values(
        duplicate_case{"QuotedJson5", "{\"a\":1,\"a\":2}", syntax_mode::json5, true, {1, 3}, {7, 3}, 3, 2},
        duplicate_case{"EmptyNameJson5", "{\"\":1,\"\":2}", syntax_mode::json5, true, {1, 2}, {6, 2}, 3, 2},
        duplicate_case{"QuotedEscapeJson5", "{\"a\":1,\"\\u0061\":2}", syntax_mode::json5, true, {1, 3}, {7, 8}, 3, 2},
        duplicate_case{
          "EmbeddedZeroJson5", "{\"a\\u0000b\":1,\"a\\u0000b\":2}", syntax_mode::json5, true, {1, 10}, {14, 10}, 3, 2},
        duplicate_case{
          "SurrogateUnitJson5", "{\"\\uD800\":1,\"\\uD800\":2}", syntax_mode::json5, true, {1, 8}, {12, 8}, 3, 2},
        duplicate_case{
          "NestedDuplicateJson5", "{\"outer\":{\"a\":1,\"a\":2}}", syntax_mode::json5, true, {10, 3}, {16, 3}, 4, 1},
        duplicate_case{
          "AfterNestedValueJson5", "{\"a\":[{}],\"a\":2}", syntax_mode::json5, true, {1, 3}, {10, 3}, 4, 2},
        duplicate_case{"CaseDistinctJson5", "{\"a\":1,\"A\":2}", syntax_mode::json5, false, {0, 0}, {0, 0}, 3, 2},
        duplicate_case{"NormalizationDistinctJson5",
                       "{\"\\u00E9\":1,\"e\\u0301\":2}",
                       syntax_mode::json5,
                       false,
                       {0, 0},
                       {0, 0},
                       3,
                       2},
        duplicate_case{
          "SeparateObjectsJson5", "[{\"a\":1},{\"a\":2}]", syntax_mode::json5, false, {0, 0}, {0, 0}, 5, 2},
        duplicate_case{"ParentAndChildJson5", "{\"a\":{\"a\":1}}", syntax_mode::json5, false, {0, 0}, {0, 0}, 3, 1},
        duplicate_case{"DifferentLengthsJson5", "{\"a\":1,\"aa\":2}", syntax_mode::json5, false, {0, 0}, {0, 0}, 3, 2},
        duplicate_case{"QuotedStrict", "{\"a\":1,\"a\":2}", syntax_mode::strict_json, true, {1, 3}, {7, 3}, 3, 2},
        duplicate_case{"EmptyNameStrict", "{\"\":1,\"\":2}", syntax_mode::strict_json, true, {1, 2}, {6, 2}, 3, 2},
        duplicate_case{
          "QuotedEscapeStrict", "{\"a\":1,\"\\u0061\":2}", syntax_mode::strict_json, true, {1, 3}, {7, 8}, 3, 2},
        duplicate_case{"EmbeddedZeroStrict",
                       "{\"a\\u0000b\":1,\"a\\u0000b\":2}",
                       syntax_mode::strict_json,
                       true,
                       {1, 10},
                       {14, 10},
                       3,
                       2},
        duplicate_case{"SurrogateUnitStrict",
                       "{\"\\uD800\":1,\"\\uD800\":2}",
                       syntax_mode::strict_json,
                       true,
                       {1, 8},
                       {12, 8},
                       3,
                       2},
        duplicate_case{"NestedDuplicateStrict",
                       "{\"outer\":{\"a\":1,\"a\":2}}",
                       syntax_mode::strict_json,
                       true,
                       {10, 3},
                       {16, 3},
                       4,
                       1},
        duplicate_case{
          "AfterNestedValueStrict", "{\"a\":[{}],\"a\":2}", syntax_mode::strict_json, true, {1, 3}, {10, 3}, 4, 2},
        duplicate_case{
          "CaseDistinctStrict", "{\"a\":1,\"A\":2}", syntax_mode::strict_json, false, {0, 0}, {0, 0}, 3, 2},
        duplicate_case{"NormalizationDistinctStrict",
                       "{\"\\u00E9\":1,\"e\\u0301\":2}",
                       syntax_mode::strict_json,
                       false,
                       {0, 0},
                       {0, 0},
                       3,
                       2},
        duplicate_case{
          "SeparateObjectsStrict", "[{\"a\":1},{\"a\":2}]", syntax_mode::strict_json, false, {0, 0}, {0, 0}, 5, 2},
        duplicate_case{
          "ParentAndChildStrict", "{\"a\":{\"a\":1}}", syntax_mode::strict_json, false, {0, 0}, {0, 0}, 3, 1},
        duplicate_case{
          "DifferentLengthsStrict", "{\"a\":1,\"aa\":2}", syntax_mode::strict_json, false, {0, 0}, {0, 0}, 3, 2},
        duplicate_case{"BareQuoted", "{a:1,\"a\":2}", syntax_mode::json5, true, {1, 1}, {5, 3}, 3, 2},
        duplicate_case{"EscapedIdentifier", "{a:1,\\u0061:2}", syntax_mode::json5, true, {1, 1}, {5, 6}, 3, 2},
        duplicate_case{"ReservedWord", "{true:1,\"true\":2}", syntax_mode::json5, true, {1, 4}, {8, 6}, 3, 2},
        duplicate_case{"TriviaOffsets", "{/*x*/ a:1, /*y*/ \"a\":2}", syntax_mode::json5, true, {7, 1}, {18, 3}, 3, 2}),
      testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<duplicate_case, bool>>& info)
    {
      return std::string(std::get<0>(info.param).name) + (std::get<1>(info.param) ? "Reject" : "Preserve");
    });
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Duplicate source and default options, with no policy assignment.
   * Act: Parse and enumerate the two equal decoded names and their original spans.
   * Expect: Default behavior keeps both distinct values in source order.
   */
  TEST(J5_032_DuplicateDefault, PreservesEveryMemberAndNameSpan)
  {
    /* Arrange */
    const std::string input = "{a:true,\"a\":false}";
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto count = root.value_if()->container_size();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count.value_if(), 2u);
    // Distinct Boolean values detect silent first-wins or last-wins replacement.
    for (std::size_t i = 0; i < 2; ++i)
    {
      const auto member = root.value_if()->member_at(i);
      ASSERT_TRUE(member.has_value());
      const auto boolean = member.value_if()->as_boolean();
      ASSERT_TRUE(boolean.has_value());
      EXPECT_EQ(*boolean.value_if(), i == 0);
      const auto span = member.value_if()->member_name_source_span();
      ASSERT_TRUE(span.has_value());
      ASSERT_TRUE(span.value_if()->has_value());
      EXPECT_EQ((**span.value_if()).byte_offset, i == 0 ? 1u : 8u);
      EXPECT_EQ((**span.value_if()).byte_length, i == 0 ? 1u : 3u);
    }
  }
  /* ================================= End Test_J5_032 ================================== */

  /* =================================== Test_J5_049 ==================================== */
  class J5_049_DuplicateWork : public testing::TestWithParam<std::size_t>
  {
  };
  /*
   * Arrange: One stored two-unit name and a duplicate candidate, with explicit work allowance.
   * Act: Run the checker without decoding or allocating any new names.
   * Expect: Allowances below six fail as resource limits; six admits exact duplicate reporting.
   */
  TEST_P(J5_049_DuplicateWork, ChargesBeforeInspectionAndComparison)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto object = values.make_object();
    const auto child = values.make_null();
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(child.has_value());
    auto name = decoded_string::copy_units(provider.callbacks(), u"ab", 2);
    ASSERT_TRUE(name.has_value());
    ASSERT_TRUE(
      values.append_member(*object.value_if(), std::move(*name.value_if()), *child.value_if(), {1, 4}, 20).has_value());
    auto candidate = decoded_string::copy_units(provider.callbacks(), u"ab", 2);
    ASSERT_TRUE(candidate.has_value());
    work_budget work{GetParam()};
    const auto attempts = provider.m_attempts;
    /* Act */
    const auto checked = reject_duplicate_name(values, *object.value_if(), *candidate.value_if(), {10, 4}, work);
    /* Expect */
    // Two unit reads per position must be admitted before any equal-name scan.
    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error_if()->code, GetParam() < 6 ? error_code::resource_limit : error_code::duplicate_policy);
    EXPECT_LE(work.consumed(), GetParam());
    EXPECT_EQ(provider.m_attempts, attempts);
    EXPECT_EQ(candidate.value_if()->size(), 2u);
    if (GetParam() >= 6)
    {
      ASSERT_TRUE(checked.error_if()->duplicate_names.has_value());
      EXPECT_EQ(checked.error_if()->duplicate_names->first.byte_offset, 1u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(P036, J5_049_DuplicateWork, testing::Values(0u, 1u, 2u, 5u, 6u, 7u),
                           [](const testing::TestParamInfo<std::size_t>& info)
                           {
                             return "Units" + std::to_string(info.param);
                           });
  /* ================================= End Test_J5_049 ================================== */

  /* =================================== Test_J5_046 ==================================== */
  /*
   * Arrange: Duplicate rejection that decodes and retains names in nested objects.
   * Act: Fail each observed allocation, then retry with a fresh engine and the same provider.
   * Expect: Explicit allocation failures, full cleanup, and repeatable two-location duplicate errors.
   */
  TEST(J5_046_DuplicateAllocation, FailsEachAllocationAndAllowsFreshEngine)
  {
    /* Arrange */
    const std::string input = "{\"outer\":{\"long_name\":[1],\"long_name\":2}}";
    parse_options options;
    options.duplicate_names = duplicate_name_policy::reject_duplicates;
    std::size_t attempts = 0;
    {
      document_provider provider;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      ASSERT_FALSE(parsed.has_value());
      ASSERT_EQ(parsed.error_if()->code, error_code::duplicate_policy);
      attempts = provider.m_attempts;
    }
    ASSERT_GT(attempts, 0u);
    /* Act */
    for (std::size_t failure = 1; failure <= attempts; ++failure)
    {
      SCOPED_TRACE(failure);
      document_provider provider;
      provider.m_fail_on = failure;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      {
        parser_engine engine{{input.data(), input.size()}, options, accounting};
        const auto parsed = engine.run();
        /* Expect */
        // Allocation failure is operational; it must not be relabeled as duplicate rejection.
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::allocation_failure);
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
      provider.m_fail_on = 0;
      {
        parser_engine retry{{input.data(), input.size()}, options, accounting};
        const auto parsed = retry.run();
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::duplicate_policy);
        ASSERT_TRUE(parsed.error_if()->duplicate_names.has_value());
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
  }
  /* ================================= End Test_J5_046 ================================== */

  /* =================================== Test_J5_039 ==================================== */
  /*
   * Arrange: Invalid/nonobject indices and a matching manually named member without provenance.
   * Act: Invoke the internal checker with otherwise sufficient work.
   * Expect: Access failures remain distinct from duplicates and do not invent either location.
   */
  TEST(J5_039_DuplicateInspection, RejectsInvalidObjectsAndMissingProvenance)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto object = values.make_object();
    const auto array = values.make_array();
    const auto scalar = values.make_null();
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(array.has_value());
    ASSERT_TRUE(scalar.has_value());
    auto stored = decoded_string::copy_units(provider.callbacks(), u"a", 1);
    ASSERT_TRUE(stored.has_value());
    ASSERT_TRUE(
      values.append_member(*object.value_if(), std::move(*stored.value_if()), *scalar.value_if()).has_value());
    auto name = decoded_string::copy_units(provider.callbacks(), u"a", 1);
    ASSERT_TRUE(name.has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    for (const value_index index :
         {value_index{}, value_index{99}, *array.value_if(), *scalar.value_if(), *object.value_if()})
    {
      work_budget work{100};
      const auto checked = reject_duplicate_name(values, index, *name.value_if(), {10, 3}, work);
      /* Expect */
      // A successful duplicate diagnostic must never substitute byte zero for missing provenance.
      ASSERT_FALSE(checked.has_value());
      EXPECT_EQ(checked.error_if()->code, error_code::wrong_type_or_access);
      EXPECT_FALSE(checked.error_if()->duplicate_names.has_value());
    }
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ================================= End Test_J5_039 ================================== */

  /* =================================== Test_J5_051 ==================================== */
  /*
   * Arrange: An object frame with an owned pending name and an out-of-input name span.
   * Act: Reject attachment, correct the original input size, then retry the same child.
   * Expect: Failure retains ownership and phase; success stores the span and clears pending state.
   */
  TEST(J5_051_MemberSpanAttachment, PreservesFrameAndAllowsRetryAfterRangeFailure)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 8};
    const auto object = values.make_object();
    const auto child = values.make_null();
    ASSERT_TRUE(object.has_value());
    ASSERT_TRUE(child.has_value());
    auto name = decoded_string::copy_units(provider.callbacks(), u"a", 1);
    ASSERT_TRUE(name.has_value());
    parser_frame frame;
    frame.container = *object.value_if();
    frame.state = parser_state::object_value;
    frame.pending_name.emplace(std::move(*name.value_if()));
    frame.name_offset = 8;
    frame.name_length = 3;
    work_budget work{100};
    /* Act */
    const auto rejected = attach_parsed_value(values, frame, *child.value_if(), work, 10);
    /* Expect */
    // Failed span validation must not consume the pending name or publish comma-or-end state.
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(frame.pending_name.has_value());
    EXPECT_EQ(frame.pending_name->size(), 1u);
    EXPECT_EQ(frame.state, parser_state::object_value);
    EXPECT_EQ(frame.name_offset, 8u);
    EXPECT_EQ(frame.name_length, 3u);
    ASSERT_TRUE(attach_parsed_value(values, frame, *child.value_if(), work, 11).has_value());
    EXPECT_FALSE(frame.pending_name.has_value());
    EXPECT_EQ(frame.name_offset, 0u);
    EXPECT_EQ(frame.name_length, 0u);
    EXPECT_EQ(frame.state, parser_state::object_comma_or_end);
    const auto member = values.view(*child.value_if());
    ASSERT_TRUE(member.has_value());
    const auto span = member.value_if()->member_name_source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ((**span.value_if()).byte_offset, 8u);
    EXPECT_EQ((**span.value_if()).byte_length, 3u);
  }
  /* ================================= End Test_J5_051 ================================== */
} // namespace
