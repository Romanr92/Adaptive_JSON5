// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/hex_escape.hpp"
#include "adaptive_json5/non_escape.hpp"
#include "adaptive_json5/unicode_escape.hpp"
#include "adaptive_json5/utf16.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /* =================================== Test_J5_016 =================================== */

  static_assert(noexcept(consume_non_escape(std::declval<cursor&>(), syntax_mode::json5)));

  /**
   * @brief Independent source spellings and unchanged scalar expectations.
   * @par Requirements
   * P018; J5-016.
   */
  struct non_escape_case
  {
      const char* name;  // Diagnostic parameter name.
      std::string bytes; // Input UTF-8 bytes immediately after backslash.
      char32_t scalar;   // Expected unchanged Unicode scalar.
  };
  class J5_016_NonEscape : public ::testing::TestWithParam<non_escape_case>
  {
  };

  /* Verify JSON5 drops only the backslash and preserves the following character.
   * Arrange: Put one character after a backslash at a nonzero offset.
   * Act: Decode in both modes and repeat with an exact-length JSON5 view.
   * Expect: Exact JSON5 scalar/span/suffix or strict syntax with no advancement.
   */
  TEST_P(J5_016_NonEscape, PreservesScalarOrRejectsStrictMode)
  {
    /* Arrange */
    const auto& param = GetParam();
    const std::string input = std::string("X\\") + param.bytes + "Z";
    const auto before = input;
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_non_escape(source, mode);
      /* Expect */
      if (mode == syntax_mode::json5)
      {
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        const auto& character = **actual.value_if();
        EXPECT_EQ(character.value, param.scalar);
        EXPECT_EQ(character.byte_offset, 2u);
        EXPECT_EQ(character.byte_length, param.bytes.size());
        EXPECT_EQ(source.offset(), 2u + param.bytes.size());
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
      EXPECT_EQ(input, before);
    }
    cursor bounded({param.bytes.data(), param.bytes.size()});
    const auto exact = consume_non_escape(bounded, syntax_mode::json5);
    ASSERT_TRUE(exact.has_value());
    ASSERT_TRUE(exact.value_if()->has_value());
    EXPECT_EQ(exact.value_if()->value().value, param.scalar);
    EXPECT_EQ(bounded.offset(), param.bytes.size());
  }
  INSTANTIATE_TEST_SUITE_P(
    Characters, J5_016_NonEscape,
    ::testing::Values(non_escape_case{"LetterA", "a", 0x61}, non_escape_case{"LetterE", "e", 0x65},
                      non_escape_case{"Slash", "/", 0x2F}, non_escape_case{"Space", " ", 0x20},
                      non_escape_case{"UpperX", "X", 0x58}, non_escape_case{"UpperU", "U", 0x55},
                      non_escape_case{"Punctuation", "!", 0x21}, non_escape_case{"Tab", "\t", 0x09},
                      non_escape_case{"Nul", std::string(1, '\0'), 0}, non_escape_case{"TwoBytes", "\xC2\xA0", 0xA0},
                      non_escape_case{"ThreeBytes", "\xE2\x82\xAC", 0x20AC},
                      non_escape_case{"Supplementary", "\xF0\x9F\x8E\xBC", 0x1F3BC}),
    [](const ::testing::TestParamInfo<non_escape_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  class J5_016_NonEscapeDigit : public ::testing::TestWithParam<char>
  {
  };

  /* Verify every escaped nonzero decimal digit is rejected.
   * Arrange: Select one digit from one through nine after a nonzero prefix.
   * Act: Decode using each syntax mode.
   * Expect: Located syntax error without consumption, including eight and nine.
   */
  TEST_P(J5_016_NonEscapeDigit, RejectsWithoutAdvancing)
  {
    /* Arrange */
    const std::array<char, 4> input{'X', '\\', GetParam(), 'Z'};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_non_escape(source, mode);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 2u);
      EXPECT_EQ(source.offset(), 2u);
      EXPECT_EQ(*source.peek(), GetParam());
    }
  }
  INSTANTIATE_TEST_SUITE_P(Digits, J5_016_NonEscapeDigit,
                           ::testing::Values('1', '2', '3', '4', '5', '6', '7', '8', '9'));

  /* ----------------------------------------------------------------------------------- */

  /* Verify special spellings and all line terminators remain for their owners.
   * Arrange: List the special escape bytes and five line-terminator sequences.
   * Act: Attempt non-escape recognition under both syntax modes.
   * Expect: Successful no-match with the complete original input unread.
   */
  TEST(J5_016_NonEscapeHandoff, DefersSpecialFormsAndLineTerminators)
  {
    /* Arrange */
    const std::array<std::string, 17> spellings{
      "'", "\"", "\\", "b", "f", "n", "r", "t", "v", "0", "x", "u", "\n", "\r", "\r\n", "\xE2\x80\xA8", "\xE2\x80\xA9"};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& spelling : spellings)
      {
        const std::string input = std::string("X\\") + spelling + "Z";
        cursor source({input.data(), input.size()});
        source.advance(2);
        /* Act */
        const auto actual = consume_non_escape(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 2u);
        EXPECT_EQ(source.remaining(), spelling.size() + 1);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  class J5_016_NonEscapeEncoding : public ::testing::TestWithParam<std::string>
  {
  };

  /* Verify malformed UTF-8 cannot be accepted by the fallback helper.
   * Arrange: Place malformed source sequences after an escape introducer.
   * Act: Decode using each mode.
   * Expect: Encoding at the original sequence start with cursor unchanged.
   */
  TEST_P(J5_016_NonEscapeEncoding, ReportsEncodingAndRollsBack)
  {
    /* Arrange */
    const std::string input = std::string("X\\") + GetParam();
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_non_escape(source, mode);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::encoding);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 2u);
      EXPECT_EQ(source.offset(), 2u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(Malformed, J5_016_NonEscapeEncoding,
                           ::testing::Values(std::string("\x80"), std::string("\xC0\xAF"), std::string("\xED\xA0\x80"),
                                             std::string("\xF4\x90\x80\x80"), std::string("\xE2\x28\xA1"),
                                             std::string("\xFF")));

  /* ----------------------------------------------------------------------------------- */

  /* Verify EOF and every partial UTF-8 width honor the explicit view length.
   * Arrange: Hide the remainder of valid two-, three- and four-byte characters.
   * Act: Decode each prefix, including EOF immediately after backslash.
   * Expect: EOF syntax or truncated encoding at exact offsets without advancement.
   */
  TEST(J5_016_NonEscapeBoundary, RejectsEofAndEveryTruncatedCharacterPrefix)
  {
    /* Arrange */
    const std::array<std::string, 3> spellings{"\xC2\xA0", "\xE2\x82\xAC", "\xF0\x9F\x8E\xBC"};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& spelling : spellings)
      {
        const std::string input = std::string("X\\") + spelling;
        for (std::size_t length = 0; length < spelling.size(); ++length)
        {
          cursor source({input.data(), 2 + length});
          source.advance(2);
          /* Act */
          const auto actual = consume_non_escape(source, mode);
          /* Expect */
          ASSERT_FALSE(actual.has_value());
          EXPECT_EQ(actual.error_if()->code, length == 0 ? error_code::syntax : error_code::encoding);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, 2u);
          EXPECT_EQ(source.offset(), 2u);
        }
      }
      cursor empty;
      const auto actual = consume_non_escape(empty, mode);
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 0u);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify x/u remain reserved after their owning decoders report malformed forms.
   * Arrange: Place malformed hex and Unicode suffixes after backslashes.
   * Act: Run the owning decoder, then directly check fallback recognition.
   * Expect: Original syntax failure, fallback no-match and unchanged source.
   * This is a component ownership check, not an implemented dispatcher.
   */
  TEST(J5_016_NonEscapeHandoff, CannotAcceptMalformedHexOrUnicodeAsLetters)
  {
    /* Arrange */
    for (const auto spelling : {std::string("x"), std::string("xG1"), std::string("u"), std::string("u12G4")})
    {
      const std::string input = std::string("X\\") + spelling;
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto owned =
        spelling.front() == 'x' ? consume_hex_escape(source, syntax_mode::json5) : consume_unicode_escape(source);
      const auto fallback = consume_non_escape(source, syntax_mode::json5);
      /* Expect */
      ASSERT_FALSE(owned.has_value());
      EXPECT_EQ(owned.error_if()->code, error_code::syntax);
      EXPECT_TRUE(owned.error_if()->has_byte_offset);
      ASSERT_TRUE(fallback.has_value());
      EXPECT_FALSE(fallback.value_if()->has_value());
      EXPECT_EQ(source.offset(), 2u);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a supplementary non-escape remains a scalar until UTF-16 conversion.
   * Arrange: Put a musical-score source character after a backslash.
   * Act: Consume it and pass its scalar to the existing UTF-16 encoder.
   * Expect: Exact supplementary scalar and D83C/DFBC units, never truncation.
   */
  TEST(J5_016_NonEscapeBoundary, RetainsSupplementaryScalarForStorage)
  {
    /* Arrange */
    const std::string input = "\\\xF0\x9F\x8E\xBC";
    cursor source({input.data(), input.size()});
    source.advance(1);
    /* Act */
    const auto actual = consume_non_escape(source, syntax_mode::json5);
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    const auto units = encode_utf16(actual.value_if()->value().value);
    /* Expect */
    ASSERT_TRUE(units.has_value());
    EXPECT_EQ(units.value_if()->size, 2u);
    EXPECT_EQ(units.value_if()->units[0], char16_t{0xD83C});
    EXPECT_EQ(units.value_if()->units[1], char16_t{0xDFBC});
    EXPECT_EQ(source.offset(), input.size());
  }

  /* ================================= End Test_J5_016 ================================= */
} // namespace
