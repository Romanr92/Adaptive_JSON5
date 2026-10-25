#include "adaptive_json5/detail/lexer/number/hexadecimal_lexer.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  /* ==================================== Test_J5_022 ==================================== */
  static_assert(noexcept(consume_hexadecimal(std::declval<cursor&>(), syntax_mode::json5)));
  /**
   * @brief Explicit source spellings and relative diagnostic expectations.
   * @par Requirements
   * P026; J5-021; J5-022.
   */
  struct hexadecimal_case
  {
      const char* name;   // Input case label.
      std::string text;   // Input source bytes without an adjacent delimiter.
      bool accepted;      // Expected JSON5 recognition success.
      error_code code;    // Expected JSON5 failure category.
      std::size_t offset; // Expected JSON5 failure byte offset relative to the token.
  };
  class J5_022_Hexadecimal : public testing::TestWithParam<std::tuple<hexadecimal_case, syntax_mode>>
  {
  };
  /* Recognize valid hex spellings or reject malformed components in both modes.
   * Arrange: Supply exact-length input after a multibyte prefix.
   * Act: Recognize with the selected syntax mode.
   * Expect: Exact original span or error category/offset; failures preserve cursor and input.
   */
  TEST_P(J5_022_Hexadecimal, RecognizesOrReportsLocatedFailure)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    const std::string input = std::string(u8"\u03B1:") + item.text;
    const auto original = input;
    cursor source({input.data(), input.size()});
    source.advance(3);
    /* Act */
    auto actual = consume_hexadecimal(source, mode);
    /* Expect */
    if (mode == syntax_mode::json5 && item.accepted)
    {
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      const auto& token = **actual.value_if();
      EXPECT_EQ(token.kind, number_kind::hexadecimal);
      EXPECT_EQ(token.byte_offset, 3u);
      EXPECT_EQ(token.byte_length, item.text.size());
      EXPECT_EQ(source.offset(), input.size());
      EXPECT_TRUE(source.is_at_end());
      const auto span = source.span_from(3);
      EXPECT_EQ(std::string(span.data, span.size), item.text);
    }
    else
    {
      ASSERT_FALSE(actual.has_value());
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->code, mode == syntax_mode::strict_json ? error_code::syntax : item.code);
      EXPECT_EQ(actual.error_if()->byte_offset, mode == syntax_mode::strict_json ? 3u : 3 + item.offset);
      EXPECT_EQ(source.offset(), 3u);
    }
    EXPECT_EQ(input, original);
  }
  INSTANTIATE_TEST_SUITE_P(
    P026, J5_022_Hexadecimal,
    testing::Combine(
      testing::Values(hexadecimal_case{"ZeroLower", "0x0", true, error_code::syntax, 0},
                      hexadecimal_case{"ZeroUpper", "0X0", true, error_code::syntax, 0},
                      hexadecimal_case{"PlusLower", "+0x1", true, error_code::syntax, 0},
                      hexadecimal_case{"PlusUpper", "+0XAf", true, error_code::syntax, 0},
                      hexadecimal_case{"MinusLower", "-0xFf", true, error_code::syntax, 0},
                      hexadecimal_case{"MinusUpper", "-0X10", true, error_code::syntax, 0},
                      hexadecimal_case{"NegativeZero", "-0x0", true, error_code::syntax, 0},
                      hexadecimal_case{"PlusZero", "+0X000", true, error_code::syntax, 0},
                      hexadecimal_case{"AllDigits", "0x0123456789abcdefABCDEF", true, error_code::syntax, 0},
                      hexadecimal_case{"EIsDigit", "0x1e2", true, error_code::syntax, 0},
                      hexadecimal_case{"LeadingHexZeros", "0X00001", true, error_code::syntax, 0},
                      hexadecimal_case{"MissingLower", "0x", false, error_code::syntax, 2},
                      hexadecimal_case{"MissingUpperSigned", "-0X", false, error_code::syntax, 3},
                      hexadecimal_case{"MissingPlus", "+0x", false, error_code::syntax, 3},
                      hexadecimal_case{"InvalidFirst", "0xG", false, error_code::syntax, 2},
                      hexadecimal_case{"PointBeforeDigit", "0x.1", false, error_code::syntax, 2},
                      hexadecimal_case{"SpaceBeforeDigit", "0x 1", false, error_code::syntax, 2},
                      hexadecimal_case{"SignBeforeDigit", "0x-1", false, error_code::syntax, 2},
                      hexadecimal_case{"LetterSuffix", "0x1z", false, error_code::syntax, 3},
                      hexadecimal_case{"UpperLetterSuffix", "-0X1G", false, error_code::syntax, 4},
                      hexadecimal_case{"Fraction", "0x1.5", false, error_code::syntax, 3},
                      hexadecimal_case{"TrailingPoint", "0x1.", false, error_code::syntax, 3},
                      hexadecimal_case{"Separator", "0x1_0", false, error_code::syntax, 3},
                      hexadecimal_case{"DollarSuffix", "0x1$", false, error_code::syntax, 3},
                      hexadecimal_case{"EscapeSuffix", R"(0x1\u0061)", false, error_code::syntax, 3},
                      hexadecimal_case{"UnicodeLetter", u8"0x1\u03B1", false, error_code::syntax, 3},
                      hexadecimal_case{"CombiningMark", u8"0x1\u0301", false, error_code::syntax, 3},
                      hexadecimal_case{"Joiner", u8"0x1\u200D", false, error_code::syntax, 3},
                      hexadecimal_case{"NonJoiner", u8"0x1\u200C", false, error_code::syntax, 3},
                      hexadecimal_case{"UnicodeDigit", u8"0x1\u0661", false, error_code::syntax, 3},
                      hexadecimal_case{"UnicodeFirstDigit", u8"0x\uFF11", false, error_code::syntax, 2},
                      hexadecimal_case{"NulBeforeDigit", std::string("0x\0", 3), false, error_code::syntax, 2}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<hexadecimal_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ----------------------------------------------------------------------------------- */
  /* Check every bounded prefix without reading complete backing bytes beyond the view.
   * Arrange: Expose each prefix of a signed hex number at a nonzero offset.
   * Act: Recognize in both syntax modes.
   * Expect: No-match before the full prefix, missing-digit failure, or exact success.
   */
  TEST(J5_022_HexadecimalBoundary, RespectsEveryExplicitLengthPrefix)
  {
    /* Arrange */
    const std::string input = "!!-0xAf";
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (std::size_t length = 0; length <= 5; ++length)
      {
        SCOPED_TRACE(length);
        cursor source({input.data(), 2 + length});
        source.advance(2);
        /* Act */
        auto actual = consume_hexadecimal(source, mode);
        /* Expect */
        if (length < 3)
        {
          ASSERT_TRUE(actual.has_value());
          EXPECT_FALSE(actual.value_if()->has_value());
          EXPECT_EQ(source.offset(), 2u);
        }
        else if (mode == syntax_mode::strict_json || length == 3)
        {
          ASSERT_FALSE(actual.has_value());
          EXPECT_EQ(actual.error_if()->code, error_code::syntax);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, mode == syntax_mode::strict_json ? 2u : 5u);
          EXPECT_EQ(source.offset(), 2u);
        }
        else
        {
          ASSERT_TRUE(actual.has_value());
          ASSERT_TRUE(actual.value_if()->has_value());
          EXPECT_EQ((**actual.value_if()).byte_offset, 2u);
          EXPECT_EQ((**actual.value_if()).byte_length, length);
          EXPECT_EQ(source.offset(), 2 + length);
        }
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Preserve unrelated spellings, including privately inspected signs.
   * Arrange: Empty views, decimal/non-finite names, wrong prefixes and doubled signs.
   * Act: Try hexadecimal recognition in both modes.
   * Expect: Successful no-match without cursor movement or input mutation.
   */
  TEST(J5_022_HexadecimalBoundary, LeavesOtherSpellingsUntouched)
  {
    /* Arrange */
    const std::string inputs[] = {
      "",         "0",    "-0",   "+0",    "42",    "0.5",   "0b1",   "0o7", "00x1", "x1",
      "Infinity", "-NaN", " 0x1", "--0x1", "+-0x1", "-+0x1", "++0x1", "-",   "+",    std::string(1, '\0')};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& input : inputs)
      {
        SCOPED_TRACE(input);
        const auto original = input;
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_hexadecimal(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 0u);
        EXPECT_EQ(input, original);
      }
    }
    cursor empty({nullptr, 0});
    auto actual = consume_hexadecimal(empty, syntax_mode::json5);
    ASSERT_TRUE(actual.has_value());
    EXPECT_FALSE(actual.value_if()->has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /* Leave terminating bytes intact after successful recognition.
   * Arrange: A signed hex token adjacent to punctuation, trivia, Unicode space or NUL.
   * Act: Recognize the JSON5 hexadecimal component.
   * Expect: Original token span only, with the complete terminator still present.
   */
  TEST(J5_022_HexadecimalBoundary, PreservesAdjacentTerminator)
  {
    /* Arrange */
    const std::string endings[] = {",",           "]",         "}",        " ",        "\r\n",
                                   "/*comment*/", "//comment", u8"\u00A0", u8"\u2028", std::string(1, '\0')};
    for (const auto& ending : endings)
    {
      const std::string input = "+0XAf" + ending;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_hexadecimal(source, syntax_mode::json5);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ((**actual.value_if()).kind, number_kind::hexadecimal);
      EXPECT_EQ((**actual.value_if()).byte_length, 5u);
      EXPECT_EQ(source.offset(), 5u);
      EXPECT_EQ(source.remaining(), ending.size());
      EXPECT_EQ(source.peek(), std::optional<char>{ending[0]});
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Reject malformed encoding before the first digit and after accepted digits.
   * Arrange: Invalid or truncated UTF-8 at original nonzero byte positions.
   * Act: Recognize a JSON5 hex token.
   * Expect: Exact encoding error location and complete cursor rollback.
   */
  TEST(J5_022_HexadecimalEncoding, RejectsMalformedUtf8Atomically)
  {
    /* Arrange */
    const std::string invalid[] = {"\x80", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2x",       "\xC2",
                                   "\xE2", "\xE2\x82", "\xF0",         "\xF0\x90",         "\xF0\x90\x90"};
    for (const std::string prefix : {std::string{"0x"}, std::string{"-0X1a"}})
    {
      for (const auto& bytes : invalid)
      {
        const auto input = "!!" + prefix + bytes;
        cursor source({input.data(), input.size()});
        source.advance(2);
        /* Act */
        auto actual = consume_hexadecimal(source, syntax_mode::json5);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::encoding);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 2 + prefix.size());
        EXPECT_EQ(source.offset(), 2u);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Accept source spellings without a fixed-width integer or floating conversion.
   * Arrange: Values beyond 64-bit ranges and 4096 hexadecimal digits.
   * Act: Recognize exact-length JSON5 inputs.
   * Expect: Complete hexadecimal token spans with original sign and spelling preserved.
   */
  TEST(J5_022_HexadecimalRange, AcceptsLargeSpellingsWithoutConversion)
  {
    /* Arrange */
    const std::string inputs[] = {"0x10000000000000000", "-0x8000000000000001", "+0X" + std::string(4096, 'F')};
    for (const auto& input : inputs)
    {
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_hexadecimal(source, syntax_mode::json5);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ((**actual.value_if()).kind, number_kind::hexadecimal);
      EXPECT_EQ((**actual.value_if()).byte_length, input.size());
      EXPECT_EQ(source.offset(), input.size());
      const auto span = source.span_from(0);
      EXPECT_EQ(std::string(span.data, span.size), input);
    }
  }
  /* ================================== End Test_J5_022 ================================== */
} // namespace
