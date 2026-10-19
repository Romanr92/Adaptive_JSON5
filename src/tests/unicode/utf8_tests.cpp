// SPDX-License-Identifier: Apache-2.0
// P005 contract-first units; utf8.hpp is supplied by the human developer.
#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/unicode/utf8.hpp"
#include "adaptive_json5/result.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
  using adaptive_json5::cursor;
  using adaptive_json5::decode_utf8;
  using adaptive_json5::decoded_character;
  using adaptive_json5::error_code;
  using adaptive_json5::input_view;
  using adaptive_json5::result;

  /* =================================== Test_J5_009 =================================== */

  static_assert(noexcept(decode_utf8(std::declval<cursor&>())));
  static_assert(
    std::is_same_v<decltype(decode_utf8(std::declval<cursor&>())), result<std::optional<decoded_character>>>);

  /**
   * @brief Independent, hand-authored UTF-8 boundary expectations.
   * @par Requirements
   * P005; J5-009.
   */
  struct valid_case
  {
      const char* name;       // Case description.
      std::string_view bytes; // Input bytes with explicit length.
      char32_t scalar;        // Expected Unicode scalar value.
  };

  const valid_case c_valid_cases[] = {
    {"AsciiZero", std::string_view{"\x00", 1}, static_cast<char32_t>(0x0)},
    {"AsciiMax", std::string_view{"\x7f", 1}, static_cast<char32_t>(0x7F)},
    {"TwoByteMin", std::string_view{"\xc2\x80", 2}, static_cast<char32_t>(0x80)},
    {"TwoByteMax", std::string_view{"\xdf\xbf", 2}, static_cast<char32_t>(0x7FF)},
    {"ThreeByteMin", std::string_view{"\xe0\xa0\x80", 3}, static_cast<char32_t>(0x800)},
    {"BeforeSurrogates", std::string_view{"\xed\x9f\xbf", 3}, static_cast<char32_t>(0xD7FF)},
    {"AfterSurrogates", std::string_view{"\xee\x80\x80", 3}, static_cast<char32_t>(0xE000)},
    {"Bom", std::string_view{"\xef\xbb\xbf", 3}, static_cast<char32_t>(0xFEFF)},
    {"Noncharacter", std::string_view{"\xef\xbf\xbf", 3}, static_cast<char32_t>(0xFFFF)},
    {"FourByteMin", std::string_view{"\xf0\x90\x80\x80", 4}, static_cast<char32_t>(0x10000)},
    {"Supplementary", std::string_view{"\xf0\x9f\x98\x80", 4}, static_cast<char32_t>(0x1F600)},
    {"ScalarMax", std::string_view{"\xf4\x8f\xbf\xbf", 4}, static_cast<char32_t>(0x10FFFF)},
  };

  /**
   * @brief Malformed sequences with no replacement-decoding interpretation.
   * @par Requirements
   * P005; J5-009.
   */
  struct invalid_case
  {
      const char* name;       // Case description.
      std::string_view bytes; // Input malformed UTF-8 bytes.
  };

  const invalid_case c_invalid_cases[] = {
    {"OverlongZero", std::string_view{"\xc0\x80", 2}},
    {"OverlongTwoByte", std::string_view{"\xc1\xbf", 2}},
    {"OverlongThreeByte", std::string_view{"\xe0\x9f\xbf", 3}},
    {"OverlongFourByte", std::string_view{"\xf0\x8f\xbf\xbf", 4}},
    {"HighSurrogate", std::string_view{"\xed\xa0\x80", 3}},
    {"LowSurrogate", std::string_view{"\xed\xbf\xbf", 3}},
    {"AboveScalarMax", std::string_view{"\xf4\x90\x80\x80", 4}},
    {"IllegalF5", std::string_view{"\xf5\x80\x80\x80", 4}},
    {"IllegalFF", std::string_view{"\xff", 1}},
    {"FiveByte", std::string_view{"\xf8\x88\x80\x80\x80", 5}},
    {"SixByte", std::string_view{"\xfc\x84\x80\x80\x80\x80", 6}},
    {"LoneContinuation", std::string_view{"\x80", 1}},
    {"LastContinuation", std::string_view{"\xbf", 1}},
    {"BadSecondLow", std::string_view{"\xc2\x7f", 2}},
    {"BadSecondHigh", std::string_view{"\xc2\xc0", 2}},
    {"BadThirdLow", std::string_view{"\xe0\xa0\x7f", 3}},
    {"BadThirdHigh", std::string_view{"\xe0\xa0\xc0", 3}},
    {"BadFourthLow", std::string_view{"\xf0\x90\x80\x7f", 4}},
    {"BadFourthHigh", std::string_view{"\xf0\x90\x80\xc0", 4}},
    {"ZeroContinuation", std::string_view{"\xe2\x00\x80", 3}},
  };

  // Parameter fixtures vary only the independent input and expected scalar.
  using J5_009_Utf8Valid = testing::TestWithParam<valid_case>;
  using J5_009_Utf8Truncated = testing::TestWithParam<valid_case>;
  using J5_009_Utf8Invalid = testing::TestWithParam<invalid_case>;

  /* Decode exact-length buffers at zero and nonzero original offsets.
   * Arrange: Prefix each valid encoding with a multibyte character.
   * Act: Decode the parameter at both offsets.
   * Expect: Exact scalar/span, one-character advancement and immutable bytes.
   */
  TEST_P(J5_009_Utf8Valid, DecodesScalarAndOriginalByteSpan)
  {
    /* Arrange */
    const auto& sample = GetParam();
    for (const std::string_view prefix : {std::string_view{}, std::string_view{"A\xE2\x82\xAC", 4}})
    {
      SCOPED_TRACE(sample.name);
      SCOPED_TRACE(prefix.size());
      std::string bytes{prefix};
      bytes.append(sample.bytes);
      const std::string before = bytes;
      cursor source{input_view{bytes.data(), bytes.size()}};
      source.advance(prefix.size());
      /* Act */
      const auto actual = decode_utf8(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.error_if(), nullptr);
      ASSERT_NE(actual.value_if(), nullptr);
      ASSERT_TRUE(actual.value_if()->has_value());
      const auto& character = actual.value_if()->value();
      EXPECT_EQ(character.value, sample.scalar);
      EXPECT_EQ(character.byte_offset, prefix.size());
      EXPECT_EQ(character.byte_length, sample.bytes.size());
      EXPECT_EQ(source.offset(), bytes.size());
      EXPECT_TRUE(source.is_at_end());
      EXPECT_EQ(bytes, before);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Reject every nonempty truncated prefix of a multibyte encoding.
   * Arrange: Expose only a prefix while full bytes remain in backing storage.
   * Act: Decode the bounded view, then repeat after a multibyte prefix.
   * Expect: Encoding error at sequence start; no advancement or replacement.
   */
  TEST_P(J5_009_Utf8Truncated, RejectsEveryTruncatedMultibytePrefix)
  {
    /* Arrange */
    const auto& sample = GetParam();
    for (std::size_t length = 1; length < sample.bytes.size(); ++length)
    {
      for (const std::string_view prefix : {std::string_view{}, std::string_view{"A\xE2\x82\xAC", 4}})
      {
        SCOPED_TRACE(sample.name);
        SCOPED_TRACE(length);
        SCOPED_TRACE(prefix.size());
        std::string bytes{prefix};
        bytes.append(sample.bytes);
        cursor source{input_view{bytes.data(), prefix.size() + length}};
        source.advance(prefix.size());
        /* Act */
        const auto actual = decode_utf8(source);
        /* Expect */
        EXPECT_FALSE(actual.has_value());
        EXPECT_EQ(actual.value_if(), nullptr);
        ASSERT_NE(actual.error_if(), nullptr);
        EXPECT_EQ(actual.error_if()->code, error_code::encoding);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, prefix.size());
        EXPECT_EQ(source.offset(), prefix.size());
        EXPECT_EQ(source.remaining(), length);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Reject malformed encodings without consuming any part of the sequence.
   * Arrange: Place malformed bytes at zero/nonzero offsets followed by ASCII.
   * Act: Decode twice at the same offset.
   * Expect: Stable encoding error/start offset, absent value and unchanged input.
   */
  TEST_P(J5_009_Utf8Invalid, ReportsEncodingErrorWithoutAdvancing)
  {
    /* Arrange */
    const auto& sample = GetParam();
    for (const std::string_view prefix : {std::string_view{}, std::string_view{"A\xE2\x82\xAC", 4}})
    {
      std::string bytes{prefix};
      bytes.append(sample.bytes);
      bytes.push_back('Z');
      const std::string before = bytes;
      cursor source{input_view{bytes.data(), bytes.size()}};
      source.advance(prefix.size());
      for (int attempt = 0; attempt < 2; ++attempt)
      {
        SCOPED_TRACE(sample.name);
        SCOPED_TRACE(prefix.size());
        /* Act */
        const auto actual = decode_utf8(source);
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

  /* ----------------------------------------------------------------------------------- */

  /* Distinguish empty input from encoding failure.
   * Arrange: Default and nonnull zero-length input views.
   * Act: Decode each cursor twice.
   * Expect: Successful empty optional, no error, unchanged zero offset.
   */
  TEST(J5_009_Utf8, EmptyInputReturnsEof)
  {
    /* Arrange */
    const char unread = static_cast<char>(0xFF);
    for (const input_view view : {input_view{}, input_view{&unread, 0}})
    {
      cursor source{view};
      for (int attempt = 0; attempt < 2; ++attempt)
      {
        /* Act */
        const auto actual = decode_utf8(source);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        ASSERT_NE(actual.value_if(), nullptr);
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(actual.error_if(), nullptr);
        EXPECT_EQ(source.offset(), 0U);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* All ASCII octets including controls and NUL are transport-valid.
   * Arrange: Exact one-byte input for each octet from 0 through 127.
   * Act: Decode each input independently.
   * Expect: Same scalar and one-byte span; grammar rejection is a later step.
   */
  TEST(J5_009_Utf8, DecodesEveryAsciiByte)
  {
    /* Arrange */
    for (unsigned int byte = 0; byte <= 0x7F; ++byte)
    {
      const char input = static_cast<char>(byte);
      cursor source{input_view{&input, 1}};
      SCOPED_TRACE(byte);
      /* Act */
      const auto actual = decode_utf8(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_NE(actual.value_if(), nullptr);
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(actual.value_if()->value().value, static_cast<char32_t>(byte));
      EXPECT_EQ(actual.value_if()->value().byte_offset, 0U);
      EXPECT_EQ(actual.value_if()->value().byte_length, 1U);
      EXPECT_EQ(source.offset(), 1U);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Validate the complete forbidden leading-octet ranges.
   * Arrange: Every continuation, overlong lead and lead above F4.
   * Act: Decode each lead followed by three continuation bytes.
   * Expect: Encoding failure at zero without consuming input.
   */
  TEST(J5_009_Utf8, RejectsEveryForbiddenLeadingByte)
  {
    /* Arrange */
    for (unsigned int byte = 0x80; byte <= 0xFF; ++byte)
    {
      if (byte >= 0xC2 && byte <= 0xF4)
      {
        continue;
      }
      const char bytes[] = {static_cast<char>(byte), static_cast<char>(0x80), static_cast<char>(0x80),
                            static_cast<char>(0x80)};
      cursor source{input_view{bytes, sizeof(bytes)}};
      SCOPED_TRACE(byte);
      /* Act */
      const auto actual = decode_utf8(source);
      /* Expect */
      EXPECT_FALSE(actual.has_value());
      ASSERT_NE(actual.error_if(), nullptr);
      EXPECT_EQ(actual.error_if()->code, error_code::encoding);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 0U);
      EXPECT_EQ(source.offset(), 0U);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Reject every noncontinuation octet at every continuation position.
   * Arrange: Valid two/three/four-byte
   * encodings with one replaced continuation.
   * Act: Decode each replacement outside the 80..BF continuation range.

   * * Expect: Located encoding failure without cursor advancement.
   */
  TEST(J5_009_Utf8, RejectsEveryInvalidContinuationByte)
  {
    /* Arrange */
    for (const std::string_view valid : {std::string_view{"\xC2\xA2", 2}, std::string_view{"\xE2\x82\xAC", 3},
                                         std::string_view{"\xF0\x9F\x98\x80", 4}})
    {
      for (std::size_t position = 1; position < valid.size(); ++position)
      {
        for (unsigned int byte = 0; byte <= 0xFF; ++byte)
        {
          if (byte >= 0x80 && byte <= 0xBF)
          {
            continue;
          }
          std::string bytes{valid};
          bytes[position] = static_cast<char>(byte);
          cursor source{input_view{bytes.data(), bytes.size()}};
          SCOPED_TRACE(valid.size());
          SCOPED_TRACE(position);
          SCOPED_TRACE(byte);
          /* Act */
          const auto actual = decode_utf8(source);
          /* Expect */
          EXPECT_FALSE(actual.has_value());
          ASSERT_NE(actual.error_if(), nullptr);
          EXPECT_EQ(actual.error_if()->code, error_code::encoding);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, 0U);
          EXPECT_EQ(source.offset(), 0U);
        }
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Sequential decoding preserves byte mapping across NUL and multibyte input.
   * Arrange: Immutable, nonterminated
   * mixed-width bytes with a hidden suffix. Act: Decode all five characters and then EOF twice. Expect: Exact
   * scalar/span for each character; suffix outside view is unread.
   */
  TEST(J5_009_Utf8, DecodesMixedWidthsAndRepeatedEof)
  {
    /* Arrange */
    const char bytes[] = {'A',
                          '\0',
                          static_cast<char>(0xC2),
                          static_cast<char>(0xA2),
                          static_cast<char>(0xE2),
                          static_cast<char>(0x82),
                          static_cast<char>(0xAC),
                          static_cast<char>(0xF0),
                          static_cast<char>(0x9F),
                          static_cast<char>(0x98),
                          static_cast<char>(0x80),
                          static_cast<char>(0xFF)};
    const std::array<char32_t, 5> scalars{U'A', 0, 0xA2, 0x20AC, 0x1F600};
    const std::array<std::size_t, 5> offsets{0, 1, 2, 4, 7};
    const std::array<std::size_t, 5> lengths{1, 1, 2, 3, 4};
    cursor source{input_view{bytes, 11}};
    for (std::size_t index = 0; index < scalars.size(); ++index)
    {
      /* Act */
      const auto actual = decode_utf8(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_NE(actual.value_if(), nullptr);
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(actual.value_if()->value().value, scalars[index]);
      EXPECT_EQ(actual.value_if()->value().byte_offset, offsets[index]);
      EXPECT_EQ(actual.value_if()->value().byte_length, lengths[index]);
      EXPECT_EQ(source.offset(), offsets[index] + lengths[index]);
    }
    for (int attempt = 0; attempt < 2; ++attempt)
    {
      /* Act */
      const auto actual = decode_utf8(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_NE(actual.value_if(), nullptr);
      EXPECT_FALSE(actual.value_if()->has_value());
      EXPECT_EQ(actual.error_if(), nullptr);
      EXPECT_EQ(source.offset(), 11U);
    }
  }

  INSTANTIATE_TEST_SUITE_P(ValidEncodings, J5_009_Utf8Valid, testing::ValuesIn(c_valid_cases),
                           [](const testing::TestParamInfo<valid_case>& info)
                           {
                             return info.param.name;
                           });
  INSTANTIATE_TEST_SUITE_P(TruncatedEncodings, J5_009_Utf8Truncated,
                           testing::ValuesIn(c_valid_cases + 2, c_valid_cases + 12),
                           [](const testing::TestParamInfo<valid_case>& info)
                           {
                             return info.param.name;
                           });
  INSTANTIATE_TEST_SUITE_P(InvalidEncodings, J5_009_Utf8Invalid, testing::ValuesIn(c_invalid_cases),
                           [](const testing::TestParamInfo<invalid_case>& info)
                           {
                             return info.param.name;
                           });

  /* ================================= End Test_J5_009 ================================= */
} // namespace
