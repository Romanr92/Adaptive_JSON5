#include "adaptive_json5/detail/lexer/number/non_finite_lexer.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  /* ==================================== Test_J5_022 ==================================== */
  static_assert(noexcept(consume_non_finite(std::declval<cursor&>(), syntax_mode::json5)));
  /**
   * @brief Independent non-finite spelling and diagnostic expectations.
   * @par Requirements
   * P027; J5-021; J5-022.
   */
  struct non_finite_case
  {
      const char* name;   // Input case label.
      std::string text;   // Input source bytes.
      bool accepted;      // Expected JSON5 recognition success.
      number_kind kind;   // Expected numeric kind on success.
      std::size_t offset; // Expected JSON5 syntax error offset relative to token start.
  };
  class J5_022_NonFinite : public testing::TestWithParam<std::tuple<non_finite_case, syntax_mode>>
  {
  };
  /* Recognize exact signed non-finite spellings and reject malformed attempts.
   * Arrange: Supply literal source expectations after a multibyte prefix.
   * Act: Recognize in the selected syntax mode.
   * Expect: Exact kind/span or located syntax failure with complete cursor rollback.
   */
  TEST_P(J5_022_NonFinite, RecognizesOrReportsLocatedFailure)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    const std::string input = std::string(u8"\u03B1:") + item.text;
    const auto original = input;
    cursor source({input.data(), input.size()});
    source.advance(3);
    /* Act */
    auto actual = consume_non_finite(source, mode);
    /* Expect */
    if (mode == syntax_mode::json5 && item.accepted)
    {
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      const auto& token = **actual.value_if();
      EXPECT_EQ(token.kind, item.kind);
      EXPECT_EQ(token.byte_offset, 3u);
      EXPECT_EQ(token.byte_length, item.text.size());
      EXPECT_EQ(source.offset(), input.size());
      const auto span = source.span_from(3);
      EXPECT_EQ(std::string(span.data, span.size), item.text);
    }
    else
    {
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, mode == syntax_mode::strict_json ? 3u : 3 + item.offset);
      EXPECT_EQ(source.offset(), 3u);
    }
    EXPECT_EQ(input, original);
  }
  INSTANTIATE_TEST_SUITE_P(
    P027, J5_022_NonFinite,
    testing::Combine(
      testing::Values(non_finite_case{"Infinity", "Infinity", true, number_kind::infinity, 0},
                      non_finite_case{"PlusInfinity", "+Infinity", true, number_kind::infinity, 0},
                      non_finite_case{"MinusInfinity", "-Infinity", true, number_kind::infinity, 0},
                      non_finite_case{"NaN", "NaN", true, number_kind::nan, 0},
                      non_finite_case{"PlusNaN", "+NaN", true, number_kind::nan, 0},
                      non_finite_case{"MinusNaN", "-NaN", true, number_kind::nan, 0},
                      non_finite_case{"UpperInfinity", "INFINITY", false, number_kind::infinity, 1},
                      non_finite_case{"UpperNan", "NAN", false, number_kind::nan, 1},
                      non_finite_case{"LowerLastNan", "Nan", false, number_kind::nan, 2},
                      non_finite_case{"WrongInfinityLast", "InfinitY", false, number_kind::infinity, 7},
                      non_finite_case{"SignedMismatch", "-InFinity", false, number_kind::infinity, 3},
                      non_finite_case{"InfinitySuffix", "Infinityx", false, number_kind::infinity, 8},
                      non_finite_case{"NanDigit", "NaN0", false, number_kind::nan, 3},
                      non_finite_case{"InfinityUnderscore", "Infinity_", false, number_kind::infinity, 8},
                      non_finite_case{"NanDollar", "NaN$", false, number_kind::nan, 3},
                      non_finite_case{"InfinityFraction", "Infinity.0", false, number_kind::infinity, 8},
                      non_finite_case{"NanPoint", "-NaN.", false, number_kind::nan, 4},
                      non_finite_case{"EscapedSpelling", R"(Inf\u0069nity)", false, number_kind::infinity, 3},
                      non_finite_case{"EscapedSuffix", R"(NaN\u0061)", false, number_kind::nan, 3},
                      non_finite_case{"SpaceWithin", "In finity", false, number_kind::infinity, 2},
                      non_finite_case{"CommentWithin", "Na/*x*/N", false, number_kind::nan, 2},
                      non_finite_case{"UnicodeSuffix", u8"Infinity\u03B1", false, number_kind::infinity, 8},
                      non_finite_case{"CombiningSuffix", u8"NaN\u0301", false, number_kind::nan, 3},
                      non_finite_case{"JoinerSuffix", u8"NaN\u200D", false, number_kind::nan, 3},
                      non_finite_case{"NonJoinerSuffix", u8"Infinity\u200C", false, number_kind::infinity, 8},
                      non_finite_case{"UnicodeLookalike", u8"N\u0430N", false, number_kind::nan, 1},
                      non_finite_case{"NulWithin", std::string("Na\0N", 4), false, number_kind::nan, 2}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<non_finite_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ----------------------------------------------------------------------------------- */
  /* Respect every exact-length prefix of all six permitted spellings.
   * Arrange: Expose each prefix of a complete backing buffer at a nonzero offset.
   * Act: Recognize in both modes without access to later backing bytes.
   * Expect: No-match before I/N, EOF error for incomplete words or exact final token.
   */
  TEST(J5_022_NonFiniteBoundary, ChecksEveryBoundedPrefix)
  {
    /* Arrange */
    const std::string spellings[] = {"Infinity", "+Infinity", "-Infinity", "NaN", "+NaN", "-NaN"};
    for (const auto& spelling : spellings)
    {
      const auto input = "!!" + spelling;
      const std::size_t sign_bytes = (spelling[0] == '+' || spelling[0] == '-') ? 1u : 0u;
      for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
      {
        for (std::size_t length = 0; length <= spelling.size(); ++length)
        {
          SCOPED_TRACE(spelling);
          SCOPED_TRACE(length);
          cursor source({input.data(), 2 + length});
          source.advance(2);
          /* Act */
          auto actual = consume_non_finite(source, mode);
          /* Expect */
          if (length <= sign_bytes)
          {
            ASSERT_TRUE(actual.has_value());
            EXPECT_FALSE(actual.value_if()->has_value());
            EXPECT_EQ(source.offset(), 2u);
          }
          else if (mode == syntax_mode::strict_json || length < spelling.size())
          {
            ASSERT_FALSE(actual.has_value());
            EXPECT_EQ(actual.error_if()->code, error_code::syntax);
            EXPECT_TRUE(actual.error_if()->has_byte_offset);
            EXPECT_EQ(actual.error_if()->byte_offset, mode == syntax_mode::strict_json ? 2u : 2 + length);
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
  }
  /* ----------------------------------------------------------------------------------- */
  /* Preserve names and unrelated numeric spellings for caller dispatch.
   * Arrange: Lowercase names, other tokens, missing/doubled signs and empty views.
   * Act: Attempt non-finite recognition in both modes.
   * Expect: Successful no-match without cursor movement; no value acceptance is implied.
   */
  TEST(J5_022_NonFiniteBoundary, LeavesOtherStartsUnconsumed)
  {
    /* Arrange */
    const std::string inputs[] = {"",
                                  "infinity",
                                  "nan",
                                  "nAn",
                                  "+infinity",
                                  "-nan",
                                  "true",
                                  "0",
                                  "-1",
                                  "0x1",
                                  ".5",
                                  " Infinity",
                                  "+",
                                  "-",
                                  "++Infinity",
                                  "--NaN",
                                  "+-NaN",
                                  "-+Infinity",
                                  R"(\u0049nfinity)",
                                  std::string(1, '\0')};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& input : inputs)
      {
        SCOPED_TRACE(input);
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_non_finite(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 0u);
      }
    }
    cursor empty({nullptr, 0});
    auto actual = consume_non_finite(empty, syntax_mode::json5);
    ASSERT_TRUE(actual.has_value());
    EXPECT_FALSE(actual.value_if()->has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /* Leave complete terminators untouched after exact non-finite names.
   * Arrange: Each signed/unsigned form followed by punctuation, trivia or explicit NUL.
   * Act: Recognize the JSON5 component.
   * Expect: Kind and span match the word/sign; all suffix bytes remain unconsumed.
   */
  TEST(J5_022_NonFiniteBoundary, PreservesAdjacentTerminator)
  {
    /* Arrange */
    const std::string spellings[] = {"Infinity", "+Infinity", "-Infinity", "NaN", "+NaN", "-NaN"};
    const std::string endings[] = {",",           "]",         "}",        " ",        "\r\n",
                                   "/*comment*/", "//comment", u8"\u00A0", u8"\u2028", std::string(1, '\0')};
    for (const auto& spelling : spellings)
    {
      for (const auto& ending : endings)
      {
        const auto input = spelling + ending;
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_non_finite(source, syntax_mode::json5);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ((**actual.value_if()).kind,
                  spelling.find("Infinity") != std::string::npos ? number_kind::infinity : number_kind::nan);
        EXPECT_EQ((**actual.value_if()).byte_length, spelling.size());
        EXPECT_EQ(source.offset(), spelling.size());
        EXPECT_EQ(source.remaining(), ending.size());
        EXPECT_EQ(source.peek(), std::optional<char>{ending[0]});
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Preserve malformed encoding categories within words and at their boundary.
   * Arrange: Invalid/truncated UTF-8 after an attempted word start or complete spelling.
   * Act: Recognize with JSON5 syntax.
   * Expect: Exact original encoding error offset and complete cursor rollback.
   */
  TEST(J5_022_NonFiniteEncoding, RejectsMalformedUtf8Atomically)
  {
    /* Arrange */
    const std::string invalid[] = {"\x80", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2x",       "\xC2",
                                   "\xE2", "\xE2\x82", "\xF0",         "\xF0\x90",         "\xF0\x90\x90"};
    for (const std::string prefix :
         {std::string{"I"}, std::string{"-Inf"}, std::string{"+Na"}, std::string{"Infinity"}, std::string{"-NaN"}})
    {
      for (const auto& bytes : invalid)
      {
        const auto input = "!!" + prefix + bytes;
        cursor source({input.data(), input.size()});
        source.advance(2);
        /* Act */
        auto actual = consume_non_finite(source, syntax_mode::json5);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::encoding);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 2 + prefix.size());
        EXPECT_EQ(source.offset(), 2u);
      }
    }
  }
  /* ================================== End Test_J5_022 ================================== */
} // namespace
