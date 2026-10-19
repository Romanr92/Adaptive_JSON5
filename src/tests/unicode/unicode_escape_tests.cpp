// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"
#include "adaptive_json5/detail/lexer/string/unicode_escape.hpp"
#include "adaptive_json5/detail/memory/decoded_string_builder.hpp"
#include "adaptive_json5/detail/unicode/utf16_decode.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Supply ordinary host storage for public component composition units.
   * @par Requirements
   * P017; J5-018.
   * @note Only test callbacks use the host heap; allocation failure remains explicit.
   */
  void* allocate_test_storage(void*, std::size_t bytes, std::size_t alignment) noexcept
  {
    return alignment <= alignof(std::max_align_t) ? std::malloc(bytes) : nullptr;
  }

  /**
   * @brief Release storage supplied by the host test callback.
   * @par Requirements
   * P017; J5-018.
   */
  void release_test_storage(void*, void* address, std::size_t, std::size_t) noexcept
  {
    std::free(address);
  }

  constexpr storage_callbacks c_test_storage{nullptr, &allocate_test_storage, &release_test_storage};

  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_015 =================================== */

  static_assert(noexcept(consume_unicode_escape(std::declval<cursor&>())));

  /**
   * @brief Literal Unicode escape spellings and exact expected UTF-16 units.
   * @par Requirements
   * P017; J5-015; J5-018.
   */
  struct unicode_case
  {
      const char* name;     // Diagnostic parameter name.
      std::string spelling; // Input u followed by four hexadecimal digits.
      char16_t expected;    // Expected decoded unit, including surrogates.
  };
  class J5_015_UnicodeEscape : public ::testing::TestWithParam<unicode_case>
  {
  };

  /* Verify exact unit decoding in both mode contexts with suffix preservation.
   * Arrange: Put a Unicode escape after an opener and backslash in both modes.
   * Act: Use boundary handoff, then decode the Unicode suffix.
   * Expect: The literal numeric unit and five-byte suffix consumption only.
   */
  TEST_P(J5_015_UnicodeEscape, DecodesExactUnitInBothModes)
  {
    /* Arrange */
    const auto& param = GetParam();
    const std::string input = std::string("X\"\\") + param.spelling + "Z";
    const auto before = input;
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      source.advance(1);
      const auto opening = consume_string_opening(source, mode);
      ASSERT_TRUE(opening.has_value());
      ASSERT_TRUE(opening.value_if()->has_value());
      const auto handoff = consume_string_piece(source, **opening.value_if(), mode);
      ASSERT_TRUE(handoff.has_value());
      ASSERT_EQ(handoff.value_if()->kind, string_piece_kind::escape_start);
      /* Act */
      const auto actual = consume_unicode_escape(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(**actual.value_if(), param.expected);
      EXPECT_EQ(source.offset(), 8u);
      EXPECT_EQ(*source.peek(), 'Z');
      EXPECT_EQ(input, before);
    }
    cursor exact({param.spelling.data(), param.spelling.size()});
    const auto bounded = consume_unicode_escape(exact);
    ASSERT_TRUE(bounded.has_value());
    ASSERT_TRUE(bounded.value_if()->has_value());
    EXPECT_EQ(**bounded.value_if(), param.expected);
    EXPECT_TRUE(exact.is_at_end());
  }
  INSTANTIATE_TEST_SUITE_P(
    Units, J5_015_UnicodeEscape,
    ::testing::Values(unicode_case{"Nul", "u0000", 0x0000}, unicode_case{"LowNibble", "u000F", 0x000F},
                      unicode_case{"SecondNibble", "u00F0", 0x00F0}, unicode_case{"ThirdNibble", "u0F00", 0x0F00},
                      unicode_case{"HighNibble", "uF000", 0xF000}, unicode_case{"Ascii", "u0041", 0x0041},
                      unicode_case{"Quote", "u0022", 0x0022}, unicode_case{"Backslash", "u005c", 0x005C},
                      unicode_case{"Lower", "uabcd", 0xABCD}, unicode_case{"Upper", "uABCD", 0xABCD},
                      unicode_case{"Mixed", "uaBcD", 0xABCD}, unicode_case{"BeforeSurrogates", "uD7FF", 0xD7FF},
                      unicode_case{"HighMin", "uD800", 0xD800}, unicode_case{"HighMax", "uDBFF", 0xDBFF},
                      unicode_case{"LowMin", "uDC00", 0xDC00}, unicode_case{"LowMax", "uDFFF", 0xDFFF},
                      unicode_case{"AfterSurrogates", "uE000", 0xE000}, unicode_case{"Max", "uFFFF", 0xFFFF}),
    [](const ::testing::TestParamInfo<unicode_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify every proper suffix prefix fails without reading hidden digits.
   * Arrange: Restrict the view to each prefix of a complete u1234 escape.
   * Act: Decode from a nonzero position, including EOF immediately after backslash.
   * Expect: Syntax at the view end and unchanged original cursor.
   */
  TEST(J5_015_UnicodeBoundary, RejectsEveryTruncatedPrefix)
  {
    /* Arrange */
    const std::array<char, 8> input{'X', '\\', 'u', '1', '2', '3', '4', '"'};
    for (std::size_t length = 0; length < 5; ++length)
    {
      cursor source({input.data(), 2 + length});
      source.advance(2);
      /* Act */
      const auto actual = consume_unicode_escape(source);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 2u + length);
      EXPECT_EQ(source.offset(), 2u);
    }
    cursor empty;
    const auto actual = consume_unicode_escape(empty);
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, error_code::syntax);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 0u);
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief A nonhex source spelling and its independent failure category.
   * @par Requirements
   * P017; J5-015.
   */
  struct invalid_case
  {
      const char* name;  // Diagnostic parameter name.
      std::string bytes; // Input invalid character or malformed UTF-8 bytes.
      error_code code;   // Expected syntax or encoding error.
  };
  class J5_015_UnicodeFailure : public ::testing::TestWithParam<invalid_case>
  {
  };

  /* Verify invalid characters and malformed encoding at every digit position.
   * Arrange: Insert one invalid spelling into each of the four digit positions.
   * Act: Decode after a nonzero input prefix.
   * Expect: Exact category and starting offset; source remains at u.
   */
  TEST_P(J5_015_UnicodeFailure, RejectsEveryDigitPositionAtomically)
  {
    /* Arrange */
    const auto& param = GetParam();
    for (std::size_t digit_index = 0; digit_index < 4; ++digit_index)
    {
      const std::string input =
        std::string("X\\u") + std::string(digit_index, '1') + param.bytes + std::string(3 - digit_index, '2');
      const auto before = input;
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_unicode_escape(source);
      /* Expect */
      ASSERT_FALSE(actual.has_value()) << digit_index;
      EXPECT_EQ(actual.error_if()->code, param.code);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 3u + digit_index);
      EXPECT_EQ(source.offset(), 2u);
      EXPECT_EQ(input, before);
    }
  }
  INSTANTIATE_TEST_SUITE_P(Invalid, J5_015_UnicodeFailure,
                           ::testing::Values(invalid_case{"NonhexLetter", "G", error_code::syntax},
                                             invalid_case{"Quote", "\"", error_code::syntax},
                                             invalid_case{"Brace", "{", error_code::syntax},
                                             invalid_case{"Space", " ", error_code::syntax},
                                             invalid_case{"Newline", "\n", error_code::syntax},
                                             invalid_case{"Nul", std::string(1, '\0'), error_code::syntax},
                                             invalid_case{"FullwidthDigit", "\xEF\xBC\x90", error_code::syntax},
                                             invalid_case{"Supplementary", "\xF0\x9F\x8E\xBC", error_code::syntax},
                                             invalid_case{"Continuation", "\x80", error_code::encoding},
                                             invalid_case{"Overlong", "\xC0\xAF", error_code::encoding},
                                             invalid_case{"SurrogateUtf8", "\xED\xA0\x80", error_code::encoding},
                                             invalid_case{"OutOfRange", "\xF4\x90\x80\x80", error_code::encoding}),
                           [](const ::testing::TestParamInfo<invalid_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify malformed multi-byte source prefixes report encoding errors, not EOF syntax.
   * Arrange: End a view inside a UTF-8 character at each expected digit position.
   * Act: Decode while the backing buffer still contains a complete UTF-8 suffix.
   * Expect: Encoding at the character start with the original cursor unchanged.
   */
  TEST(J5_015_UnicodeBoundary, RejectsTruncatedUtf8AtEveryDigitPosition)
  {
    /* Arrange */
    for (std::size_t digit_index = 0; digit_index < 4; ++digit_index)
    {
      const std::string input = std::string("X\\u") + std::string(digit_index, '1') + "\xF0\x9F\x8E\xBC";
      for (std::size_t prefix = 1; prefix < 4; ++prefix)
      {
        cursor source({input.data(), 3 + digit_index + prefix});
        source.advance(2);
        /* Act */
        const auto actual = consume_unicode_escape(source);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::encoding);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 3u + digit_index);
        EXPECT_EQ(source.offset(), 2u);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify other introducers and suffix bytes remain unread.
   * Arrange: Select all non-u bytes, then complete u0041 with varied suffixes.
   * Act: Attempt Unicode recognition and decode a complete owned spelling.
   * Expect: No-match for other forms and exactly five consumed bytes on success.
   */
  TEST(J5_015_UnicodeBoundary, PreservesOtherIntroducersAndTrailingBytes)
  {
    /* Arrange */
    for (unsigned int byte = 0; byte <= 255; ++byte)
    {
      if (byte == 'u')
      {
        continue;
      }
      const char input = static_cast<char>(byte);
      cursor source({&input, 1});
      /* Act */
      const auto actual = consume_unicode_escape(source);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_FALSE(actual.value_if()->has_value());
      EXPECT_EQ(source.offset(), 0u);
    }
    for (const char suffix : {'F', '"', '\0', static_cast<char>(0x80)})
    {
      const std::array<char, 6> input{'u', '0', '0', '4', '1', suffix};
      cursor source({input.data(), input.size()});
      const auto actual = consume_unicode_escape(source);
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(**actual.value_if(), char16_t{0x41});
      EXPECT_EQ(source.offset(), 5u);
      EXPECT_EQ(*source.peek(), suffix);
    }
  }

  /* ================================= End Test_J5_015 ================================= */

  /* =================================== Test_J5_018 =================================== */

  /**
   * @brief Two literal escapes and expected stored units and conversion outcome.
   * @par Requirements
   * P017; J5-018.
   */
  struct pair_case
  {
      const char* name;                   // Diagnostic parameter name.
      std::string first;                  // Input first Unicode escape suffix.
      std::string second;                 // Input second Unicode escape suffix.
      char16_t first_unit;                // Expected first stored unit.
      char16_t second_unit;               // Expected second stored unit.
      bool valid_pair;                    // Whether checked scalar/UTF-8 conversion should succeed.
      char32_t scalar;                    // Expected scalar for valid pairs; otherwise unused.
      std::array<unsigned char, 4> bytes; // Expected four UTF-8 bytes for valid pairs.
  };
  class J5_018_UnicodePair : public ::testing::TestWithParam<pair_case>
  {
  };

  /* Verify escaped pairs and invalid orders retain their exact original units.
   * Arrange: Independently decode two literal suffixes into a bounded builder.
   * Act: Store, finish, inspect units and perform checked scalar/UTF-8 conversion.
   * Expect: Exact units; valid pairs convert, invalid sequences preserve storage
   * while reporting encoding without changing the destination.
   */
  TEST_P(J5_018_UnicodePair, PreservesUnitsAndChecksConversion)
  {
    /* Arrange */
    const auto& param = GetParam();
    cursor first_source({param.first.data(), param.first.size()});
    cursor second_source({param.second.data(), param.second.size()});
    decoded_string_builder builder(c_test_storage, 2);
    std::array<unsigned char, 4> output;
    output.fill(0xA5);
    const auto before = output;
    /* Act */
    const auto first = consume_unicode_escape(first_source);
    const auto second = consume_unicode_escape(second_source);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(first.value_if()->has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(second.value_if()->has_value());
    ASSERT_TRUE(builder.append_unit(**first.value_if()).has_value());
    ASSERT_TRUE(builder.append_unit(**second.value_if()).has_value());
    auto text = builder.finish();
    const auto decoded = decode_utf16(text, 0);
    const auto exported = export_utf8(text, output.data(), output.size());
    /* Expect */
    ASSERT_EQ(text.size(), 2u);
    const auto first_stored = text.unit_at(0);
    const auto second_stored = text.unit_at(1);
    ASSERT_TRUE(first_stored.has_value());
    ASSERT_TRUE(second_stored.has_value());
    EXPECT_EQ(*first_stored.value_if(), param.first_unit);
    EXPECT_EQ(*second_stored.value_if(), param.second_unit);
    ASSERT_EQ(decoded.has_value(), param.valid_pair);
    ASSERT_EQ(exported.has_value(), param.valid_pair);
    if (param.valid_pair)
    {
      EXPECT_EQ(decoded.value_if()->value, param.scalar);
      EXPECT_EQ(decoded.value_if()->unit_count, 2u);
      EXPECT_EQ(*exported.value_if(), 4u);
      EXPECT_EQ(output, param.bytes);
    }
    else
    {
      EXPECT_EQ(decoded.error_if()->code, error_code::encoding);
      EXPECT_FALSE(decoded.error_if()->has_byte_offset);
      EXPECT_EQ(exported.error_if()->code, error_code::encoding);
      EXPECT_FALSE(exported.error_if()->has_byte_offset);
      EXPECT_EQ(output, before);
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    Sequences, J5_018_UnicodePair,
    ::testing::Values(pair_case{"Min", "uD800", "uDC00", 0xD800, 0xDC00, true, 0x10000, {0xF0, 0x90, 0x80, 0x80}},
                      pair_case{"Music", "ud83c", "uDfBc", 0xD83C, 0xDFBC, true, 0x1F3BC, {0xF0, 0x9F, 0x8E, 0xBC}},
                      pair_case{"Max", "uDBFF", "uDFFF", 0xDBFF, 0xDFFF, true, 0x10FFFF, {0xF4, 0x8F, 0xBF, 0xBF}},
                      pair_case{"Reversed", "uDC00", "uD800", 0xDC00, 0xD800, false, 0, {}},
                      pair_case{"TwoHigh", "uD800", "uDBFF", 0xD800, 0xDBFF, false, 0, {}},
                      pair_case{"TwoLow", "uDC00", "uDFFF", 0xDC00, 0xDFFF, false, 0, {}},
                      pair_case{"HighThenAscii", "uD800", "u0041", 0xD800, 0x0041, false, 0, {}}),
    [](const ::testing::TestParamInfo<pair_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify escaped lone surrogates survive storage but cannot be UTF-8 exported.
   * Arrange: Decode each high/low surrogate range boundary from a literal suffix.
   * Act: Copy the unit into owned storage and attempt export.
   * Expect: Unit fidelity, allocation-free encoding error and unchanged output.
   */
  TEST(J5_018_UnicodeEscape, PreservesLoneSurrogatesWithoutReplacement)
  {
    /* Arrange */
    const std::array<unicode_case, 4> cases{{{"HighMin", "uD800", 0xD800},
                                             {"HighMax", "uDBFF", 0xDBFF},
                                             {"LowMin", "uDC00", 0xDC00},
                                             {"LowMax", "uDFFF", 0xDFFF}}};
    for (const auto& item : cases)
    {
      cursor source({item.spelling.data(), item.spelling.size()});
      std::array<unsigned char, 4> output;
      output.fill(0xA5);
      const auto before = output;
      /* Act */
      const auto decoded = consume_unicode_escape(source);
      ASSERT_TRUE(decoded.has_value());
      ASSERT_TRUE(decoded.value_if()->has_value());
      auto text = decoded_string::copy_units(c_test_storage, &**decoded.value_if(), 1);
      ASSERT_TRUE(text.has_value());
      const auto stored = text.value_if()->unit_at(0);
      const auto exported = export_utf8(*text.value_if(), output.data(), output.size());
      /* Expect */
      ASSERT_TRUE(stored.has_value());
      EXPECT_EQ(*stored.value_if(), item.expected);
      ASSERT_FALSE(exported.has_value());
      EXPECT_EQ(exported.error_if()->code, error_code::encoding);
      EXPECT_FALSE(exported.error_if()->has_byte_offset);
      EXPECT_EQ(output, before);
    }
  }

  /* ================================= End Test_J5_018 ================================= */
} // namespace
