// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/identifier_classification.hpp"
#include "adaptive_json5/identifier_lexer.hpp"
#include "adaptive_json5/string_lexer.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  /* ===================================== Fixtures ==================================== */
  /**
   * @brief Host storage with per-request failure injection and release validation.
   * @par Requirements
   * P022; P023; J5-040; J5-046; J5-051.
   * @note Fixed records track allocations; only these test callbacks use malloc.
   */
  class lexer_provider
  {
    private:
      struct allocation_record
      {
          void* address{};
          std::size_t bytes{};
          std::size_t alignment{};
      };
      std::array<allocation_record, 16> m_records{};

      /**
       * @brief Allocate or inject failure on the selected request.
       * @par Requirements
       * P022; P023; J5-046.
       */
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<lexer_provider*>(context);
        ++self.m_attempts;
        if (self.m_attempts == self.m_fail_on || alignment > alignof(std::max_align_t))
        {
          return nullptr;
        }
        for (auto& record : self.m_records)
        {
          if (record.address == nullptr)
          {
            record.address = std::malloc(bytes);
            if (record.address != nullptr)
            {
              record.bytes = bytes;
              record.alignment = alignment;
              ++self.m_allocations;
              self.m_live_bytes += bytes;
            }
            return record.address;
          }
        }
        return nullptr;
      }

      /**
       * @brief Verify and release one original allocation exactly once.
       * @par Requirements
       * P022; P023; J5-040; J5-051.
       */
      static void release(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<lexer_provider*>(context);
        for (auto& record : self.m_records)
        {
          if (record.address == address && address != nullptr)
          {
            EXPECT_EQ(record.bytes, bytes);
            EXPECT_EQ(record.alignment, alignment);
            self.m_live_bytes -= record.bytes;
            ++self.m_releases;
            std::free(address);
            record = {};
            return;
          }
        }
        ADD_FAILURE() << "Unknown allocation or duplicate release";
      }

    public:
      std::size_t m_attempts{};
      std::size_t m_fail_on{};
      std::size_t m_allocations{};
      std::size_t m_releases{};
      std::size_t m_live_bytes{};

      /**
       * @brief Confirm owners released all allocations before provider destruction.
       * @par Requirements
       * P022; P023; J5-040.
       */
      ~lexer_provider()
      {
        EXPECT_EQ(m_live_bytes, 0u);
        EXPECT_EQ(m_allocations, m_releases);
        for (const auto& record : m_records)
        {
          // Reclaim leaked test storage after reporting a production ownership failure.
          std::free(record.address);
        }
      }

      /**
       * @brief Borrow the test provider through the public callback interface.
       * @par Requirements
       * P022; P023; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &release};
      }
  };

  /**
   * @brief Compare exact decoded units using only the public checked accessor.
   * @par Requirements
   * P022; J5-019.
   */
  void expect_units(const decoded_string& actual, const std::u16string& expected)
  {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      const auto unit = actual.unit_at(index);
      ASSERT_TRUE(unit.has_value());
      EXPECT_EQ(*unit.value_if(), expected[index]) << index;
    }
  }

  /* =================================== End Fixtures ================================== */
  /* =================================== Test_J5_019 =================================== */

  static_assert(noexcept(consume_identifier(std::declval<cursor&>(), {}, 0)));
  static_assert(std::is_nothrow_move_constructible_v<identifier_token>);
  static_assert(!std::is_copy_constructible_v<identifier_token>);
  /**
   * @brief Literal identifier fixtures independent of production classification.
   * @par Requirements
   * P022; J5-019.
   */
  struct name_case
  {
      const char* name;        // Input case label.
      std::string input;       // Input UTF-8 bytes, without a required terminator.
      std::u16string expected; // Expected decoded UTF-16 units.
  };
  class J5_019_Identifier : public testing::TestWithParam<name_case>
  {
  };
  /* Recognize category representatives and reserved words.
   * Arrange: Supply exact-length UTF-8 input and caller storage.
   * Act: Consume one identifier.
   * Expect: Exact units, byte span, EOF, unchanged input and no escape metadata.
   */
  TEST_P(J5_019_Identifier, RecognizesOrdinaryNames)
  {
    /* Arrange */
    const auto& item = GetParam();
    lexer_provider provider;
    const auto before = item.input;
    cursor source({item.input.data(), item.input.size()});
    /* Act */
    auto actual = consume_identifier(source, provider.callbacks(), 32);
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    const auto& token = **actual.value_if();
    expect_units(token.value, item.expected);
    EXPECT_EQ(token.byte_offset, 0u);
    EXPECT_EQ(token.byte_length, item.input.size());
    EXPECT_EQ(source.offset(), item.input.size());
    EXPECT_FALSE(token.contains_escape);
    EXPECT_EQ(item.input, before);
  }
  INSTANTIATE_TEST_SUITE_P(
    P022, J5_019_Identifier,
    testing::Values(name_case{"Dollar", "$", u"$"}, name_case{"Underscore", "_", u"_"},
                    name_case{"UpperLower", "Az", u"Az"}, name_case{"Titlecase", u8"\u01C5", u"\u01C5"},
                    name_case{"Modifier", u8"\u02B0", u"\u02B0"}, name_case{"OtherLetter", u8"\u4E00", u"\u4E00"},
                    name_case{"LetterNumber", u8"\u2160", u"\u2160"},
                    name_case{"Continuations", u8"a0\u0301\u0903\u203F\u200C\u200D$_",
                              u"a0\u0301\u0903\u203F\u200C\u200D$_"},
                    name_case{"Supplementary", u8"\U00010400x", u"\U00010400x"}, name_case{"True", "true", u"true"},
                    name_case{"False", "false", u"false"}, name_case{"Null", "null", u"null"},
                    name_case{"Reserved", "class", u"class"}, name_case{"Suffix", "true0", u"true0"}),
    [](const testing::TestParamInfo<name_case>& item)
    {
      return item.param.name;
    });
  /* ----------------------------------------------------------------------------------- */

  /* Reject invalid first characters without allocating.
   * Arrange: Supply digits, continuation-only characters, punctuation and empty views.
   * Act: Try identifier recognition.
   * Expect: Successful no-match with unchanged cursor and no storage requests.
   */
  TEST(J5_019_IdentifierBoundary, RejectsInvalidStartsWithoutAllocation)
  {
    /* Arrange */
    const std::string cases[] = {
      "",         "0abc",     "9",        ":",        "-",        " ",           "\"a\"", std::string(1, '\0'),
      u8"\u0301", u8"\u0903", u8"\u203F", u8"\u200C", u8"\u200D", u8"\U0001F600"};
    for (const auto& input : cases)
    {
      SCOPED_TRACE(testing::PrintToString(input));
      lexer_provider provider;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), 0);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_FALSE(actual.value_if()->has_value());
      EXPECT_EQ(source.offset(), 0u);
      EXPECT_EQ(provider.m_attempts, 0u);
    }
    cursor empty({nullptr, 0});
    auto actual = consume_identifier(empty, {}, 0);
    ASSERT_TRUE(actual.has_value());
    EXPECT_FALSE(actual.value_if()->has_value());
  }
  /* ----------------------------------------------------------------------------------- */

  /* Preserve token boundaries at nonzero offsets and explicit NUL bytes.
   * Arrange: Put an identifier between a prefix and each terminating character.
   * Act: Recognize from offset two.
   * Expect: Exact source span and the complete terminator left unconsumed.
   */
  TEST(J5_019_IdentifierBoundary, PreservesTerminatorAndOriginalSpan)
  {
    /* Arrange */
    const std::string endings[] = {
      ":", ",", "}", "]", " ", "\n", "/comment", "-", ".", std::string(1, '\0'), u8"\u00A0", u8"\U0001F600"};
    for (const auto& ending : endings)
    {
      lexer_provider provider;
      const std::string input = std::string("!!") + u8"\u03B1b2" + ending;
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), 3);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      const auto& token = **actual.value_if();
      expect_units(token.value, u"\u03B1b2");
      EXPECT_EQ(token.byte_offset, 2u);
      EXPECT_EQ(token.byte_length, 4u);
      EXPECT_EQ(source.offset(), 6u);
      ASSERT_TRUE(source.peek().has_value());
      EXPECT_EQ(*source.peek(), ending[0]);
    }
  }
  /* ----------------------------------------------------------------------------------- */

  /* Validate every truncated multibyte prefix without committing a partial name.
   * Arrange: Malformed UTF-8 before or after a valid identifier prefix.
   * Act: Consume the identifier.
   * Expect: Located encoding failure, unchanged cursor and released temporary storage.
   */
  TEST(J5_019_IdentifierEncoding, RejectsMalformedAndTruncatedUtf8Atomically)
  {
    /* Arrange */
    const std::string malformed[] = {"\x80", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2x",       "\xC2",
                                     "\xE2", "\xE2\x82", "\xF0",         "\xF0\x90",         "\xF0\x90\x90"};
    for (const auto& bytes : malformed)
    {
      for (const std::string prefix : {std::string{}, std::string{"ab"}})
      {
        lexer_provider provider;
        const auto input = std::string("!") + prefix + bytes;
        cursor source({input.data(), input.size()});
        source.advance(1);
        /* Act */
        auto actual = consume_identifier(source, provider.callbacks(), 32);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::encoding);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 1 + prefix.size());
        EXPECT_EQ(source.offset(), 1u);
        EXPECT_EQ(provider.m_live_bytes, 0u);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */

  /* Check decoded unit limits including supplementary scalar pairs.
   * Arrange: A supplementary letter followed by an ASCII letter, with varying limits.
   * Act: Recognize the exact same input at each limit.
   * Expect: Limits zero/one/two fail atomically; the exact three-unit limit succeeds.
   */
  TEST(J5_019_IdentifierStorage, EnforcesUtf16UnitLimits)
  {
    /* Arrange */
    const std::string input = u8"\U00010400x";
    for (std::size_t limit = 0; limit <= 3; ++limit)
    {
      lexer_provider provider;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), limit);
      /* Expect */
      if (limit == 3)
      {
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        expect_units((**actual.value_if()).value, u"\U00010400x");
        EXPECT_EQ(source.offset(), 5u);
      }
      else
      {
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, limit < 2 ? 0u : 4u);
        EXPECT_EQ(source.offset(), 0u);
        EXPECT_EQ(provider.m_live_bytes, 0u);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */

  /* Fail each observed allocation and retry using the unchanged cursor.
   * Arrange: Measure a successful long name's allocation requests, then inject each failure.
   * Act: Recognize, fail and retry.
   * Expect: Explicit allocation error, balanced release and exact successful retry content.
   */
  TEST(J5_019_IdentifierStorage, FailsEveryAllocationAndAllowsRetry)
  {
    /* Arrange */
    const std::string input = "abcdefghijklmnopq";
    std::size_t attempts{};
    {
      lexer_provider provider;
      cursor source({input.data(), input.size()});
      auto baseline = consume_identifier(source, provider.callbacks(), input.size());
      ASSERT_TRUE(baseline.has_value());
      ASSERT_TRUE(baseline.value_if()->has_value());
      attempts = provider.m_attempts;
    }
    ASSERT_GT(attempts, 1u);
    for (std::size_t failure = 1; failure <= attempts; ++failure)
    {
      lexer_provider provider;
      provider.m_fail_on = failure;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), input.size());
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::allocation_failure);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_LT(actual.error_if()->byte_offset, input.size());
      EXPECT_EQ(source.offset(), 0u);
      EXPECT_EQ(provider.m_live_bytes, 0u);
      EXPECT_EQ(provider.m_allocations, provider.m_releases);
      provider.m_fail_on = 0;
      auto retry = consume_identifier(source, provider.callbacks(), input.size());
      ASSERT_TRUE(retry.has_value());
      ASSERT_TRUE(retry.value_if()->has_value());
      expect_units((**retry.value_if()).value, u"abcdefghijklmnopq");
      EXPECT_EQ(source.offset(), input.size());
    }
  }
  /* ----------------------------------------------------------------------------------- */

  /* Check missing storage and independent owned token lifetime.
   * Arrange: Recognize from a mutable exact-length buffer with and without storage.
   * Act: Move a successful token and overwrite/destroy its input.
   * Expect: Explicit missing-provider error and preserved moved decoded content.
   */
  TEST(J5_019_IdentifierStorage, OwnsContentAfterInputDestructionAndMove)
  {
    /* Arrange */
    lexer_provider provider;
    std::optional<identifier_token> retained;
    {
      std::string input = "owned";
      cursor source({input.data(), input.size()});
      /* Act */
      auto missing = consume_identifier(source, {}, 5);
      /* Expect */
      ASSERT_FALSE(missing.has_value());
      EXPECT_EQ(missing.error_if()->code, error_code::allocation_failure);
      EXPECT_EQ(source.offset(), 0u);
      auto actual = consume_identifier(source, provider.callbacks(), 5);
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      retained.emplace(std::move(**actual.value_if()));
      input.assign(input.size(), '?');
    }
    expect_units(retained->value, u"owned");
    EXPECT_EQ(retained->byte_offset, 0u);
    EXPECT_EQ(retained->byte_length, 5u);
    retained.reset();
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  /* ================================= End Test_J5_019 ================================= */

  /* ==================================== Test_J5_020 ==================================== */
  class J5_020_IdentifierEscapes : public testing::TestWithParam<name_case>
  {
  };
  /* Recognize escapes using independent expected content.
   * Arrange: Supply mixed raw/escaped names at a nonzero offset.
   * Act: Consume with an exact decoded unit limit.
   * Expect: Exact units, original byte span, escape flag and unconsumed colon.
   */
  TEST_P(J5_020_IdentifierEscapes, DecodesIdentifierEscapes)
  {
    /* Arrange */
    const auto& item = GetParam();
    lexer_provider provider;
    const std::string input = "!!" + item.input + ":";
    cursor source({input.data(), input.size()});
    source.advance(2);
    /* Act */
    auto actual = consume_identifier(source, provider.callbacks(), item.expected.size());
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    const auto& token = **actual.value_if();
    expect_units(token.value, item.expected);
    EXPECT_EQ(token.byte_offset, 2u);
    EXPECT_EQ(token.byte_length, item.input.size());
    EXPECT_EQ(source.offset(), 2 + item.input.size());
    EXPECT_EQ(source.peek(), std::optional<char>{':'});
    EXPECT_TRUE(token.contains_escape);
  }

  INSTANTIATE_TEST_SUITE_P(
    P023, J5_020_IdentifierEscapes,
    testing::Values(name_case{"Start", R"(\u0061b)", u"ab"}, name_case{"PartDigit", R"(a\u0030)", u"a0"},
                    name_case{"DollarUnderscore", R"(\u0024\u005F)", u"$_"},
                    name_case{"MarksAndJoiners", R"(a\u0301\u0903\u203F\u200C\u200D)",
                              u"a\u0301\u0903\u203F\u200C\u200D"},
                    name_case{"HexCase", R"(\u00e9\u00E9)", u"\u00E9\u00E9"},
                    name_case{"RawAndEscaped", u8"\u03B1\\u0062\U00010400", u"\u03B1b\U00010400"},
                    name_case{"Reserved", R"(\u0063lass)", u"class"},
                    name_case{"LiteralLooking", R"(tr\u0075e)", u"true"}),
    [](const testing::TestParamInfo<name_case>& item)
    {
      return item.param.name;
    });
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief Explicit invalid escaped-name expectations.
   * @par Requirements
   * P023; J5-020.
   */
  struct escape_error_case
  {
      const char* name;   // Input case label.
      std::string input;  // Input source bytes.
      error_code code;    // Expected explicit error category.
      std::size_t offset; // Expected original byte offset relative to the name.
  };
  class J5_020_IdentifierFailure : public testing::TestWithParam<escape_error_case>
  {
  };
  /* Reject malformed and positionally invalid escapes atomically.
   * Arrange: Put each invalid name after two prefix bytes.
   * Act: Attempt identifier recognition.
   * Expect: Exact error category/offset, original cursor and no live allocations.
   */
  TEST_P(J5_020_IdentifierFailure, ReportsLocatedErrorWithoutPartialToken)
  {
    /* Arrange */
    const auto& item = GetParam();
    lexer_provider provider;
    const std::string input = "!!" + item.input;
    cursor source({input.data(), input.size()});
    source.advance(2);
    /* Act */
    auto actual = consume_identifier(source, provider.callbacks(), 32);
    /* Expect */
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, item.code);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 2 + item.offset);
    EXPECT_EQ(source.offset(), 2u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }
  INSTANTIATE_TEST_SUITE_P(P023, J5_020_IdentifierFailure,
                           testing::Values(escape_error_case{"DigitStart", R"(\u0030a)", error_code::syntax, 0},
                                           escape_error_case{"MarkStart", R"(\u0301)", error_code::syntax, 0},
                                           escape_error_case{"JoinerStart", R"(\u200D)", error_code::syntax, 0},
                                           escape_error_case{"NonJoinerStart", R"(\u200C)", error_code::syntax, 0},
                                           escape_error_case{"HyphenPart", R"(a\u002D)", error_code::syntax, 1},
                                           escape_error_case{"NulPart", R"(a\u0000)", error_code::syntax, 1},
                                           escape_error_case{"HighSurrogate", R"(\uD800)", error_code::syntax, 0},
                                           escape_error_case{"LowSurrogate", R"(a\uDC00)", error_code::syntax, 1},
                                           escape_error_case{"SurrogatePair", R"(\uD801\uDC00)", error_code::syntax, 0},
                                           escape_error_case{"NonHex", R"(a\u00G0)", error_code::syntax, 5},
                                           escape_error_case{"HexEscape", R"(a\x61)", error_code::syntax, 1},
                                           escape_error_case{"UpperU", R"(\U0061)", error_code::syntax, 0},
                                           escape_error_case{"BraceEscape", R"(a\u{61})", error_code::syntax, 3},
                                           escape_error_case{"EscapedBackslash", R"(a\\)", error_code::syntax, 1},
                                           escape_error_case{"EncodingInDigits", std::string("a\\u00") + "\x80",
                                                             error_code::encoding, 5},
                                           escape_error_case{"EncodingAfterEscape", std::string(R"(\u0061)") + "\x80",
                                                             error_code::encoding, 6}),
                           [](const testing::TestParamInfo<escape_error_case>& item)
                           {
                             return item.param.name;
                           });
  /* ----------------------------------------------------------------------------------- */
  /* Reject every incomplete escape prefix using the explicit input length.
   * Arrange: Keep a complete backing buffer but expose each incomplete escape prefix.
   * Act: Consume from a nonzero offset with and without an accepted name prefix.
   * Expect: Syntax error at exposed EOF and complete rollback without overreading.
   */
  TEST(J5_020_IdentifierBoundary, RejectsEveryTruncatedEscapePrefix)
  {
    /* Arrange */
    for (const std::string prefix : {std::string{}, std::string{"ab"}})
    {
      const std::string input = "!!" + prefix + R"(\u0061)";
      for (std::size_t length = 1; length < 6; ++length)
      {
        lexer_provider provider;
        const auto size = 2 + prefix.size() + length;
        cursor source({input.data(), size});
        source.advance(2);
        /* Act */
        auto actual = consume_identifier(source, provider.callbacks(), 32);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::syntax);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, size);
        EXPECT_EQ(source.offset(), 2u);
        EXPECT_EQ(provider.m_live_bytes, 0u);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Count decoded units rather than escape spelling bytes.
   * Arrange: Three escaped characters with limits below and at the decoded length.
   * Act: Recognize the name.
   * Expect: Exact limit success or resource failure at the first excluded escape.
   */
  TEST(J5_020_IdentifierStorage, EnforcesDecodedLimitsAtEscapeOffsets)
  {
    /* Arrange */
    const std::string input = R"(\u0061\u0062\u0063)";
    for (std::size_t limit = 0; limit <= 3; ++limit)
    {
      lexer_provider provider;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), limit);
      /* Expect */
      if (limit == 3)
      {
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        expect_units((**actual.value_if()).value, u"abc");
        EXPECT_EQ(source.offset(), 18u);
      }
      else
      {
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, 6 * limit);
        EXPECT_EQ(source.offset(), 0u);
        EXPECT_EQ(provider.m_live_bytes, 0u);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Inject every allocation failure while constructing escaped content.
   * Arrange: Measure a fully escaped name's actual allocation count.
   * Act: Fail each request, then retry from the original cursor.
   * Expect: Allocation failure at an escape start, no leaked storage and exact retry.
   */
  TEST(J5_020_IdentifierStorage, FailsEveryAllocationAndRetriesEscapedName)
  {
    /* Arrange */
    const std::string input = R"(\u0061\u0062\u0063\u0064\u0065\u0066\u0067\u0068\u0069)";
    std::size_t attempts{};
    {
      lexer_provider provider;
      cursor source({input.data(), input.size()});
      auto actual = consume_identifier(source, provider.callbacks(), 9);
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      attempts = provider.m_attempts;
    }
    ASSERT_GT(attempts, 1u);
    for (std::size_t failure = 1; failure <= attempts; ++failure)
    {
      lexer_provider provider;
      provider.m_fail_on = failure;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), 9);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::allocation_failure);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset % 6, 0u);
      EXPECT_LT(actual.error_if()->byte_offset, input.size());
      EXPECT_EQ(source.offset(), 0u);
      EXPECT_EQ(provider.m_live_bytes, 0u);
      EXPECT_EQ(provider.m_allocations, provider.m_releases);
      provider.m_fail_on = 0;
      auto retry = consume_identifier(source, provider.callbacks(), 9);
      ASSERT_TRUE(retry.has_value());
      ASSERT_TRUE(retry.value_if()->has_value());
      expect_units((**retry.value_if()).value, u"abcdefghi");
      EXPECT_TRUE((**retry.value_if()).contains_escape);
      EXPECT_EQ(source.offset(), input.size());
    }
  }

  /* ----------------------------------------------------------------------------------- */
  /* Retain decoded escaped content independently of input and token movement.
   * Arrange: Mutable escaped source and caller-owned storage.
   * Act: Recognize, move the token, overwrite and destroy the input.
   * Expect: Exact decoded units, retained escape metadata and balanced cleanup.
   */
  TEST(J5_020_IdentifierStorage, OwnsEscapedContentAfterMoveAndInputDestruction)
  {
    /* Arrange */
    lexer_provider provider;
    std::optional<identifier_token> retained;
    {
      std::string input = R"(a\u0062)";
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), 2);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      retained.emplace(std::move(**actual.value_if()));
      input.assign(input.size(), '?');
    }
    expect_units(retained->value, u"ab");
    EXPECT_TRUE(retained->contains_escape);
    EXPECT_EQ(retained->byte_length, 7u);
    retained.reset();
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }

  /* ================================== End Test_J5_020 ================================== */
  /* ==================================== Test_J5_033 ==================================== */
  /* Preserve exact name content across quoting and escaping without normalization.
   * Arrange: Bare, escaped and quoted spellings with literal expected UTF-16 sequences.
   * Act: Use the real identifier and quoted-string readers.
   * Expect: Equivalent spellings share units; case and normalization variants remain distinct.
   */
  TEST(J5_033_IdentifierContent, PreservesExactDecodedNamesAcrossSpellings)
  {
    /* Arrange */
    const std::string identifiers[] = {"ab", R"(a\u0062)", R"(\u0061b)", "Ab", u8"\u00E9", R"(e\u0301)"};
    const std::u16string expected[] = {u"ab", u"ab", u"ab", u"Ab", u"\u00E9", u"e\u0301"};
    lexer_provider provider;
    for (std::size_t index = 0; index < 6; ++index)
    {
      cursor source({identifiers[index].data(), identifiers[index].size()});
      /* Act */
      auto actual = consume_identifier(source, provider.callbacks(), 8);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      expect_units((**actual.value_if()).value, expected[index]);
    }
    for (const std::string input : {std::string{R"("ab")"}, std::string{R"("a\u0062")"}, std::string{R"('ab')"}})
    {
      cursor source({input.data(), input.size()});
      auto actual = consume_string(source, syntax_mode::json5, provider.callbacks(), 2);
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      expect_units((**actual.value_if()).value, u"ab");
    }
  }
  /* ================================== End Test_J5_033 ================================== */

  /* ==================================== Test_J5_023 ==================================== */
  static_assert(noexcept(classify_identifier(std::declval<const identifier_token&>(), identifier_context::value,
                                             syntax_mode::json5)));
  /**
   * @brief Literal source and independent classification expectations.
   * @par Requirements
   * P024; J5-023.
   */
  struct classification_case
  {
      const char* name;                   // Input case label.
      std::string input;                  // Input identifier source bytes.
      std::u16string decoded;             // Expected unchanged decoded UTF-16 units.
      identifier_classification expected; // Expected value-context classification.
      bool escaped;                       // Expected token escape metadata.
  };
  class J5_023_IdentifierClassification : public testing::TestWithParam<std::tuple<classification_case, syntax_mode>>
  {
  };

  /* Classify actual identifier tokens in both grammatical contexts.
   * Arrange: Recognize a name after a multibyte prefix, then arm storage failure.
   * Act: Repeatedly classify the same token as value and member name.
   * Expect: Exact classification or strict-key error, no allocation or token/cursor mutation.
   */
  TEST_P(J5_023_IdentifierClassification, ClassifiesValueAndMemberContexts)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    lexer_provider provider;
    const std::string input = std::string(u8"\u03B1:") + item.input + ",";
    const auto original = input;
    cursor source({input.data(), input.size()});
    source.advance(3);
    auto recognized = consume_identifier(source, provider.callbacks(), 32);
    ASSERT_TRUE(recognized.has_value());
    ASSERT_TRUE(recognized.value_if()->has_value());
    const auto& token = **recognized.value_if();
    const auto attempts = provider.m_attempts;
    const auto live_bytes = provider.m_live_bytes;
    const auto releases = provider.m_releases;
    provider.m_fail_on = attempts + 1;

    /* Act */
    const auto value = classify_identifier(token, identifier_context::value, mode);
    const auto member = classify_identifier(token, identifier_context::member_name, mode);
    const auto repeated = classify_identifier(token, identifier_context::value, mode);

    /* Expect */
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value.value_if(), item.expected);
    ASSERT_TRUE(repeated.has_value());
    EXPECT_EQ(*repeated.value_if(), item.expected);
    if (mode == syntax_mode::strict_json)
    {
      ASSERT_FALSE(member.has_value());
      EXPECT_EQ(member.error_if()->code, error_code::syntax);
      EXPECT_TRUE(member.error_if()->has_byte_offset);
      EXPECT_EQ(member.error_if()->byte_offset, 3u);
    }
    else
    {
      ASSERT_TRUE(member.has_value());
      EXPECT_EQ(*member.value_if(), identifier_classification::member_name);
    }
    expect_units(token.value, item.decoded);
    EXPECT_EQ(token.byte_offset, 3u);
    EXPECT_EQ(token.byte_length, item.input.size());
    EXPECT_EQ(token.contains_escape, item.escaped);
    EXPECT_EQ(source.offset(), 3 + item.input.size());
    EXPECT_EQ(source.peek(), std::optional<char>{','});
    EXPECT_EQ(input, original);
    EXPECT_EQ(provider.m_attempts, attempts);
    EXPECT_EQ(provider.m_live_bytes, live_bytes);
    EXPECT_EQ(provider.m_releases, releases);
  }
  INSTANTIATE_TEST_SUITE_P(
    P024, J5_023_IdentifierClassification,
    testing::Combine(
      testing::Values(
        classification_case{"Null", "null", u"null", identifier_classification::null_literal, false},
        classification_case{"True", "true", u"true", identifier_classification::true_literal, false},
        classification_case{"False", "false", u"false", identifier_classification::false_literal, false},
        classification_case{"NullCase", "Null", u"Null", identifier_classification::unrecognized_value, false},
        classification_case{"TrueCase", "TRUE", u"TRUE", identifier_classification::unrecognized_value, false},
        classification_case{"FalseCase", "False", u"False", identifier_classification::unrecognized_value, false},
        classification_case{"NullSuffix", "nullx", u"nullx", identifier_classification::unrecognized_value, false},
        classification_case{"TrueSuffix", "true0", u"true0", identifier_classification::unrecognized_value, false},
        classification_case{"FalseSuffix", "false_", u"false_", identifier_classification::unrecognized_value, false},
        classification_case{"SameLengthMismatch", "trux", u"trux", identifier_classification::unrecognized_value,
                            false},
        classification_case{"PrefixN", "n", u"n", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixNu", "nu", u"nu", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixNul", "nul", u"nul", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixT", "t", u"t", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixTr", "tr", u"tr", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixTru", "tru", u"tru", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixF", "f", u"f", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixFa", "fa", u"fa", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixFal", "fal", u"fal", identifier_classification::unrecognized_value, false},
        classification_case{"PrefixFals", "fals", u"fals", identifier_classification::unrecognized_value, false},
        classification_case{"EscapedNull", R"(\u006Eull)", u"null", identifier_classification::unrecognized_value,
                            true},
        classification_case{"EscapedTrue", R"(tr\u0075e)", u"true", identifier_classification::unrecognized_value,
                            true},
        classification_case{"EscapedFalse", R"(fals\u0065)", u"false", identifier_classification::unrecognized_value,
                            true},
        classification_case{"Unknown", "undefined", u"undefined", identifier_classification::unrecognized_value, false},
        classification_case{"Reserved", "class", u"class", identifier_classification::unrecognized_value, false},
        classification_case{"UnicodeLookalike", u8"tru\u0435", u"tru\u0435",
                            identifier_classification::unrecognized_value, false},
        classification_case{"UnicodeSuffix", u8"true\u200D", u"true\u200D",
                            identifier_classification::unrecognized_value, false},
        classification_case{"InfinityDeferred", "Infinity", u"Infinity", identifier_classification::unrecognized_value,
                            false},
        classification_case{"NaNDeferred", "NaN", u"NaN", identifier_classification::unrecognized_value, false}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<classification_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ================================== End Test_J5_023 ================================== */

} // namespace
