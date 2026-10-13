// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/whitespace.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using adaptive_json5::cursor;
  using adaptive_json5::input_view;
  using adaptive_json5::skip_json5_whitespace;

  /* =================================== Test_J5_011 =================================== */

  static_assert(noexcept(skip_json5_whitespace(std::declval<cursor&>())));
  static_assert(noexcept(adaptive_json5::is_json5_space(0)));

  /**
   * @brief Independently encoded whitespace and nonwhitespace fixtures.
   * @par Requirements
   * P008; J5-011; J5-025.
   */
  struct whitespace_case
  {
      const char* name;       // Case description.
      std::string_view bytes; // Exact UTF-8 input bytes.
      char32_t scalar;        // Scalar for the space-only classifier.
      bool space;             // Expected space-only classification.
      std::size_t consumed;   // Expected skipped bytes.
      std::size_t lines;      // Expected line-terminator sequence count.
  };

  using J5_011_Whitespace = testing::TestWithParam<whitespace_case>;

  /* Recognize every admitted space and distinguish line terminators/nonwhitespace.
   * Arrange: Independent UTF-8 fixture at zero and a multibyte-prefix offset.
   * Act: Classify the scalar and skip up to a following token.
   * Expect: Exact byte span, line count, immutable input and unread token.
   */
  TEST_P(J5_011_Whitespace, ClassifiesAndSkipsExactRun)
  {
    /* Arrange */
    const auto& sample = GetParam();
    for (const std::string_view prefix : {std::string_view{}, std::string_view{"A\xE2\x82\xAC", 4}})
    {
      for (const std::string_view token : {"{", "}", "[", "]", ":", ",", "\"text\"", "'text'", "true", "false", "null",
                                           "1", "0x1", "Infinity", "NaN", "name"})
      {
        std::string bytes{prefix};
        bytes.append(sample.bytes);
        bytes.append(token);
        bytes.append(sample.bytes);
        const auto before = bytes;
        cursor source{input_view{bytes.data(), bytes.size()}};
        source.advance(prefix.size());
        SCOPED_TRACE(prefix.size());
        SCOPED_TRACE(std::string{token});
        /* Act */
        const bool is_space = adaptive_json5::is_json5_space(sample.scalar);
        const auto actual = skip_json5_whitespace(source);
        /* Expect */
        EXPECT_EQ(is_space, sample.space);
        ASSERT_TRUE(actual.has_value());
        EXPECT_EQ(actual.error_if(), nullptr);
        ASSERT_NE(actual.value_if(), nullptr);
        EXPECT_EQ(actual.value_if()->byte_offset, prefix.size());
        EXPECT_EQ(actual.value_if()->byte_length, sample.consumed);
        EXPECT_EQ(actual.value_if()->line_break_count, sample.lines);
        EXPECT_EQ(source.offset(), prefix.size() + sample.consumed);
        EXPECT_EQ(bytes, before);
        if (sample.consumed != 0)
        {
          EXPECT_EQ(source.peek(), token.front());
          // Simulate the token consumer; this unit tests only surrounding whitespace.
          source.advance(token.size());
          const auto trailing_start = source.offset();
          /* Act */
          const auto trailing = skip_json5_whitespace(source);
          /* Expect */
          ASSERT_TRUE(trailing.has_value());
          ASSERT_NE(trailing.value_if(), nullptr);
          EXPECT_EQ(trailing.value_if()->byte_offset, trailing_start);
          EXPECT_EQ(trailing.value_if()->byte_length, sample.consumed);
          EXPECT_EQ(trailing.value_if()->line_break_count, sample.lines);
          EXPECT_TRUE(source.is_at_end());
        }
      }
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    CharacterFixtures, J5_011_Whitespace,
    testing::Values(whitespace_case{"Ht", std::string_view{"\x09", 1}, 0x9, true, 1, 0},
                    whitespace_case{"Vt", std::string_view{"\x0b", 1}, 0xB, true, 1, 0},
                    whitespace_case{"Ff", std::string_view{"\x0c", 1}, 0xC, true, 1, 0},
                    whitespace_case{"Sp", std::string_view{"\x20", 1}, 0x20, true, 1, 0},
                    whitespace_case{"Nbsp", std::string_view{"\xc2\xa0", 2}, 0xA0, true, 2, 0},
                    whitespace_case{"Ogham", std::string_view{"\xe1\x9a\x80", 3}, 0x1680, true, 3, 0},
                    whitespace_case{"Space2000", std::string_view{"\xe2\x80\x80", 3}, 0x2000, true, 3, 0},
                    whitespace_case{"Space2001", std::string_view{"\xe2\x80\x81", 3}, 0x2001, true, 3, 0},
                    whitespace_case{"Space2002", std::string_view{"\xe2\x80\x82", 3}, 0x2002, true, 3, 0},
                    whitespace_case{"Space2003", std::string_view{"\xe2\x80\x83", 3}, 0x2003, true, 3, 0},
                    whitespace_case{"Space2004", std::string_view{"\xe2\x80\x84", 3}, 0x2004, true, 3, 0},
                    whitespace_case{"Space2005", std::string_view{"\xe2\x80\x85", 3}, 0x2005, true, 3, 0},
                    whitespace_case{"Space2006", std::string_view{"\xe2\x80\x86", 3}, 0x2006, true, 3, 0},
                    whitespace_case{"Space2007", std::string_view{"\xe2\x80\x87", 3}, 0x2007, true, 3, 0},
                    whitespace_case{"Space2008", std::string_view{"\xe2\x80\x88", 3}, 0x2008, true, 3, 0},
                    whitespace_case{"Space2009", std::string_view{"\xe2\x80\x89", 3}, 0x2009, true, 3, 0},
                    whitespace_case{"Space200A", std::string_view{"\xe2\x80\x8a", 3}, 0x200A, true, 3, 0},
                    whitespace_case{"LegacyZeroWidthSpace", std::string_view{"\xe2\x80\x8b", 3}, 0x200B, true, 3, 0},
                    whitespace_case{"NarrowNbsp", std::string_view{"\xe2\x80\xaf", 3}, 0x202F, true, 3, 0},
                    whitespace_case{"MediumMathSpace", std::string_view{"\xe2\x81\x9f", 3}, 0x205F, true, 3, 0},
                    whitespace_case{"IdeographicSpace", std::string_view{"\xe3\x80\x80", 3}, 0x3000, true, 3, 0},
                    whitespace_case{"Bom", std::string_view{"\xef\xbb\xbf", 3}, 0xFEFF, true, 3, 0},
                    whitespace_case{"Lf", std::string_view{"\x0a", 1}, 0xA, false, 1, 1},
                    whitespace_case{"Cr", std::string_view{"\x0d", 1}, 0xD, false, 1, 1},
                    whitespace_case{"CrLf", std::string_view{"\x0d\x0a", 2}, 0xD, false, 2, 1},
                    whitespace_case{"Ls", std::string_view{"\xe2\x80\xa8", 3}, 0x2028, false, 3, 1},
                    whitespace_case{"Ps", std::string_view{"\xe2\x80\xa9", 3}, 0x2029, false, 3, 1},
                    whitespace_case{"Nul", std::string_view{"\x00", 1}, 0x0, false, 0, 0},
                    whitespace_case{"Backspace", std::string_view{"\x08", 1}, 0x8, false, 0, 0},
                    whitespace_case{"NextLine", std::string_view{"\xc2\x85", 2}, 0x85, false, 0, 0},
                    whitespace_case{"MongolianSeparator", std::string_view{"\xe1\xa0\x8e", 3}, 0x180E, false, 0, 0},
                    whitespace_case{"SubscriptZero", std::string_view{"\xe2\x82\x80", 3}, 0x2080, false, 0, 0},
                    whitespace_case{"Joiner", std::string_view{"\xe2\x80\x8d", 3}, 0x200D, false, 0, 0},
                    whitespace_case{"Emoji", std::string_view{"\xf0\x9f\x98\x80", 4}, 0x1F600, false, 0, 0}),
    [](const testing::TestParamInfo<whitespace_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Empty/default/nonterminated views preserve valid EOF byte locations.
   * Arrange: Empty views and an exact-length CR buffer with hidden LF.
   * Act: Skip the view, then skip again at EOF.
   * Expect: Empty spans for EOF, one CR line for the bounded view, no hidden read.
   */
  TEST(J5_011_WhitespaceEof, HandlesEmptyAndExactLengthViews)
  {
    /* Arrange */
    const char bytes[] = {'\r', '\n'};
    for (const input_view view : {input_view{}, input_view{bytes, 0}, input_view{bytes, 1}})
    {
      cursor source{view};
      /* Act */
      const auto actual = skip_json5_whitespace(source);
      const auto again = skip_json5_whitespace(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_NE(actual.value_if(), nullptr);
      EXPECT_EQ(actual.value_if()->byte_offset, 0U);
      EXPECT_EQ(actual.value_if()->byte_length, view.size);
      EXPECT_EQ(actual.value_if()->line_break_count, view.size);
      ASSERT_TRUE(again.has_value());
      ASSERT_NE(again.value_if(), nullptr);
      EXPECT_EQ(again.value_if()->byte_offset, view.size);
      EXPECT_EQ(again.value_if()->byte_length, 0U);
      EXPECT_EQ(again.value_if()->line_break_count, 0U);
      EXPECT_EQ(source.offset(), view.size);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Mixed line sequences count CRLF once and stop before a decoded token.
   * Arrange: Spaces, CRLF, LF, CR, LS, PS, BOM and supplementary nonwhitespace.
   * Act: Skip twice without consuming the token.
   * Expect: Eighteen whitespace bytes, five lines and an empty second run.
   */
  TEST(J5_011_WhitespaceRun, CountsMixedLinesAndStopsAtSupplementaryToken)
  {
    /* Arrange */
    const std::string bytes{" \t\r\n\n\r\xE2\x80\xA8\xE2\x80\xA9\xEF\xBB\xBF\xC2\xA0 \xF0\x9F\x98\x80", 22};
    cursor source{input_view{bytes.data(), bytes.size()}};
    /* Act */
    const auto actual = skip_json5_whitespace(source);
    const auto again = skip_json5_whitespace(source);
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_NE(actual.value_if(), nullptr);
    EXPECT_EQ(actual.value_if()->byte_offset, 0U);
    EXPECT_EQ(actual.value_if()->byte_length, 18U);
    EXPECT_EQ(actual.value_if()->line_break_count, 5U);
    EXPECT_EQ(source.offset(), 18U);
    ASSERT_TRUE(again.has_value());
    ASSERT_NE(again.value_if(), nullptr);
    EXPECT_EQ(again.value_if()->byte_offset, 18U);
    EXPECT_EQ(again.value_if()->byte_length, 0U);
    EXPECT_EQ(again.value_if()->line_break_count, 0U);
  }

  /* ================================= End Test_J5_011 ================================= */

  /* =================================== Test_J5_025 =================================== */

  /* BOM is whitespace at the beginning and between token boundaries in JSON5.
   * Arrange: Leading BOM, token byte, another BOM and a second token byte.
   * Act: Skip, consume the first token boundary byte, then skip again.
   * Expect: Both BOMs are skipped with exact spans and zero line breaks.
   */
  TEST(J5_025_Json5Whitespace, SkipsLeadingAndInteriorBom)
  {
    /* Arrange */
    const char bytes[] = "\xEF\xBB\xBF{\xEF\xBB\xBF}";
    cursor source{input_view{bytes, sizeof(bytes) - 1}};
    /* Act */
    const auto leading = skip_json5_whitespace(source);
    const auto token = source.next();
    const auto interior = skip_json5_whitespace(source);
    /* Expect */
    ASSERT_TRUE(leading.has_value());
    ASSERT_NE(leading.value_if(), nullptr);
    EXPECT_EQ(leading.value_if()->byte_offset, 0U);
    EXPECT_EQ(leading.value_if()->byte_length, 3U);
    EXPECT_EQ(leading.value_if()->line_break_count, 0U);
    EXPECT_EQ(token, '{');
    ASSERT_TRUE(interior.has_value());
    ASSERT_NE(interior.value_if(), nullptr);
    EXPECT_EQ(interior.value_if()->byte_offset, 4U);
    EXPECT_EQ(interior.value_if()->byte_length, 3U);
    EXPECT_EQ(interior.value_if()->line_break_count, 0U);
    EXPECT_EQ(source.peek(), '}');
  }

  /* ================================= End Test_J5_025 ================================= */

  /* =================================== Test_J5_009 =================================== */

  /* Malformed input after consumed candidates rolls back the entire run.
   * Arrange: Invalid/truncated transport after no trivia or spaces/CRLF/BOM.
   * Act: Skip twice at a nonzero original byte offset.
   * Expect: Exact failing sequence offset with unchanged original cursor/input.
   */
  TEST(J5_009_WhitespaceEncoding, ReportsOriginalOffsetAndRollsBackRun)
  {
    /* Arrange */
    for (const std::string_view invalid :
         {std::string_view{"\xFF", 1}, std::string_view{"\xE2\x80", 2}, std::string_view{"\xED\xA0\x80", 3}})
    {
      for (const std::string_view trivia : {std::string_view{}, std::string_view{" \r\n\xEF\xBB\xBF", 6}})
      {
        std::string bytes{"A\xE2\x82\xAC", 4};
        bytes.append(trivia);
        bytes.append(invalid);
        const auto before = bytes;
        cursor source{input_view{bytes.data(), bytes.size()}};
        source.advance(4);
        for (int attempt = 0; attempt < 2; ++attempt)
        {
          /* Act */
          const auto actual = skip_json5_whitespace(source);
          /* Expect */
          EXPECT_FALSE(actual.has_value());
          EXPECT_EQ(actual.value_if(), nullptr);
          ASSERT_NE(actual.error_if(), nullptr);
          EXPECT_EQ(actual.error_if()->code, adaptive_json5::error_code::encoding);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, 4U + trivia.size());
          EXPECT_EQ(source.offset(), 4U);
          EXPECT_EQ(bytes, before);
        }
      }
    }
  }

  /* ================================= End Test_J5_009 ================================= */
} // namespace
