// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/decimal_lexer.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  /* ==================================== Test_J5_021 ==================================== */
  static_assert(noexcept(consume_decimal(std::declval<cursor&>(), syntax_mode::json5)));
  /**
   * @brief Independent decimal grammar expectations for both syntax modes.
   * @par Requirements
   * P025; J5-021; J5-022.
   */
  struct decimal_case
  {
      const char* name;        // Input case label.
      std::string text;        // Input numeric source bytes.
      bool json5_ok;           // Expected JSON5 recognition success.
      std::size_t json5_error; // Expected JSON5 relative error byte offset on failure.
      bool json_ok;            // Expected strict JSON recognition success.
      std::size_t json_error;  // Expected strict JSON relative error byte offset on failure.
  };
  class J5_021_Decimal : public testing::TestWithParam<std::tuple<decimal_case, syntax_mode>>
  {
  };
  /* Check decimal grammar and original spans without numeric conversion.
   * Arrange: Supply each spelling after a multibyte prefix with an adjacent comma.
   * Act: Recognize one decimal in the selected mode.
   * Expect: Exact token span or located syntax error, unchanged input and atomic failure.
   */
  TEST_P(J5_021_Decimal, RecognizesOrReportsExactGrammarError)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    const bool accepted = mode == syntax_mode::json5 ? item.json5_ok : item.json_ok;
    const auto error_offset = mode == syntax_mode::json5 ? item.json5_error : item.json_error;
    const std::string input = std::string(u8"\u03B1:") + item.text + ",";
    const auto original = input;
    cursor source({input.data(), input.size()});
    source.advance(3);
    /* Act */
    auto actual = consume_decimal(source, mode);
    /* Expect */
    if (accepted)
    {
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      const auto& token = **actual.value_if();
      EXPECT_EQ(token.kind, number_kind::decimal);
      EXPECT_EQ(token.byte_offset, 3u);
      EXPECT_EQ(token.byte_length, item.text.size());
      EXPECT_EQ(source.offset(), 3 + item.text.size());
      EXPECT_EQ(source.peek(), std::optional<char>{','});
      const auto span = source.span_from(3);
      EXPECT_EQ(std::string(span.data, span.size), item.text);
    }
    else
    {
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 3 + error_offset);
      EXPECT_EQ(source.offset(), 3u);
    }
    EXPECT_EQ(input, original);
  }
  INSTANTIATE_TEST_SUITE_P(
    P025, J5_021_Decimal,
    testing::Combine(
      testing::Values(
        decimal_case{"Zero", "0", true, 0, true, 0}, decimal_case{"NegativeZero", "-0", true, 0, true, 0},
        decimal_case{"Integer", "1234567890", true, 0, true, 0}, decimal_case{"Negative", "-42", true, 0, true, 0},
        decimal_case{"Fraction", "12.50", true, 0, true, 0}, decimal_case{"ZeroFraction", "0.005", true, 0, true, 0},
        decimal_case{"NegativeFraction", "-0.0", true, 0, true, 0}, decimal_case{"Exponent", "1e2", true, 0, true, 0},
        decimal_case{"UpperExponent", "1E-2", true, 0, true, 0}, decimal_case{"ExponentPlus", "1e+2", true, 0, true, 0},
        decimal_case{"ExponentZeros", "0e000", true, 0, true, 0},
        decimal_case{"FractionExponent", "-12.50E+003", true, 0, true, 0},
        decimal_case{"Plus", "+1", true, 0, false, 0}, decimal_case{"PlusZero", "+0", true, 0, false, 0},
        decimal_case{"LeadingPoint", ".5", true, 0, false, 0},
        decimal_case{"NegativeLeadingPoint", "-.5", true, 0, false, 1},
        decimal_case{"PlusLeadingPoint", "+.5", true, 0, false, 0},
        decimal_case{"TrailingPoint", "5.", true, 0, false, 2},
        decimal_case{"TrailingPointExponent", "5.e2", true, 0, false, 2},
        decimal_case{"PointExponent", ".5e-2", true, 0, false, 0},
        decimal_case{"LeadingZero", "01", false, 1, false, 1}, decimal_case{"TwoZeros", "00", false, 1, false, 1},
        decimal_case{"NegativeLeadingZero", "-01", false, 2, false, 2},
        decimal_case{"PlusLeadingZero", "+01", false, 2, false, 0},
        decimal_case{"OctalLooking", "077", false, 1, false, 1}, decimal_case{"LonePoint", ".", false, 1, false, 0},
        decimal_case{"PointExponentMissingFraction", ".e2", false, 1, false, 0},
        decimal_case{"LoneMinus", "-", false, 1, false, 1}, decimal_case{"LonePlus", "+", false, 1, false, 0},
        decimal_case{"DoubleMinus", "--1", false, 1, false, 1}, decimal_case{"DoublePlus", "++1", false, 1, false, 0},
        decimal_case{"MinusPlus", "-+1", false, 1, false, 1}, decimal_case{"PlusMinus", "+-1", false, 1, false, 0},
        decimal_case{"MissingExponent", "1e", false, 2, false, 2},
        decimal_case{"MissingSignedExponent", "1E-", false, 3, false, 3},
        decimal_case{"MissingPlusExponent", "1e+", false, 3, false, 3},
        decimal_case{"ExponentDoubleSign", "1e--2", false, 3, false, 3},
        decimal_case{"ExponentPoint", "1e.2", false, 2, false, 2},
        decimal_case{"ExponentSpace", "1e 2", false, 2, false, 2},
        decimal_case{"TrailingPointMissingExponent", "1.e", false, 3, false, 2}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<decimal_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ----------------------------------------------------------------------------------- */
  /* Respect explicit buffer length at every prefix of a decimal spelling.
   * Arrange: Expose progressively longer views over a complete backing buffer.
   * Act: Recognize each bounded prefix in both modes.
   * Expect: Complete grammar prefixes succeed; incomplete components fail at exposed EOF.
   */
  TEST(J5_021_DecimalBoundary, RespectsEveryExplicitLengthPrefix)
  {
    /* Arrange */
    const std::string input = "-12.5e+6";
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (std::size_t length = 1; length <= input.size(); ++length)
      {
        SCOPED_TRACE(length);
        cursor source({input.data(), length});
        const bool valid = length != 1 && length != 6 && length != 7 && (length != 4 || mode == syntax_mode::json5);
        /* Act */
        auto actual = consume_decimal(source, mode);
        /* Expect */
        if (valid)
        {
          ASSERT_TRUE(actual.has_value());
          ASSERT_TRUE(actual.value_if()->has_value());
          EXPECT_EQ((**actual.value_if()).byte_length, length);
          EXPECT_EQ(source.offset(), length);
        }
        else
        {
          ASSERT_FALSE(actual.has_value());
          EXPECT_EQ(actual.error_if()->code, error_code::syntax);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, length);
          EXPECT_EQ(source.offset(), 0u);
        }
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Avoid narrowing syntactically finite spellings through machine arithmetic.
   * Arrange: Integers beyond fixed-width ranges, long fractions and huge signed exponents.
   * Act: Recognize in each syntax mode.
   * Expect: Every byte retained in the decimal span regardless of conversion range.
   */
  TEST(J5_021_DecimalRange, AcceptsLargeSpellingsWithoutConversion)
  {
    /* Arrange */
    const std::string inputs[] = {"9007199254740993",
                                  "18446744073709551616",
                                  "-9223372036854775809",
                                  std::string(4096, '9'),
                                  "0." + std::string(4096, '0') + "1",
                                  "1e" + std::string(4096, '9'),
                                  "-0e-" + std::string(4096, '9')};
    for (const auto& input : inputs)
    {
      for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
      {
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_decimal(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ((**actual.value_if()).kind, number_kind::decimal);
        EXPECT_EQ((**actual.value_if()).byte_length, input.size());
        EXPECT_EQ(source.offset(), input.size());
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Leave nonnumeric input and delegated numeric families untouched.
   * Arrange: Empty views, identifiers, trivia, punctuation and hex/non-finite spellings.
   * Act: Attempt decimal recognition without skipping input.
   * Expect: Successful no-match and unchanged cursor in both modes.
   */
  TEST(J5_021_DecimalBoundary, PreservesNoMatchAndDelegatedFamilies)
  {
    /* Arrange */
    const std::string inputs[] = {
      "",         " ",   "\n1", "true", "abc",      "[",   ",",         std::string(1, '\0'),
      u8"\u0661", "0x1", "0X",  "-0xA", "Infinity", "NaN", "-Infinity", "-NaN"};
    for (const auto& input : inputs)
    {
      for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
      {
        SCOPED_TRACE(testing::PrintToString(input));
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_decimal(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 0u);
      }
    }
    cursor source({nullptr, 0});
    auto actual = consume_decimal(source, syntax_mode::json5);
    ASSERT_TRUE(actual.has_value());
    EXPECT_FALSE(actual.value_if()->has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /* Preserve terminators without implying complete-token validation.
   * Arrange: Read-only numeric bytes followed by punctuation, trivia or explicit NUL.
   * Act: Recognize the decimal component.
   * Expect: Exact span and first suffix byte left unconsumed for later operations.
   */
  TEST(J5_021_DecimalBoundary, LeavesAdjacentDelimiterUnconsumed)
  {
    /* Arrange */
    const std::string suffixes[] = {",", "]", "}", " ", "\r\n", "/*comment*/", std::string(1, '\0')};
    for (const auto& suffix : suffixes)
    {
      for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
      {
        const std::string input = "-0.5e+2" + suffix;
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_decimal(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ((**actual.value_if()).byte_length, 7u);
        EXPECT_EQ(source.offset(), 7u);
        EXPECT_EQ(source.peek(), std::optional<char>{suffix[0]});
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Preserve encoding errors where decimal grammar requires another character.
   * Arrange: Malformed UTF-8 after a sign, exponent marker/sign or required fraction point.
   * Act: Recognize in each syntax mode.
   * Expect: Encoding error at the invalid sequence and unchanged original cursor.
   */
  TEST(J5_021_DecimalEncoding, ReportsEncodingAtRequiredDigits)
  {
    /* Arrange */
    const std::string invalid[] = {"\x80", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82"};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const std::string prefix :
           {std::string{"-"}, std::string{"1e"}, std::string{"1e+"}, std::string{"."}, std::string{"1."}})
      {
        if ((prefix == "." && mode == syntax_mode::strict_json) || (prefix == "1." && mode == syntax_mode::json5))
        {
          continue;
        }
        for (const auto& bytes : invalid)
        {
          const auto input = "!!" + prefix + bytes;
          cursor source({input.data(), input.size()});
          source.advance(2);
          /* Act */
          auto actual = consume_decimal(source, mode);
          /* Expect */
          ASSERT_FALSE(actual.has_value());
          EXPECT_EQ(actual.error_if()->code, error_code::encoding);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, 2 + prefix.size());
          EXPECT_EQ(source.offset(), 2u);
        }
      }
    }
  }
  /* ================================== End Test_J5_021 ================================== */
} // namespace
