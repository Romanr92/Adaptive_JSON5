// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/hex_escape.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /* =================================== Test_J5_016 =================================== */

  static_assert(noexcept(hex_digit_value(char32_t{})));
  static_assert(noexcept(consume_hex_escape(std::declval<cursor&>(), syntax_mode::json5)));

  /* Verify every admitted ASCII hex character has its independent numeric value.
   * Arrange: List all 22 admitted spellings with literal expected values.
   * Act: Classify each spelling.
   * Expect: A present exact value, including zero and both letter cases.
   */
  TEST(J5_016_HexDigit, MapsAllAsciiHexSpellings)
  {
    /* Arrange */
    constexpr std::u32string_view spellings = U"0123456789abcdefABCDEF";
    constexpr std::array<char16_t, 22> expected{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                                11, 12, 13, 14, 15, 10, 11, 12, 13, 14, 15};
    for (std::size_t index = 0; index < spellings.size(); ++index)
    {
      /* Act */
      const auto actual = hex_digit_value(spellings[index]);
      /* Expect */
      ASSERT_TRUE(actual.has_value()) << index;
      EXPECT_EQ(*actual, expected[index]) << index;
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify classification admits only ASCII hexadecimal characters.
   * Arrange: Select every other byte value and representative non-ASCII scalars.
   * Act: Classify without locale-dependent facilities.
   * Expect: No value for any nonhex character or out-of-domain scalar.
   */
  TEST(J5_016_HexDigit, RejectsNonhexAndUnicodeLookalikes)
  {
    /* Arrange */
    constexpr std::u32string_view admitted = U"0123456789abcdefABCDEF";
    for (char32_t scalar = 0; scalar <= 255; ++scalar)
    {
      if (admitted.find(scalar) != std::u32string_view::npos)
      {
        continue;
      }
      /* Act */
      const auto actual = hex_digit_value(scalar);
      /* Expect */
      EXPECT_FALSE(actual.has_value()) << static_cast<unsigned int>(scalar);
    }
    for (char32_t scalar : {char32_t{0x0660}, char32_t{0xFF10}, char32_t{0xFF21}, char32_t{0xFF41}, char32_t{0xD800},
                            char32_t{0x1F3BC}, std::numeric_limits<char32_t>::max()})
    {
      EXPECT_FALSE(hex_digit_value(scalar).has_value());
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Literal hex escape spellings and independent expected unit values.
   * @par Requirements
   * P016; J5-016.
   */
  struct hex_case
  {
      const char* name;     // Diagnostic parameter name.
      std::string spelling; // Input x followed by two ASCII hex digits.
      char16_t expected;    // Expected decoded UTF-16 unit.
  };
  class J5_016_HexEscape : public ::testing::TestWithParam<hex_case>
  {
  };

  /* Verify exact decoded content, bounded reads and strict-mode rejection.
   * Arrange: Place each literal escape at a nonzero offset with an unread suffix.
   * Act: Decode using JSON5 and strict JSON, then an exact-length JSON5 view.
   * Expect: Exact unit and three-byte consumption, or strict syntax at x.
   */
  TEST_P(J5_016_HexEscape, DecodesExactUnitAndPreservesSuffix)
  {
    /* Arrange */
    const auto& param = GetParam();
    const std::string input = std::string("X\\") + param.spelling + "Z";
    const auto before = input;
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_hex_escape(source, mode);
      /* Expect */
      if (mode == syntax_mode::json5)
      {
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ(**actual.value_if(), param.expected);
        EXPECT_EQ(source.offset(), 5u);
        EXPECT_EQ(*source.peek(), 'Z');
      }
      else
      {
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::syntax);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 2u);
        EXPECT_EQ(source.offset(), 2u);
      }
    }
    cursor bounded({input.data(), 5});
    bounded.advance(2);
    const auto exact = consume_hex_escape(bounded, syntax_mode::json5);
    ASSERT_TRUE(exact.has_value());
    ASSERT_TRUE(exact.value_if()->has_value());
    EXPECT_EQ(**exact.value_if(), param.expected);
    EXPECT_EQ(bounded.offset(), 5u);
    EXPECT_FALSE(bounded.peek().has_value());
    EXPECT_EQ(input, before);
  }
  INSTANTIATE_TEST_SUITE_P(Spellings, J5_016_HexEscape,
                           ::testing::Values(hex_case{"Nul", "x00", 0x00}, hex_case{"LowNibble", "x0f", 0x0F},
                                             hex_case{"HighNibble", "xf0", 0xF0}, hex_case{"Ascii", "x41", 0x41},
                                             hex_case{"Quote", "x22", 0x22}, hex_case{"Backslash", "x5C", 0x5C},
                                             hex_case{"Upper", "xAF", 0xAF}, hex_case{"Lower", "xaf", 0xAF},
                                             hex_case{"Mixed", "xaF", 0xAF}, hex_case{"Max", "xFF", 0xFF}),
                           [](const ::testing::TestParamInfo<hex_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Malformed escape suffixes and independent failure locations.
   * @par Requirements
   * P016; J5-016.
   */
  struct invalid_hex_case
  {
      const char* name;     // Diagnostic parameter name.
      std::string spelling; // Input escape suffix beginning with x.
      error_code code;      // Expected syntax or encoding category.
      std::size_t relative; // Expected byte offset relative to x.
  };
  class J5_016_HexFailure : public ::testing::TestWithParam<invalid_hex_case>
  {
  };

  /* Verify malformed escapes never publish a partial unit or advance source.
   * Arrange: Place malformed hex digits or UTF-8 after a nonzero prefix.
   * Act: Decode a recognized x escape in JSON5.
   * Expect: Exact category and original offset with source still pointing at x.
   */
  TEST_P(J5_016_HexFailure, ReportsLocatedErrorAndRollsBack)
  {
    /* Arrange */
    const auto& param = GetParam();
    const std::string input = std::string("X\\") + param.spelling;
    const auto before = input;
    cursor source({input.data(), input.size()});
    source.advance(2);
    /* Act */
    const auto actual = consume_hex_escape(source, syntax_mode::json5);
    /* Expect */
    ASSERT_FALSE(actual.has_value());
    ASSERT_NE(actual.error_if(), nullptr);
    EXPECT_EQ(actual.error_if()->code, param.code);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 2u + param.relative);
    EXPECT_EQ(source.offset(), 2u);
    EXPECT_EQ(*source.peek(), 'x');
    EXPECT_EQ(input, before);
  }
  INSTANTIATE_TEST_SUITE_P(
    Malformed, J5_016_HexFailure,
    ::testing::Values(invalid_hex_case{"MissingBoth", "x", error_code::syntax, 1},
                      invalid_hex_case{"MissingSecond", "x4", error_code::syntax, 2},
                      invalid_hex_case{"FirstNonhex", "xG1", error_code::syntax, 1},
                      invalid_hex_case{"SecondNonhex", "x1g", error_code::syntax, 2},
                      invalid_hex_case{"Quote", "x4\"", error_code::syntax, 2},
                      invalid_hex_case{"Space", "x 1", error_code::syntax, 1},
                      invalid_hex_case{"Newline", "x4\n", error_code::syntax, 2},
                      invalid_hex_case{"EmbeddedZero", std::string("x\0F", 3), error_code::syntax, 1},
                      invalid_hex_case{"FullwidthDigit", "x\xEF\xBC\x90", error_code::syntax, 1},
                      invalid_hex_case{"Supplementary", "x4\xF0\x9F\x8E\xBC", error_code::syntax, 2},
                      invalid_hex_case{"Continuation", "x\x80", error_code::encoding, 1},
                      invalid_hex_case{"Overlong", "x4\xC0\xAF", error_code::encoding, 2},
                      invalid_hex_case{"Surrogate", "x\xED\xA0\x80", error_code::encoding, 1},
                      invalid_hex_case{"OutOfRange", "x4\xF4\x90\x80\x80", error_code::encoding, 2},
                      invalid_hex_case{"TruncatedUtf8", "x4\xE2\x82", error_code::encoding, 2}),
    [](const ::testing::TestParamInfo<invalid_hex_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify every short explicit view stops at its declared boundary.
   * Arrange: Hide valid remaining digits beyond each prefix of x41.
   * Act: Attempt decoding from the same nonzero offset.
   * Expect: EOF syntax at the view end and no advancement or hidden-byte read.
   */
  TEST(J5_016_HexEscapeBoundary, RejectsEveryTruncatedPrefix)
  {
    /* Arrange */
    const std::array<char, 6> input{'X', '\\', 'x', '4', '1', '"'};
    for (std::size_t count = 0; count < 3; ++count)
    {
      cursor source({input.data(), 2 + count});
      source.advance(2);
      /* Act */
      const auto actual = consume_hex_escape(source, syntax_mode::json5);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 2u + count);
      EXPECT_EQ(source.offset(), 2u);
    }
    cursor empty;
    const auto actual = consume_hex_escape(empty, syntax_mode::strict_json);
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, error_code::syntax);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 0u);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify unowned introducers remain available to other escape decoders.
   * Arrange: Select every byte other than lowercase x, including uppercase X.
   * Act: Attempt recognition in both syntax modes.
   * Expect: Successful no-match without consuming or validating another form.
   */
  TEST(J5_016_HexEscapeBoundary, LeavesEveryOtherIntroducerUnread)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (unsigned int byte = 0; byte <= 255; ++byte)
      {
        if (byte == 'x')
        {
          continue;
        }
        const std::array<char, 3> input{'\\', static_cast<char>(byte), 'Z'};
        cursor source({input.data(), input.size()});
        source.advance(1);
        /* Act */
        const auto actual = consume_hex_escape(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value()) << byte;
        EXPECT_FALSE(actual.value_if()->has_value()) << byte;
        EXPECT_EQ(source.offset(), 1u) << byte;
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a completed escape never consumes or validates its suffix.
   * Arrange: Follow x41 with another hex digit, quote, NUL or invalid UTF-8 byte.
   * Act: Decode exactly one escape.
   * Expect: Unit A, exactly three bytes consumed and the suffix still unread.
   */
  TEST(J5_016_HexEscapeBoundary, StopsAfterExactlyTwoDigits)
  {
    /* Arrange */
    for (const char suffix : {'4', '"', '\0', static_cast<char>(0x80)})
    {
      const std::array<char, 5> input{'\\', 'x', '4', '1', suffix};
      cursor source({input.data(), input.size()});
      source.advance(1);
      /* Act */
      const auto actual = consume_hex_escape(source, syntax_mode::json5);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(**actual.value_if(), char16_t{0x41});
      EXPECT_EQ(source.offset(), 4u);
      EXPECT_EQ(*source.peek(), suffix);
    }
  }

  /* ================================= End Test_J5_016 ================================= */
} // namespace
