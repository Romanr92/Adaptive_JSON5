// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/simple_escape.hpp"
#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /* =================================== Test_J5_015 =================================== */

  static_assert(noexcept(consume_simple_escape(std::declval<cursor&>(), syntax_mode::json5)));

  /**
   * @brief Independent spelling and numeric decoded-unit expectations.
   * @par Requirements
   * P015; J5-015; J5-016.
   */
  struct escape_case
  {
      const char* name; // Diagnostic parameter name.
      char spelling;    // Input byte following the backslash.
      char16_t unit;    // Expected numeric UTF-16 unit.
      bool json5_only;  // Whether strict JSON must reject the spelling.
  };
  class J5_015_SimpleEscape : public ::testing::TestWithParam<escape_case>
  {
  };

  /* Verify exact decoded units and mode-specific rejection.
   * Arrange: Place one escape spelling at a nonzero offset with a quote suffix.
   * Act: Decode it under each mode.
   * Expect: Exact numeric unit and one-byte advancement, or located strict error.
   */
  TEST_P(J5_015_SimpleEscape, DecodesExactUnitOrRejectsByMode)
  {
    /* Arrange */
    const auto& param = GetParam();
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      const std::array<char, 4> input{'X', '\\', param.spelling, '"'};
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_simple_escape(source, mode);
      /* Expect */
      const bool accepted = mode == syntax_mode::json5 || !param.json5_only;
      ASSERT_EQ(actual.has_value(), accepted);
      if (accepted)
      {
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ(**actual.value_if(), param.unit);
        EXPECT_EQ(source.offset(), 3u);
        EXPECT_EQ(*source.peek(), '"');
      }
      else
      {
        ASSERT_NE(actual.error_if(), nullptr);
        EXPECT_EQ(actual.error_if()->code, error_code::syntax);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 2u);
        EXPECT_EQ(source.offset(), 2u);
      }
      EXPECT_EQ(input[2], param.spelling);
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    Spellings, J5_015_SimpleEscape,
    ::testing::Values(escape_case{"DoubleQuote", '"', 0x0022, false}, escape_case{"SingleQuote", '\'', 0x0027, true},
                      escape_case{"Backslash", '\\', 0x005C, false}, escape_case{"Slash", '/', 0x002F, false},
                      escape_case{"Backspace", 'b', 0x0008, false}, escape_case{"FormFeed", 'f', 0x000C, false},
                      escape_case{"LineFeed", 'n', 0x000A, false}, escape_case{"CarriageReturn", 'r', 0x000D, false},
                      escape_case{"Tab", 't', 0x0009, false}, escape_case{"VerticalTab", 'v', 0x000B, true},
                      escape_case{"Nul", '0', 0x0000, true}),
    [](const ::testing::TestParamInfo<escape_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify a missing escape suffix returns a located syntax error.
   * Arrange: Select a default empty cursor and a cursor at EOF after a backslash.
   * Act: Attempt decoding in both modes.
   * Expect: Syntax at the current end offset with no cursor change.
   */
  TEST(J5_015_SimpleEscapeBoundary, RejectsEofWithoutAdvancing)
  {
    /* Arrange */
    const std::array<char, 2> input{'X', '\\'};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto view : {input_view{}, input_view{input.data(), input.size()}})
      {
        cursor source(view);
        source.advance(view.size);
        const auto start = source.offset();
        /* Act */
        const auto actual = consume_simple_escape(source, mode);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::syntax);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, start);
        EXPECT_EQ(source.offset(), start);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a final one-byte escape spelling needs no terminator or following byte.
   * Arrange: Use exact-length input ending at the escape suffix.
   * Act: Decode each JSON-compatible simple escape in both modes.
   * Expect: Correct unit and cursor at EOF; no enclosing-string success is claimed.
   */
  TEST(J5_015_SimpleEscapeBoundary, AcceptsExactLengthSuffixWithoutTerminator)
  {
    /* Arrange */
    const std::array<escape_case, 8> cases{{{"Quote", '"', 0x22, false},
                                            {"Slash", '/', 0x2F, false},
                                            {"Backslash", '\\', 0x5C, false},
                                            {"Backspace", 'b', 0x08, false},
                                            {"FormFeed", 'f', 0x0C, false},
                                            {"Lf", 'n', 0x0A, false},
                                            {"Cr", 'r', 0x0D, false},
                                            {"Tab", 't', 0x09, false}}};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& item : cases)
      {
        const std::array<char, 2> input{'\\', item.spelling};
        cursor source({input.data(), input.size()});
        source.advance(1);
        /* Act */
        const auto actual = consume_simple_escape(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value()) << item.name;
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ(**actual.value_if(), item.unit);
        EXPECT_EQ(source.offset(), input.size());
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify escaped quotes are decoded before returning to boundary recognition.
   * Arrange: Use a double-quoted string containing one escaped double quote.
   * Act: Call the opener, escape handoff, simple decoder and closing recognizer.
   * Expect: The escaped quote is content and only the final quote closes.
   */
  TEST(J5_015_SimpleEscapeBoundary, SeparatesEscapedQuoteFromClosingQuote)
  {
    /* Arrange */
    const std::array<char, 5> input{'"', '\\', '"', '"', 'Z'};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      /* Act */
      const auto opening = consume_string_opening(source, mode);
      ASSERT_TRUE(opening.has_value());
      ASSERT_TRUE(opening.value_if()->has_value());
      const auto handoff = consume_string_piece(source, **opening.value_if(), mode);
      const auto escaped = consume_simple_escape(source, mode);
      const auto closing = consume_string_piece(source, **opening.value_if(), mode);
      /* Expect */
      ASSERT_TRUE(handoff.has_value());
      EXPECT_EQ(handoff.value_if()->kind, string_piece_kind::escape_start);
      ASSERT_TRUE(escaped.has_value());
      ASSERT_TRUE(escaped.value_if()->has_value());
      EXPECT_EQ(**escaped.value_if(), char16_t{0x22});
      ASSERT_TRUE(closing.has_value());
      EXPECT_EQ(closing.value_if()->kind, string_piece_kind::closing_quote);
      EXPECT_EQ(closing.value_if()->byte_offset, 3u);
      EXPECT_EQ(source.offset(), 4u);
      EXPECT_EQ(*source.peek(), 'Z');
    }
  }

  /* ================================= End Test_J5_015 ================================= */

  /* =================================== Test_J5_016 =================================== */

  class J5_016_ZeroDigit : public ::testing::TestWithParam<char>
  {
  };

  /* Verify zero cannot be followed by any decimal digit.
   * Arrange: Place zero and a selected decimal digit after a backslash.
   * Act: Decode in JSON5 and strict JSON.
   * Expect: JSON5 reports the following digit; strict JSON rejects zero itself.
   * Neither failure consumes any part of the suffix.
   */
  TEST_P(J5_016_ZeroDigit, RejectsEveryDecimalLookaheadAtomically)
  {
    /* Arrange */
    const std::array<char, 5> input{'X', '\\', '0', GetParam(), '"'};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_simple_escape(source, mode);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, mode == syntax_mode::json5 ? 3u : 2u);
      EXPECT_EQ(source.offset(), 2u);
      EXPECT_EQ(*source.peek(), '0');
      EXPECT_EQ(*source.peek(1), GetParam());
    }
  }
  INSTANTIATE_TEST_SUITE_P(Digits, J5_016_ZeroDigit,
                           ::testing::Values('0', '1', '2', '3', '4', '5', '6', '7', '8', '9'));

  /* ----------------------------------------------------------------------------------- */

  /* Verify nondigit lookahead permits NUL without consuming the following byte.
   * Arrange: Follow zero with quote, punctuation, letters, whitespace or raw zero.
   * Act: Decode the zero escape in JSON5.
   * Expect: A present zero-valued unit, one-byte advance and intact suffix.
   */
  TEST(J5_016_ZeroEscape, AcceptsNondigitLookaheadWithoutConsumingIt)
  {
    /* Arrange */
    for (const char next : {'"', '\'', '/', ':', 'a', 'e', 'x', 'u', '\\', ' ', '\n', '\0'})
    {
      const std::array<char, 3> input{'\\', '0', next};
      cursor source({input.data(), input.size()});
      source.advance(1);
      /* Act */
      const auto actual = consume_simple_escape(source, syntax_mode::json5);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(**actual.value_if(), char16_t{});
      EXPECT_EQ(source.offset(), 2u);
      EXPECT_EQ(*source.peek(), next);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify zero lookahead respects the explicit input length.
   * Arrange: Hide a digit beyond the view ending immediately after zero.
   * Act: Decode NUL, then ask the boundary reader for the missing closing quote.
   * Expect: Component escape success followed by EOF syntax for the open string.
   */
  TEST(J5_016_ZeroEscape, HonorsBoundedLookaheadAndStillRequiresClosingQuote)
  {
    /* Arrange */
    const std::array<char, 4> input{'"', '\\', '0', '1'};
    cursor source({input.data(), 3});
    const auto opening = consume_string_opening(source, syntax_mode::json5);
    ASSERT_TRUE(opening.has_value());
    ASSERT_TRUE(opening.value_if()->has_value());
    ASSERT_TRUE(consume_string_piece(source, **opening.value_if(), syntax_mode::json5).has_value());
    /* Act */
    const auto actual = consume_simple_escape(source, syntax_mode::json5);
    const auto closing = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    EXPECT_EQ(**actual.value_if(), char16_t{});
    ASSERT_FALSE(closing.has_value());
    EXPECT_EQ(closing.error_if()->code, error_code::syntax);
    EXPECT_TRUE(closing.error_if()->has_byte_offset);
    EXPECT_EQ(closing.error_if()->byte_offset, 3u);
    EXPECT_EQ(source.offset(), 3u);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify all unowned spellings remain available to later escape recognizers.
   * Arrange: Select every byte except the eleven simple escape spellings.
   * Act: Attempt simple recognition in each mode.
   * Expect: Successful no-match and no advance, including x/u, digits, raw NUL,
   * line terminators and malformed UTF-8 leading bytes. No validity is claimed.
   */
  TEST(J5_016_SimpleEscapeHandoff, LeavesEveryOtherByteUnread)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (unsigned int byte = 0; byte <= 255; ++byte)
      {
        const char spelling = static_cast<char>(byte);
        if (spelling == '"' || spelling == '\'' || spelling == '\\' || spelling == '/' || spelling == 'b' ||
            spelling == 'f' || spelling == 'n' || spelling == 'r' || spelling == 't' || spelling == 'v' ||
            spelling == '0')
        {
          continue;
        }
        const std::array<char, 2> input{'\\', spelling};
        cursor source({input.data(), input.size()});
        source.advance(1);
        /* Act */
        const auto actual = consume_simple_escape(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value()) << byte;
        EXPECT_FALSE(actual.value_if()->has_value()) << byte;
        EXPECT_EQ(source.offset(), 1u) << byte;
        EXPECT_EQ(*source.peek(), spelling);
      }
    }
  }

  /* ================================= End Test_J5_016 ================================= */
} // namespace
