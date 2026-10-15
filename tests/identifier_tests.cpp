// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/identifier_lexer.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  /* ===================================== Fixtures ==================================== */
  /**
   * @brief Host storage with per-request failure injection and release validation.
   * @par Requirements
   * P022; J5-040; J5-046; J5-051.
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
       * P022; J5-046.
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
       * P022; J5-040; J5-051.
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
       * P022; J5-040.
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
       * P022; J5-046.
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
} // namespace
