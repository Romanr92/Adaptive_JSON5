#include "adaptive_json5/detail/lexer/line_terminator.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using adaptive_json5::consume_line_terminator;
  using adaptive_json5::cursor;
  using adaptive_json5::error_code;
  using adaptive_json5::input_view;

  /* =================================== Test_J5_011 =================================== */

  static_assert(noexcept(consume_line_terminator(std::declval<cursor&>())));

  /**
   * @brief Independent encoding and span expectations for terminator recognition.
   * @par Requirements
   * P007; J5-011; J5-044.
   */
  struct terminator_case
  {
      const char* name;       // Fixture description.
      std::string_view bytes; // Exact UTF-8 input bytes.
      std::size_t length;     // Expected consumed byte length, or zero for no match.
  };

  using J5_011_LineTerminator = testing::TestWithParam<terminator_case>;

  /* Recognize one sequence without consuming a following character.
   * Arrange: Terminator/nonterminator bytes at zero and a multibyte-prefix offset.
   * Act: Call the actual recognizer.
   * Expect: Exact byte span for a match; unchanged cursor for a nonmatch.
   */
  TEST_P(J5_011_LineTerminator, RecognizesExactSpanOrLeavesInput)
  {
    /* Arrange */
    const auto& sample = GetParam();
    for (const std::string_view prefix : {std::string_view{}, std::string_view{"A\xE2\x82\xAC", 4}})
    {
      SCOPED_TRACE(prefix.size());
      std::string bytes{prefix};
      bytes.append(sample.bytes);
      bytes.push_back('X');
      const auto before = bytes;
      cursor source{input_view{bytes.data(), bytes.size()}};
      source.advance(prefix.size());
      /* Act */
      const auto actual = consume_line_terminator(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.error_if(), nullptr);
      ASSERT_NE(actual.value_if(), nullptr);
      EXPECT_EQ(source.offset(), prefix.size() + sample.length);
      EXPECT_EQ(bytes, before);
      if (sample.length != 0)
      {
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ(actual.value_if()->value().byte_offset, prefix.size());
        EXPECT_EQ(actual.value_if()->value().byte_length, sample.length);
        EXPECT_EQ(source.peek(), 'X');
      }
      else
      {
        EXPECT_FALSE(actual.value_if()->has_value());
      }
    }
  }

  INSTANTIATE_TEST_SUITE_P(SequenceFixtures, J5_011_LineTerminator,
                           testing::Values(terminator_case{"Lf", std::string_view{"\x0a", 1}, 1},
                                           terminator_case{"Cr", std::string_view{"\x0d", 1}, 1},
                                           terminator_case{"CrLf", std::string_view{"\x0d\x0a", 2}, 2},
                                           terminator_case{"LineSeparator", std::string_view{"\xe2\x80\xa8", 3}, 3},
                                           terminator_case{"ParagraphSeparator", std::string_view{"\xe2\x80\xa9", 3},
                                                           3},
                                           terminator_case{"SubscriptZero", std::string_view{"\xe2\x82\x80", 3}, 0},
                                           terminator_case{"AsciiLetter", std::string_view{"\x41", 1}, 0},
                                           terminator_case{"Space", std::string_view{"\x20", 1}, 0},
                                           terminator_case{"Tab", std::string_view{"\x09", 1}, 0},
                                           terminator_case{"Nul", std::string_view{"\x00", 1}, 0},
                                           terminator_case{"NoBreakSpace", std::string_view{"\xc2\xa0", 2}, 0},
                                           terminator_case{"NextLine", std::string_view{"\xc2\x85", 2}, 0},
                                           terminator_case{"Bom", std::string_view{"\xef\xbb\xbf", 3}, 0},
                                           terminator_case{"Emoji", std::string_view{"\xf0\x9f\x98\x80", 4}, 0}),
                           [](const testing::TestParamInfo<terminator_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Treat empty input and EOF after consumed input as successful absence.
   * Arrange: Default, nonnull empty and end-position cursors.
   * Act: Recognize twice at each end position.
   * Expect: Empty successful optional and stable byte offset.
   */
  TEST(J5_011_LineTerminatorEof, ReturnsNoSequenceAtEnd)
  {
    /* Arrange */
    const char bytes[] = {'A', static_cast<char>(0xFF)};
    for (const input_view view : {input_view{}, input_view{bytes, 0}, input_view{bytes, sizeof(bytes)}})
    {
      cursor source{view};
      source.advance(view.size);
      for (int attempt = 0; attempt < 2; ++attempt)
      {
        /* Act */
        const auto actual = consume_line_terminator(source);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        ASSERT_NE(actual.value_if(), nullptr);
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(actual.error_if(), nullptr);
        EXPECT_EQ(source.offset(), view.size);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* CR is a complete sequence at EOF, even when backing storage has a hidden LF.
   * Arrange: Nonterminated CR/LF bytes with an explicit view length of one.
   * Act: Recognize CR, then attempt recognition at EOF.
   * Expect: One-byte span followed by successful absence; hidden LF remains unread.
   */
  TEST(J5_011_LineTerminatorEof, CrDoesNotReadPastView)
  {
    /* Arrange */
    const char bytes[] = {'\r', '\n'};
    cursor source{input_view{bytes, 1}};
    /* Act */
    const auto actual = consume_line_terminator(source);
    const auto eof = consume_line_terminator(source);
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_NE(actual.value_if(), nullptr);
    ASSERT_TRUE(actual.value_if()->has_value());
    EXPECT_EQ(actual.value_if()->value().byte_offset, 0U);
    EXPECT_EQ(actual.value_if()->value().byte_length, 1U);
    EXPECT_EQ(source.offset(), 1U);
    ASSERT_TRUE(eof.has_value());
    ASSERT_NE(eof.value_if(), nullptr);
    EXPECT_FALSE(eof.value_if()->has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Keep CRLF together while leaving adjacent line sequences separate.
   * Arrange: CR, CRLF and LF with no terminating NUL.
   * Act: Recognize three successive sequences.
   * Expect: Spans of one, two and one bytes at offsets zero, one and three.
   */
  TEST(J5_011_LineTerminatorGrouping, ConsumesAdjacentSequencesSeparately)
  {
    /* Arrange */
    const char bytes[] = {'\r', '\r', '\n', '\n'};
    const std::size_t lengths[] = {1, 2, 1};
    cursor source{input_view{bytes, sizeof(bytes)}};
    for (const auto length : lengths)
    {
      const auto start = source.offset();
      /* Act */
      const auto actual = consume_line_terminator(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_NE(actual.value_if(), nullptr);
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(actual.value_if()->value().byte_offset, start);
      EXPECT_EQ(actual.value_if()->value().byte_length, length);
      EXPECT_EQ(source.offset(), start + length);
    }
    EXPECT_TRUE(source.is_at_end());
  }

  /* ================================= End Test_J5_011 ================================= */

  /* =================================== Test_J5_009 =================================== */

  /**
   * @brief Invalid transport fixtures for the line-terminator recognizer.
   * @par Requirements
   * P007; J5-009.
   */
  struct encoding_case
  {
      const char* name;       // Fixture description.
      std::string_view bytes; // Exact malformed or truncated input.
  };

  using J5_009_LineTerminatorEncoding = testing::TestWithParam<encoding_case>;

  /* Propagate decoder failures without consuming malformed input.
   * Arrange: Invalid/truncated encoding at zero and a nonzero byte offset.
   * Act: Recognize twice at the same offset.
   * Expect: Located encoding failure and unchanged cursor/input on both attempts.
   */
  TEST_P(J5_009_LineTerminatorEncoding, PreservesErrorAndCursor)
  {
    /* Arrange */
    const auto& sample = GetParam();
    for (const std::string_view prefix : {std::string_view{}, std::string_view{"A\xE2\x82\xAC", 4}})
    {
      std::string bytes{prefix};
      bytes.append(sample.bytes);
      const auto before = bytes;
      cursor source{input_view{bytes.data(), bytes.size()}};
      source.advance(prefix.size());
      for (int attempt = 0; attempt < 2; ++attempt)
      {
        /* Act */
        const auto actual = consume_line_terminator(source);
        /* Expect */
        EXPECT_FALSE(actual.has_value());
        EXPECT_EQ(actual.value_if(), nullptr);
        ASSERT_NE(actual.error_if(), nullptr);
        EXPECT_EQ(actual.error_if()->code, error_code::encoding);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, prefix.size());
        EXPECT_EQ(source.offset(), prefix.size());
        EXPECT_EQ(bytes, before);
      }
    }
  }

  INSTANTIATE_TEST_SUITE_P(InvalidTransport, J5_009_LineTerminatorEncoding,
                           testing::Values(encoding_case{"TruncatedLead", std::string_view{"\xe2", 1}},
                                           encoding_case{"TruncatedSeparator", std::string_view{"\xe2\x80", 2}},
                                           encoding_case{"BadContinuation", std::string_view{"\xe2\x80\x41", 3}},
                                           encoding_case{"LoneContinuation", std::string_view{"\x80", 1}},
                                           encoding_case{"Overlong", std::string_view{"\xc0\x80", 2}},
                                           encoding_case{"Surrogate", std::string_view{"\xed\xa0\x80", 3}},
                                           encoding_case{"AboveMaximum", std::string_view{"\xf4\x90\x80\x80", 4}}),
                           [](const testing::TestParamInfo<encoding_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* CR recognition does not decode unrelated malformed bytes after it.
   * Arrange: Valid CR followed by an invalid UTF-8 leading byte.
   * Act: Recognize CR, then recognize at the following byte.
   * Expect: One-byte CR succeeds; the next operation reports encoding failure at one.
   */
  TEST(J5_009_LineTerminatorEncodingTail, ReportsMalformedByteAfterCrSeparately)
  {
    /* Arrange */
    const char bytes[] = {'\r', static_cast<char>(0xFF)};
    cursor source{input_view{bytes, sizeof(bytes)}};
    /* Act */
    const auto actual = consume_line_terminator(source);
    const auto failure = consume_line_terminator(source);
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_NE(actual.value_if(), nullptr);
    ASSERT_TRUE(actual.value_if()->has_value());
    EXPECT_EQ(actual.value_if()->value().byte_offset, 0U);
    EXPECT_EQ(actual.value_if()->value().byte_length, 1U);
    EXPECT_FALSE(failure.has_value());
    ASSERT_NE(failure.error_if(), nullptr);
    EXPECT_EQ(failure.error_if()->code, error_code::encoding);
    EXPECT_TRUE(failure.error_if()->has_byte_offset);
    EXPECT_EQ(failure.error_if()->byte_offset, 1U);
    EXPECT_EQ(source.offset(), 1U);
  }

  /* ================================= End Test_J5_009 ================================= */
} // namespace
