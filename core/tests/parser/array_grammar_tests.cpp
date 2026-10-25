#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "parser/parser_engine.hpp"

#include <gtest/gtest.h>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_013 ==================================== */
  /**
   * @brief Describe independent array grammar expectations in both syntax modes.
   * @par Requirements
   * P034; J5-013.
   */
  struct array_case
  {
      const char* name;     // Input case name for generated test identification.
      const char* input;    // Input bytes containing an array candidate.
      bool accepted;        // Expected success in the selected mode.
      std::size_t position; // Expected error byte offset, or direct element count on success.
      syntax_mode mode;     // Input syntax selection.
  };
  class J5_013_ArrayGrammar : public testing::TestWithParam<array_case>
  {
  };

  /*
   * Isolate array punctuation from unrelated object-name and scalar extensions.
   * Arrange: A named array candidate with an explicit mode and independent expectation.
   * Act: Run a fresh internal engine and destroy it before checking storage balance.
   * Expect: Exact element count and span, or located syntax failure without a partial tree.
   */
  TEST_P(J5_013_ArrayGrammar, EnforcesElementAndSeparatorRules)
  {
    /* Arrange */
    const auto fixture = GetParam();
    const std::string input = fixture.input;
    document_provider provider;
    parse_options options{fixture.mode};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    {
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      /* Expect */
      // Independent expected offsets distinguish a forbidden comma from a later closer.
      ASSERT_EQ(parsed.has_value(), fixture.accepted) << input;
      if (fixture.accepted)
      {
        EXPECT_EQ(parsed.value_if()->consumed_bytes, input.size());
        const auto root = parsed.value_if()->values.root();
        ASSERT_TRUE(root.has_value());
        const auto count = root.value_if()->container_size();
        ASSERT_TRUE(count.has_value());
        EXPECT_EQ(*count.value_if(), fixture.position);
        const auto span = root.value_if()->source_span();
        ASSERT_TRUE(span.has_value());
        ASSERT_TRUE(span.value_if()->has_value());
        EXPECT_EQ((**span.value_if()).byte_offset, 0u);
        EXPECT_EQ((**span.value_if()).byte_length, input.size());
      }
      else
      {
        EXPECT_EQ(parsed.value_if(), nullptr);
        ASSERT_NE(parsed.error_if(), nullptr);
        EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
        EXPECT_TRUE(parsed.error_if()->has_byte_offset);
        EXPECT_EQ(parsed.error_if()->byte_offset, fixture.position);
      }
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  INSTANTIATE_TEST_SUITE_P(
    P034, J5_013_ArrayGrammar,
    testing::Values(array_case{"EmptyJson5", "[]", true, 0, syntax_mode::json5},
                    array_case{"EmptyStrict", "[]", true, 0, syntax_mode::strict_json},
                    array_case{"MixedJson5", "[null,true,2,\"x\",[],{}]", true, 6, syntax_mode::json5},
                    array_case{"MixedStrict", "[null,true,2,\"x\",[],{}]", true, 6, syntax_mode::strict_json},
                    array_case{"TrailingJson5", "[1,]", true, 1, syntax_mode::json5},
                    array_case{"TrailingStrict", "[1,]", false, 3, syntax_mode::strict_json},
                    array_case{"NestedTrailingJson5", "[[1,],2,]", true, 2, syntax_mode::json5},
                    array_case{"NestedTrailingStrict", "[[1,],2,]", false, 4, syntax_mode::strict_json},
                    array_case{"CommentAfterCommaJson5", "[1,/*x*/]", true, 1, syntax_mode::json5},
                    array_case{"CommentAfterCommaStrict", "[1,/*x*/]", false, 3, syntax_mode::strict_json},
                    array_case{"EmptyHoleJson5", "[,]", false, 1, syntax_mode::json5},
                    array_case{"EmptyHoleStrict", "[,]", false, 1, syntax_mode::strict_json},
                    array_case{"DoubleCommaJson5", "[1,,2]", false, 3, syntax_mode::json5},
                    array_case{"DoubleCommaStrict", "[1,,2]", false, 3, syntax_mode::strict_json},
                    array_case{"DoubleTrailingJson5", "[1,,]", false, 3, syntax_mode::json5},
                    array_case{"DoubleTrailingStrict", "[1,,]", false, 3, syntax_mode::strict_json},
                    array_case{"MissingScalarCommaJson5", "[1 2]", false, 3, syntax_mode::json5},
                    array_case{"MissingScalarCommaStrict", "[1 2]", false, 3, syntax_mode::strict_json},
                    array_case{"MissingContainerCommaJson5", "[[]{}]", false, 3, syntax_mode::json5},
                    array_case{"MissingContainerCommaStrict", "[[]{}]", false, 3, syntax_mode::strict_json},
                    array_case{"ColonJson5", "[1:2]", false, 2, syntax_mode::json5},
                    array_case{"ColonStrict", "[1:2]", false, 2, syntax_mode::strict_json},
                    array_case{"MissingValueJson5", "[1,", false, 3, syntax_mode::json5},
                    array_case{"MissingValueStrict", "[1,", false, 3, syntax_mode::strict_json},
                    array_case{"NestedHoleJson5", "[[,]]", false, 2, syntax_mode::json5},
                    array_case{"NestedHoleStrict", "[[,]]", false, 2, syntax_mode::strict_json},
                    array_case{"MismatchedCloseJson5", "[1,}", false, 3, syntax_mode::json5},
                    array_case{"MismatchedCloseStrict", "[1,}", false, 3, syntax_mode::strict_json}),
    [](const testing::TestParamInfo<array_case>& info)
    {
      return info.param.name;
    });
  /* ================================= End Test_J5_013 ================================== */

  /* =================================== Test_J5_030 ==================================== */
  class J5_030_ParsedArrayOrder : public testing::TestWithParam<syntax_mode>
  {
  };
  /*
   * Ensure nested descendants never appear as direct siblings and repeated values survive.
   * Arrange: Mixed values, repeated numbers, and different nested container shapes.
   * Act: Parse, then observe every direct element by indexing and iteration.
   * Expect: Both APIs return the exact source spans and types in source order, then end.
   */
  TEST_P(J5_030_ParsedArrayOrder, PreservesMixedNestedAndRepeatedElements)
  {
    /* Arrange */
    const std::string input = "[7,true,\"\",null,[8,9],{\"x\":[]},7,false]";
    const char* spellings[] = {"7", "true", "\"\"", "null", "[8,9]", "{\"x\":[]}", "7", "false"};
    const value_kind kinds[] = {value_kind::number, value_kind::boolean, value_kind::string, value_kind::null,
                                value_kind::array,  value_kind::object,  value_kind::number, value_kind::boolean};
    document_provider provider;
    parse_options options{GetParam()};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const auto& values = parsed.value_if()->values;
    const auto root = values.root();
    ASSERT_TRUE(root.has_value());
    const auto count = root.value_if()->container_size();
    ASSERT_TRUE(count.has_value());
    ASSERT_EQ(*count.value_if(), 8u);
    // The internal engine creates the root container before any descendant record.
    auto iterator = values.iterate(value_index{0});
    ASSERT_TRUE(iterator.has_value());
    std::size_t expected_offset = 1;
    for (std::size_t ordinal = 0; ordinal < 8; ++ordinal)
    {
      SCOPED_TRACE(ordinal);
      const auto indexed = root.value_if()->element_at(ordinal);
      auto next = iterator.value_if()->next();
      ASSERT_TRUE(indexed.has_value());
      ASSERT_TRUE(next.has_value());
      ASSERT_TRUE(next.value_if()->has_value());
      // Span equality identifies the actual sibling, even when its type or value repeats.
      for (const auto& view : {*indexed.value_if(), **next.value_if()})
      {
        const auto kind = view.kind();
        ASSERT_TRUE(kind.has_value());
        EXPECT_EQ(*kind.value_if(), kinds[ordinal]);
        const auto span = view.source_span();
        ASSERT_TRUE(span.has_value());
        ASSERT_TRUE(span.value_if()->has_value());
        const auto actual = **span.value_if();
        EXPECT_EQ(actual.byte_offset, expected_offset);
        EXPECT_EQ(input.substr(actual.byte_offset, actual.byte_length), spellings[ordinal]);
      }
      expected_offset += std::string(spellings[ordinal]).size() + 1;
    }
    EXPECT_FALSE(root.value_if()->element_at(8).has_value());
    for (int repeat = 0; repeat < 2; ++repeat)
    {
      const auto end = iterator.value_if()->next();
      ASSERT_TRUE(end.has_value());
      EXPECT_FALSE(end.value_if()->has_value());
    }
  }
  INSTANTIATE_TEST_SUITE_P(P034, J5_030_ParsedArrayOrder, testing::Values(syntax_mode::json5, syntax_mode::strict_json),
                           [](const testing::TestParamInfo<syntax_mode>& info)
                           {
                             return info.param == syntax_mode::json5 ? "Json5" : "StrictJson";
                           });
  /* ================================= End Test_J5_030 ================================== */
} // namespace
