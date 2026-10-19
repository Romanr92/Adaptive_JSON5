// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/comment.hpp"
#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/lexer/trivia.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /* =================================== Test_J5_012 =================================== */

  static_assert(noexcept(consume_line_comment(std::declval<cursor&>())));
  static_assert(noexcept(consume_block_comment(std::declval<cursor&>())));
  static_assert(noexcept(skip_trivia(std::declval<cursor&>(), syntax_mode::json5)));

  /**
   * @brief Independent comment-recognition inputs and expected original spans.
   * @par Requirements
   * P009; P010; J5-012.
   */
  struct comment_case
  {
      const char* name;      // Parameter description.
      std::string_view text; // Input bytes, including any unread suffix.
      bool block;            // Input recognizer selection: true for block comments.
      std::size_t length;    // Expected comment length in bytes; zero means no match.
  };

  using J5_012_Comments = testing::TestWithParam<comment_case>;

  /* Recognize comments without consuming a terminator or following token.
   * Arrange: Independent fixtures at zero and a nonzero original byte offset.
   * Act: Call the selected production comment recognizer.
   * Expect: Exact span or no match, preserved input and precise cursor advancement.
   */
  TEST_P(J5_012_Comments, PreservesExactSpanAndSuffix)
  {
    /* Arrange */
    const auto& sample = GetParam();
    for (const std::string_view prefix : {std::string_view{}, std::string_view{"\xE2\x82\xAC", 3}})
    {
      SCOPED_TRACE(prefix.size());
      std::string bytes{prefix};
      bytes.append(sample.text);
      const auto original = bytes;
      cursor source{input_view{bytes.data(), bytes.size()}};
      source.advance(prefix.size());
      /* Act */
      const auto actual = sample.block ? consume_block_comment(source) : consume_line_comment(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.error_if(), nullptr);
      ASSERT_NE(actual.value_if(), nullptr);
      EXPECT_EQ(source.offset(), prefix.size() + sample.length);
      EXPECT_EQ(bytes, original);
      if (sample.length == 0)
      {
        EXPECT_FALSE(actual.value_if()->has_value());
      }
      else
      {
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ(actual.value_if()->value().byte_offset, prefix.size());
        EXPECT_EQ(actual.value_if()->value().byte_length, sample.length);
      }
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    Spellings, J5_012_Comments,
    testing::Values(
      comment_case{"LineEof", "//abc", false, 5}, comment_case{"EmptyLineEof", "//", false, 2},
      comment_case{"Lf", "//a\nX", false, 3}, comment_case{"Cr", "//a\rX", false, 3},
      comment_case{"CrLf", "//a\r\nX", false, 3}, comment_case{"LineSeparator", "//a\xE2\x80\xA8X", false, 3},
      comment_case{"ParagraphSeparator", "//a\xE2\x80\xA9X", false, 3}, comment_case{"EmptyBeforeLf", "//\n", false, 2},
      comment_case{"UnicodeLine", "//\xF0\x9F\x98\x80", false, 6},
      comment_case{"NulLine", std::string_view{"//\0x", 4}, false, 4},
      comment_case{"LiteralEscapes", "//\\n\\u2028", false, 10},
      comment_case{"DelimitersInLine", "// /* */ {} , :", false, 15}, comment_case{"EmptyInput", "", false, 0},
      comment_case{"LoneSlash", "/", false, 0}, comment_case{"OtherSlash", "/x", false, 0},
      comment_case{"BlockIsNotLine", "/**/", false, 0}, comment_case{"QuoteIsNotLine", "\"//\"", false, 0},
      comment_case{"EmptyBlock", "/**/X", true, 4}, comment_case{"FirstCloser", "/*a*/b*/", true, 5},
      comment_case{"NoNesting", "/*a/*b*/c*/", true, 8}, comment_case{"Stars", "/****/X", true, 6},
      comment_case{"UnicodeBlock", "/*\xE2\x82\xAC*/X", true, 7},
      comment_case{"NewlinesInBlock", "/*\r\n\xE2\x80\xA8*/", true, 9},
      comment_case{"NulBlock", std::string_view{"/*\0*/", 5}, true, 5}, comment_case{"LineIsNotBlock", "//x", true, 0},
      comment_case{"QuoteIsNotBlock", "'/*x*/'", true, 0}, comment_case{"EmptyBlockInput", "", true, 0},
      comment_case{"LoneBlockSlash", "/", true, 0}),
    [](const testing::TestParamInfo<comment_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Reject every incomplete suffix of a block comment.
   * Arrange: Every proper prefix of a valid ASCII comment at a nonzero offset.
   * Act: Recognize each bounded prefix without exposing its backing suffix.
   * Expect: No match before the opener completes; syntax failure and rollback after it.
   */
  TEST(J5_012_CommentBoundaries, RejectsEveryUnterminatedPrefix)
  {
    /* Arrange */
    const std::string bytes = "X/*abc**/";
    for (std::size_t end = 1; end < bytes.size(); ++end)
    {
      SCOPED_TRACE(end);
      cursor source{input_view{bytes.data(), end}};
      source.advance(1);
      /* Act */
      const auto actual = consume_block_comment(source);
      /* Expect */
      EXPECT_EQ(source.offset(), 1U);
      if (end < 3)
      {
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
      }
      else
      {
        ASSERT_FALSE(actual.has_value());
        ASSERT_NE(actual.error_if(), nullptr);
        EXPECT_EQ(actual.error_if()->code, error_code::syntax);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 1U);
        EXPECT_EQ(actual.value_if(), nullptr);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Bound recognition to explicit lengths and permit successful reuse at EOF.
   * Arrange: Nonterminated arrays and default empty cursors.
   * Act: Recognize comments with suffix bytes hidden by the view length.
   * Expect: Line EOF succeeds; hidden block closer cannot complete a comment.
   */
  TEST(J5_012_CommentBoundaries, RespectsViewLengthAndEmptyCursors)
  {
    /* Arrange */
    const char line[] = {'/', '/', 'x', '\n'};
    const char block[] = {'/', '*', '*', '/'};
    cursor line_source{input_view{line, 3}};
    cursor block_source{input_view{block, 3}};
    cursor empty;
    /* Act */
    const auto line_result = consume_line_comment(line_source);
    const auto block_result = consume_block_comment(block_source);
    const auto empty_line = consume_line_comment(empty);
    const auto empty_block = consume_block_comment(empty);
    const auto at_end = consume_line_comment(line_source);
    /* Expect */
    ASSERT_TRUE(line_result.has_value());
    ASSERT_TRUE(line_result.value_if()->has_value());
    EXPECT_EQ(line_result.value_if()->value().byte_length, 3U);
    EXPECT_TRUE(line_source.is_at_end());
    ASSERT_FALSE(block_result.has_value());
    EXPECT_EQ(block_result.error_if()->code, error_code::syntax);
    EXPECT_EQ(block_source.offset(), 0U);
    ASSERT_TRUE(empty_line.has_value());
    EXPECT_FALSE(empty_line.value_if()->has_value());
    ASSERT_TRUE(empty_block.has_value());
    EXPECT_FALSE(empty_block.value_if()->has_value());
    ASSERT_TRUE(at_end.has_value());
    EXPECT_FALSE(at_end.value_if()->has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Malformed UTF-8 fixture bytes independently chosen from encoding rules.
   * @par Requirements
   * P009; P010; P011; J5-009; J5-012.
   */
  struct encoding_case
  {
      const char* name;       // Parameter description.
      std::string_view bytes; // Invalid input sequence in bytes.
  };

  using J5_012_CommentEncoding = testing::TestWithParam<encoding_case>;

  /* Reject malformed bytes inside either comment without publishing progress.
   * Arrange: Valid prefix, opener and text followed by malformed UTF-8.
   * Act: Recognize both comment forms, then reuse the original cursor prefix.
   * Expect: Encoding category and exact sequence-start offset; cursor unchanged.
   */
  TEST_P(J5_012_CommentEncoding, ReportsOriginalOffsetAndRollsBack)
  {
    /* Arrange */
    for (const bool block : {false, true})
    {
      SCOPED_TRACE(block);
      std::string bytes = block ? "X/*ok" : "X//ok";
      bytes.append(GetParam().bytes);
      cursor source{input_view{bytes.data(), bytes.size()}};
      source.advance(1);
      /* Act */
      const auto actual = block ? consume_block_comment(source) : consume_line_comment(source);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      ASSERT_NE(actual.error_if(), nullptr);
      EXPECT_EQ(actual.error_if()->code, error_code::encoding);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 5U);
      EXPECT_EQ(actual.value_if(), nullptr);
      EXPECT_EQ(source.offset(), 1U);
      EXPECT_EQ(source.peek(), '/');
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    Malformed, J5_012_CommentEncoding,
    testing::Values(encoding_case{"Continuation", "\x80"}, encoding_case{"InvalidLead", "\xFF"},
                    encoding_case{"Overlong", "\xC0\xAF"}, encoding_case{"Surrogate", "\xED\xA0\x80"},
                    encoding_case{"AboveMaximum", "\xF4\x90\x80\x80"}, encoding_case{"TruncatedTwo", "\xC2"},
                    encoding_case{"TruncatedThree", "\xE2\x82"}, encoding_case{"TruncatedFour", "\xF0\x9F\x98"},
                    encoding_case{"BadContinuation", "\xE2X"}),
    [](const testing::TestParamInfo<encoding_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Ignore delimiters inside a comment and never inspect bytes after its end.
   * Arrange: Valid comments followed by invalid UTF-8 outside their spans.
   * Act: Recognize each comment directly.
   * Expect: Success through the proper boundary and the invalid suffix unread.
   */
  TEST(J5_012_CommentBoundaries, StopsBeforeInvalidSuffix)
  {
    /* Arrange */
    const std::string line = "//a\n\xFF";
    const std::string block = "/*a*/\xFF";
    cursor line_source{input_view{line.data(), line.size()}};
    cursor block_source{input_view{block.data(), block.size()}};
    /* Act */
    const auto line_result = consume_line_comment(line_source);
    const auto block_result = consume_block_comment(block_source);
    /* Expect */
    ASSERT_TRUE(line_result.has_value());
    EXPECT_EQ(line_source.offset(), 3U);
    ASSERT_TRUE(block_result.has_value());
    EXPECT_EQ(block_source.offset(), 5U);
  }

  /* ================================= End Test_J5_012 ================================= */

  /* =================================== Test_J5_024 =================================== */

  /**
   * @brief Trivia outcomes in each language, with independent span/error expectations.
   * @par Requirements
   * P011; J5-010; J5-012; J5-024.
   */
  struct trivia_case
  {
      const char* name;      // Parameter description.
      std::string_view text; // Input bytes after a fixed prefix.
      syntax_mode mode;      // Input language selection.
      std::size_t length;    // Expected consumed bytes on success.
      bool failure;          // Expected failure rather than success.
      error_code code;       // Expected category on failure.
      std::size_t offset;    // Expected error byte offset relative to text.
  };

  using J5_024_Trivia = testing::TestWithParam<trivia_case>;

  /* Compose mode-specific trivia with atomic advancement.
   * Arrange: Fixed prefix plus mixed trivia, token boundaries or invalid input.
   * Act: Skip trivia from the original nonzero offset.
   * Expect: Exact range on success or original located error and full rollback.
   */
  TEST_P(J5_024_Trivia, ComposesOrReportsLocatedFailure)
  {
    /* Arrange */
    const auto& sample = GetParam();
    std::string bytes = "XYZ";
    bytes.append(sample.text);
    const auto original = bytes;
    cursor source{input_view{bytes.data(), bytes.size()}};
    source.advance(3);
    /* Act */
    const auto actual = skip_trivia(source, sample.mode);
    /* Expect */
    EXPECT_EQ(bytes, original);
    if (sample.failure)
    {
      ASSERT_FALSE(actual.has_value());
      ASSERT_NE(actual.error_if(), nullptr);
      EXPECT_EQ(actual.error_if()->code, sample.code);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 3 + sample.offset);
      EXPECT_EQ(actual.value_if(), nullptr);
      EXPECT_EQ(source.offset(), 3U);
    }
    else
    {
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.error_if(), nullptr);
      ASSERT_NE(actual.value_if(), nullptr);
      EXPECT_EQ(actual.value_if()->byte_offset, 3U);
      EXPECT_EQ(actual.value_if()->byte_length, sample.length);
      EXPECT_EQ(source.offset(), 3 + sample.length);
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    Modes, J5_024_Trivia,
    testing::Values(
      trivia_case{"Empty", "", syntax_mode::json5, 0, false, error_code::syntax, 0},
      trivia_case{"Mixed", " \t/*a*///b\r\n{}", syntax_mode::json5, 12, false, error_code::syntax, 0},
      trivia_case{"AdjacentComments", "/**//**/X", syntax_mode::json5, 8, false, error_code::syntax, 0},
      trivia_case{"LineAtEof", " //x", syntax_mode::json5, 4, false, error_code::syntax, 0},
      trivia_case{"UnicodeWhitespace", "\xC2\xA0\xE2\x80\xA8X", syntax_mode::json5, 5, false, error_code::syntax, 0},
      trivia_case{"StrictSpaces", " \t\r\nX", syntax_mode::strict_json, 4, false, error_code::syntax, 0},
      trivia_case{"StrictNbsp", " \xC2\xA0", syntax_mode::strict_json, 1, false, error_code::syntax, 0},
      trivia_case{"StrictVerticalTab", "\v", syntax_mode::strict_json, 0, false, error_code::syntax, 0},
      trivia_case{"StrictFormFeed", "\f", syntax_mode::strict_json, 0, false, error_code::syntax, 0},
      trivia_case{"StrictBom", "\xEF\xBB\xBF", syntax_mode::strict_json, 0, false, error_code::syntax, 0},
      trivia_case{"StrictSeparator", "\xE2\x80\xA8", syntax_mode::strict_json, 0, false, error_code::syntax, 0},
      trivia_case{"StrictLine", " \t//x", syntax_mode::strict_json, 0, true, error_code::syntax, 2},
      trivia_case{"StrictBlock", " /*x*/", syntax_mode::strict_json, 0, true, error_code::syntax, 1},
      trivia_case{"StrictUnterminated", " /*", syntax_mode::strict_json, 0, true, error_code::syntax, 1},
      trivia_case{"UnterminatedAfterComment", " /**/ /*x", syntax_mode::json5, 0, true, error_code::syntax, 6},
      trivia_case{"BadEncodingAfterTrivia", " /**/\xFF", syntax_mode::json5, 0, true, error_code::encoding, 5},
      trivia_case{"BadEncodingInComment", " /*a\xFF*/", syntax_mode::json5, 0, true, error_code::encoding, 4},
      trivia_case{"StrictBadEncoding", " \xFF", syntax_mode::strict_json, 0, true, error_code::encoding, 1},
      trivia_case{"NumberBoundary", "1/*x*/2", syntax_mode::json5, 0, false, error_code::syntax, 0},
      trivia_case{"QuotedBoundary", "\"//x\"", syntax_mode::json5, 0, false, error_code::syntax, 0},
      trivia_case{"LoneSlash", " /", syntax_mode::json5, 1, false, error_code::syntax, 0},
      trivia_case{"OtherSlash", " /x", syntax_mode::strict_json, 1, false, error_code::syntax, 0},
      trivia_case{"NulBoundary", std::string_view{" \0X", 3}, syntax_mode::json5, 1, false, error_code::syntax, 0}),
    [](const testing::TestParamInfo<trivia_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Preserve both sides of trivia rather than joining separated token spellings.
   * Arrange: Numeric-looking bytes separated by a comment.
   * Act: Skip trivia before and after a simulated one-byte token consumer.
   * Expect: First digit remains initially; the second remains after comment skipping.
   */
  TEST(J5_024_TriviaBoundary, DoesNotJoinSeparatedTokens)
  {
    /* Arrange */
    const char bytes[] = {'1', '/', '*', 'x', '*', '/', '2'};
    cursor source{input_view{bytes, sizeof(bytes)}};
    /* Act */
    const auto before = skip_trivia(source, syntax_mode::json5);
    const auto first = source.next();
    const auto between = skip_trivia(source, syntax_mode::json5);
    const auto second = source.peek();
    /* Expect */
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(before.value_if()->byte_length, 0U);
    EXPECT_EQ(first, '1');
    ASSERT_TRUE(between.has_value());
    EXPECT_EQ(between.value_if()->byte_offset, 1U);
    EXPECT_EQ(between.value_if()->byte_length, 5U);
    EXPECT_EQ(source.offset(), 6U);
    EXPECT_EQ(second, '2');
  }

  /* ----------------------------------------------------------------------------------- */

  /* Keep default empty cursors and hidden suffixes outside the trivia run.
   * Arrange: Default cursors and a bounded view hiding an invalid following byte.
   * Act: Skip trivia in both languages, including repeated recognition at EOF.
   * Expect: Empty ranges or exactly the visible spaces, with no hidden-byte error.
   */
  TEST(J5_024_TriviaBoundary, HandlesEmptyAndBoundedViews)
  {
    /* Arrange */
    const char bytes[] = {' ', '\t', static_cast<char>(0xFF)};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor empty;
      cursor bounded{input_view{bytes, 2}};
      /* Act */
      const auto empty_result = skip_trivia(empty, mode);
      const auto bounded_result = skip_trivia(bounded, mode);
      const auto end_result = skip_trivia(bounded, mode);
      /* Expect */
      ASSERT_TRUE(empty_result.has_value());
      EXPECT_EQ(empty_result.value_if()->byte_offset, 0U);
      EXPECT_EQ(empty_result.value_if()->byte_length, 0U);
      ASSERT_TRUE(bounded_result.has_value());
      EXPECT_EQ(bounded_result.value_if()->byte_length, 2U);
      ASSERT_TRUE(end_result.has_value());
      EXPECT_EQ(end_result.value_if()->byte_offset, 2U);
      EXPECT_EQ(end_result.value_if()->byte_length, 0U);
    }
  }

  /* ================================= End Test_J5_024 ================================= */

  /* =================================== Test_J5_013 =================================== */

  static_assert(noexcept(consume_punctuation(std::declval<cursor&>())));

  /* Recognize adjacent punctuation with exact kinds and original byte ranges.
   * Arrange: All six punctuators following a three-byte Unicode prefix.
   * Act: Consume one punctuation token per call, then call at EOF.
   * Expect: Independently specified kinds, one-byte lengths and exact offsets.
   */
  TEST(J5_013_Punctuation, RecognizesAdjacentKindsAndSpans)
  {
    /* Arrange */
    const std::string bytes = "\xE2\x82\xAC{}[]:,";
    const auto original = bytes;
    const std::array<punctuation_kind, 6> expected = {punctuation_kind::left_brace,   punctuation_kind::right_brace,
                                                      punctuation_kind::left_bracket, punctuation_kind::right_bracket,
                                                      punctuation_kind::colon,        punctuation_kind::comma};
    cursor source{input_view{bytes.data(), bytes.size()}};
    source.advance(3);
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      SCOPED_TRACE(index);
      /* Act */
      const auto actual = consume_punctuation(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual->kind, expected[index]);
      EXPECT_EQ(actual->byte_offset, 3 + index);
      EXPECT_EQ(actual->byte_length, 1U);
      EXPECT_EQ(source.offset(), 4 + index);
    }
    EXPECT_FALSE(consume_punctuation(source).has_value());
    EXPECT_EQ(bytes, original);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Restrict recognition to the exact six ASCII punctuators.
   * Arrange: Each possible byte, a default cursor and a hidden punctuation suffix.
   * Act: Recognize each one-byte view or empty view.
   * Expect: Only the six specified bytes advance; all other bytes remain unread.
   */
  TEST(J5_013_Punctuation, LeavesEveryOtherByteAndEmptyInputUnread)
  {
    /* Arrange */
    const std::string_view accepted = "{}[]:,";
    for (unsigned int value = 0; value <= 255; ++value)
    {
      SCOPED_TRACE(value);
      const char byte = static_cast<char>(value);
      cursor source{input_view{&byte, 1}};
      const bool match = accepted.find(byte) != std::string_view::npos;
      /* Act */
      const auto actual = consume_punctuation(source);
      /* Expect */
      EXPECT_EQ(actual.has_value(), match);
      EXPECT_EQ(source.offset(), match ? 1U : 0U);
    }
    cursor empty;
    const char hidden = '{';
    cursor bounded{input_view{&hidden, 0}};
    EXPECT_FALSE(consume_punctuation(empty).has_value());
    EXPECT_FALSE(consume_punctuation(bounded).has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Prevent comment punctuation from escaping trivia recognition.
   * Arrange: Both comment forms containing all punctuators before one real token.
   * Act: Skip trivia and recognize the following punctuation in JSON5 mode.
   * Expect: Only the final opening bracket is emitted at its original offset.
   */
  TEST(J5_013_Punctuation, RecognizesOnlyAfterCommentTrivia)
  {
    /* Arrange */
    const std::string bytes = "/*{}[]:,*/ //{}[]:,\n[";
    cursor source{input_view{bytes.data(), bytes.size()}};
    /* Act */
    const auto trivia = skip_trivia(source, syntax_mode::json5);
    const auto token = consume_punctuation(source);
    /* Expect */
    ASSERT_TRUE(trivia.has_value());
    EXPECT_EQ(trivia.value_if()->byte_length, bytes.size() - 1);
    ASSERT_TRUE(token.has_value());
    EXPECT_EQ(token->kind, punctuation_kind::left_bracket);
    EXPECT_EQ(token->byte_offset, bytes.size() - 1);
    EXPECT_EQ(token->byte_length, 1U);
    EXPECT_TRUE(source.is_at_end());
  }

  /* ================================= End Test_J5_013 ================================= */
} // namespace
