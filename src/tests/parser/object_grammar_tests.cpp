// SPDX-License-Identifier: Apache-2.0
#include "../../../tests/support/document_provider.hpp"
#include "adaptive_json5/detail/parser/parser_engine.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_013 ==================================== */
  /**
   * @brief Describe independent object grammar expectations in both syntax modes.
   * @par Requirements
   * P035; J5-013.
   */
  struct object_case
  {
      const char* name;     // Input case name for generated test identification.
      const char* input;    // Input bytes containing an object candidate.
      bool accepted;        // Expected success in the selected mode.
      std::size_t position; // Expected error byte offset, or direct element count on success.
      syntax_mode mode;     // Input syntax selection.
  };
  class J5_013_ObjectGrammar : public testing::TestWithParam<object_case>
  {
  };

  /*
   * Isolate object punctuation from unrelated object-name and scalar extensions.
   * Arrange: A named object candidate with an explicit mode and independent expectation.
   * Act: Run a fresh internal engine and destroy it before checking storage balance.
   * Expect: Exact element count and span, or located syntax failure without a partial tree.
   */
  TEST_P(J5_013_ObjectGrammar, EnforcesElementAndSeparatorRules)
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
    P035, J5_013_ObjectGrammar,
    testing::Values(object_case{"EmptyJson5", "{}", true, 0, syntax_mode::json5},
                    object_case{"EmptyStrict", "{}", true, 0, syntax_mode::strict_json},
                    object_case{"MixedJson5", "{\"n\":null,\"b\":true,\"s\":\"x\",\"a\":[],\"o\":{}}", true, 5,
                                syntax_mode::json5},
                    object_case{"MixedStrict", "{\"n\":null,\"b\":true,\"s\":\"x\",\"a\":[],\"o\":{}}", true, 5,
                                syntax_mode::strict_json},
                    object_case{"EmptyNameJson5", "{\"\":1}", true, 1, syntax_mode::json5},
                    object_case{"EmptyNameStrict", "{\"\":1}", true, 1, syntax_mode::strict_json},
                    object_case{"MissingNameJson5", "{:1}", false, 1, syntax_mode::json5},
                    object_case{"MissingNameStrict", "{:1}", false, 1, syntax_mode::strict_json},
                    object_case{"CommaFirstJson5", "{,}", false, 1, syntax_mode::json5},
                    object_case{"CommaFirstStrict", "{,}", false, 1, syntax_mode::strict_json},
                    object_case{"MissingColonJson5", "{\"a\" 1}", false, 5, syntax_mode::json5},
                    object_case{"MissingColonStrict", "{\"a\" 1}", false, 5, syntax_mode::strict_json},
                    object_case{"MissingValueJson5", "{\"a\":}", false, 5, syntax_mode::json5},
                    object_case{"MissingValueStrict", "{\"a\":}", false, 5, syntax_mode::strict_json},
                    object_case{"MissingCommaJson5", "{\"a\":1 \"b\":2}", false, 7, syntax_mode::json5},
                    object_case{"MissingCommaStrict", "{\"a\":1 \"b\":2}", false, 7, syntax_mode::strict_json},
                    object_case{"DoubleCommaJson5", "{\"a\":1,,\"b\":2}", false, 7, syntax_mode::json5},
                    object_case{"DoubleCommaStrict", "{\"a\":1,,\"b\":2}", false, 7, syntax_mode::strict_json},
                    object_case{"DoubleColonJson5", "{\"a\"::1}", false, 5, syntax_mode::json5},
                    object_case{"DoubleColonStrict", "{\"a\"::1}", false, 5, syntax_mode::strict_json},
                    object_case{"DoubleTrailingJson5", "{\"a\":1,,}", false, 7, syntax_mode::json5},
                    object_case{"DoubleTrailingStrict", "{\"a\":1,,}", false, 7, syntax_mode::strict_json},
                    object_case{"NumericNameJson5", "{1:2}", false, 1, syntax_mode::json5},
                    object_case{"NumericNameStrict", "{1:2}", false, 1, syntax_mode::strict_json},
                    object_case{"EofAfterNameJson5", "{\"a\"", false, 4, syntax_mode::json5},
                    object_case{"EofAfterNameStrict", "{\"a\"", false, 4, syntax_mode::strict_json},
                    object_case{"EofAfterColonJson5", "{\"a\":", false, 5, syntax_mode::json5},
                    object_case{"EofAfterColonStrict", "{\"a\":", false, 5, syntax_mode::strict_json},
                    object_case{"EofAfterCommaJson5", "{\"a\":1,", false, 7, syntax_mode::json5},
                    object_case{"EofAfterCommaStrict", "{\"a\":1,", false, 7, syntax_mode::strict_json},
                    object_case{"NestedMissingValueJson5", "{\"a\":{\"b\":}}", false, 10, syntax_mode::json5},
                    object_case{"NestedMissingValueStrict", "{\"a\":{\"b\":}}", false, 10, syntax_mode::strict_json},
                    object_case{"MismatchedCloseJson5", "{\"a\":1]", false, 6, syntax_mode::json5},
                    object_case{"MismatchedCloseStrict", "{\"a\":1]", false, 6, syntax_mode::strict_json},
                    object_case{"TrailingJson5", "{\"a\":1,}", true, 1, syntax_mode::json5},
                    object_case{"TrailingStrict", "{\"a\":1,}", false, 7, syntax_mode::strict_json},
                    object_case{"NestedTrailingJson5", "{\"a\":{\"b\":1,}}", true, 1, syntax_mode::json5},
                    object_case{"NestedTrailingStrict", "{\"a\":{\"b\":1,}}", false, 12, syntax_mode::strict_json},
                    object_case{"BareNameJson5", "{a:1}", true, 1, syntax_mode::json5},
                    object_case{"BareNameStrict", "{a:1}", false, 1, syntax_mode::strict_json},
                    object_case{"SingleQuotedJson5", "{'a':1}", true, 1, syntax_mode::json5},
                    object_case{"SingleQuotedStrict", "{'a':1}", false, 1, syntax_mode::strict_json},
                    object_case{"CommentBeforeCloseJson5", "{\"a\":1,/*x*/}", true, 1, syntax_mode::json5},
                    object_case{"CommentBeforeCloseStrict", "{\"a\":1,/*x*/}", false, 7, syntax_mode::strict_json}),
    [](const testing::TestParamInfo<object_case>& info)
    {
      return info.param.name;
    });
  /* ================================= End Test_J5_013 ================================== */

  /* =================================== Test_J5_020 ==================================== */
  /**
   * @brief Specify exact decoded names independently of their source spelling.
   * @par Requirements
   * P035; J5-019; J5-020.
   */
  struct name_case
  {
      const char* name;       // Input case identifier.
      const char* input;      // Input object containing one member.
      std::u16string decoded; // Expected decoded UTF-16 name.
      syntax_mode mode;       // Input syntax selection.
  };
  class J5_020_ParsedMemberName : public testing::TestWithParam<name_case>
  {
  };
  /*
   * Keep member-name recognition distinct from value literal recognition.
   * Arrange: Reserved words, escaped identifiers and quoted names with equivalent content.
   * Act: Parse one member and read its retained decoded name.
   * Expect: Exact UTF-16 units, including case, empty content and embedded zero.
   */
  TEST_P(J5_020_ParsedMemberName, RetainsExactDecodedName)
  {
    /* Arrange */
    const auto fixture = GetParam();
    const std::string input = fixture.input;
    const auto& expected = fixture.decoded;
    document_provider provider;
    parse_options options{fixture.mode};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto member = root.value_if()->member_at(0);
    ASSERT_TRUE(member.has_value());
    const auto name = member.value_if()->member_name();
    ASSERT_TRUE(name.has_value());
    // Compare units rather than exported UTF-8 so no normalization or NUL truncation can hide a change.
    ASSERT_EQ((*name.value_if())->size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      const auto unit = (*name.value_if())->unit_at(index);
      ASSERT_TRUE(unit.has_value());
      EXPECT_EQ(*unit.value_if(), expected[index]);
    }
    const auto count = root.value_if()->container_size();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count.value_if(), 1u);
  }
  INSTANTIATE_TEST_SUITE_P(P035, J5_020_ParsedMemberName,
                           testing::Values(name_case{"TrueName", "{true:1}", u"true", syntax_mode::json5},
                                           name_case{"NullName", "{null:1}", u"null", syntax_mode::json5},
                                           name_case{"ReservedName", "{if:1}", u"if", syntax_mode::json5},
                                           name_case{"EscapedStart", "{\\u0061:1}", u"a", syntax_mode::json5},
                                           name_case{"EscapedContinuation", "{a\\u0062:1}", u"ab", syntax_mode::json5},
                                           name_case{"QuotedEscape", "{\"\\u0061\":1}", u"a", syntax_mode::strict_json},
                                           name_case{"SingleQuoted", "{'a':1}", u"a", syntax_mode::json5},
                                           name_case{"DollarUnderscore", "{$_:1}", u"$_", syntax_mode::json5},
                                           name_case{"Empty", "{\"\":1}", u"", syntax_mode::strict_json},
                                           name_case{"EmbeddedZero", "{\"a\\u0000b\":1}", std::u16string{u'a', 0, u'b'},
                                                     syntax_mode::strict_json}),
                           [](const testing::TestParamInfo<name_case>& info)
                           {
                             return info.param.name;
                           });
  /* ================================= End Test_J5_020 ================================== */

  /* =================================== Test_J5_031 ==================================== */
  class J5_031_ParsedObjectMembers : public testing::TestWithParam<syntax_mode>
  {
  };
  /*
   * Verify that pending names survive nested descent and duplicates remain observable.
   * Arrange: A nested array/object followed by repeated names and mixed scalar values.
   * Act: Parse, overwrite input, then enumerate members by indexing and iteration.
   * Expect: Owned decoded names, exact values and source member order survive unchanged.
   */
  TEST_P(J5_031_ParsedObjectMembers, RetainsNamesAcrossNestingAndRepeatedMembers)
  {
    /* Arrange */
    std::string input = "{\"outer\":[{\"inner\":1}],\"same\":true,\"same\":false,\"\":null}";
    const std::u16string expected[] = {u"outer", u"same", u"same", u""};
    const value_kind kinds[] = {value_kind::array, value_kind::boolean, value_kind::boolean, value_kind::null};
    document_provider provider;
    parse_options options{GetParam()};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run();
    std::fill(input.begin(), input.end(), '?');
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const auto& values = parsed.value_if()->values;
    const auto root = values.root();
    ASSERT_TRUE(root.has_value());
    const auto count = root.value_if()->container_size();
    ASSERT_TRUE(count.has_value());
    ASSERT_EQ(*count.value_if(), 4u);
    // Internal engine construction places the root record before all descendants.
    auto iterator = values.iterate(value_index{0});
    ASSERT_TRUE(iterator.has_value());
    for (std::size_t ordinal = 0; ordinal < 4; ++ordinal)
    {
      SCOPED_TRACE(ordinal);
      const auto indexed = root.value_if()->member_at(ordinal);
      const auto next = iterator.value_if()->next();
      ASSERT_TRUE(indexed.has_value());
      ASSERT_TRUE(next.has_value());
      ASSERT_TRUE(next.value_if()->has_value());
      for (const auto& view : {*indexed.value_if(), **next.value_if()})
      {
        const auto name = view.member_name();
        ASSERT_TRUE(name.has_value());
        ASSERT_EQ((*name.value_if())->size(), expected[ordinal].size());
        for (std::size_t index = 0; index < expected[ordinal].size(); ++index)
        {
          const auto unit = (*name.value_if())->unit_at(index);
          ASSERT_TRUE(unit.has_value());
          EXPECT_EQ(*unit.value_if(), expected[ordinal][index]);
        }
        const auto kind = view.kind();
        ASSERT_TRUE(kind.has_value());
        EXPECT_EQ(*kind.value_if(), kinds[ordinal]);
        if (ordinal == 1 || ordinal == 2)
        {
          const auto boolean = view.as_boolean();
          ASSERT_TRUE(boolean.has_value());
          EXPECT_EQ(*boolean.value_if(), ordinal == 1);
        }
      }
    }
    const auto end = iterator.value_if()->next();
    ASSERT_TRUE(end.has_value());
    EXPECT_FALSE(end.value_if()->has_value());
    EXPECT_FALSE(root.value_if()->member_at(4).has_value());
    const auto outer = root.value_if()->member_at(0);
    ASSERT_TRUE(outer.has_value());
    const auto nested = outer.value_if()->element_at(0);
    ASSERT_TRUE(nested.has_value());
    const auto inner = nested.value_if()->member_at(0);
    ASSERT_TRUE(inner.has_value());
    const auto inner_name = inner.value_if()->member_name();
    ASSERT_TRUE(inner_name.has_value());
    ASSERT_EQ((*inner_name.value_if())->size(), 5u);
    const std::u16string inner_expected = u"inner";
    for (std::size_t index = 0; index < inner_expected.size(); ++index)
    {
      const auto unit = (*inner_name.value_if())->unit_at(index);
      ASSERT_TRUE(unit.has_value());
      EXPECT_EQ(*unit.value_if(), inner_expected[index]);
    }
    const auto number = inner.value_if()->as_number();
    ASSERT_TRUE(number.has_value());
    const auto spelling = (*number.value_if())->spelling();
    EXPECT_EQ(std::string(spelling.data, spelling.size), "1");
  }
  INSTANTIATE_TEST_SUITE_P(P035, J5_031_ParsedObjectMembers,
                           testing::Values(syntax_mode::json5, syntax_mode::strict_json),
                           [](const testing::TestParamInfo<syntax_mode>& info)
                           {
                             return info.param == syntax_mode::json5 ? "Json5" : "StrictJson";
                           });
  /* ================================= End Test_J5_031 ================================== */
} // namespace
