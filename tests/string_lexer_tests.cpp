// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/string_lexer.hpp"
#include "adaptive_json5/utf8_export.hpp"

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

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Host storage with per-request failure injection and release validation.
   * @par Requirements
   * P021; J5-040; J5-046; J5-051.
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
       * P021; J5-046.
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
       * P021; J5-040; J5-051.
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
       * P021; J5-040.
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
       * P021; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &release};
      }
  };

  /**
   * @brief Capture bounded warning descriptors without allocating in callbacks.
   * @par Requirements
   * P021; J5-017.
   */
  struct warning_log
  {
      std::array<string_warning, 16> m_items{};
      std::size_t m_count{};
      lexer_provider* m_provider{};
      std::size_t m_first_attempt_count{};

      /**
       * @brief Copy one warning and check notification replay performs no allocation.
       * @par Requirements
       * P021; J5-017.
       */
      static void notify(void* context, const string_warning& warning) noexcept
      {
        auto& self = *static_cast<warning_log*>(context);
        if (self.m_provider != nullptr)
        {
          if (self.m_count == 0)
          {
            self.m_first_attempt_count = self.m_provider->m_attempts;
          }
          EXPECT_EQ(self.m_provider->m_attempts, self.m_first_attempt_count);
          EXPECT_GT(self.m_provider->m_live_bytes, 0u);
        }
        if (self.m_count < self.m_items.size())
        {
          self.m_items[self.m_count] = warning;
        }
        else
        {
          ADD_FAILURE() << "Unexpected warning count exceeds test capture capacity";
        }
        ++self.m_count;
      }

      /**
       * @brief Borrow this log as the lexer observer.
       * @par Requirements
       * P021; J5-017.
       */
      string_warning_observer observer() noexcept
      {
        return {this, &notify};
      }
  };

  /**
   * @brief Compare exact decoded units using only the public checked accessor.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-018.
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

  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_014 =================================== */

  static_assert(std::is_nothrow_move_constructible_v<string_token>);
  static_assert(!std::is_copy_constructible_v<string_token>);
  static_assert(noexcept(consume_string(std::declval<cursor&>(), syntax_mode::json5, {}, 0)));
  static_assert(noexcept(consume_string_escape(std::declval<cursor&>(), syntax_mode::json5)));

  /**
   * @brief Complete literal spellings and independent expected decoded units.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018.
   */
  struct string_case
  {
      const char* name;     // Diagnostic parameter name.
      std::string literal;  // Input complete quoted string token.
      std::u16string units; // Expected exact decoded UTF-16 units.
      syntax_mode mode;     // Input syntax selection.
  };
  class J5_014_StringLexer : public ::testing::TestWithParam<string_case>
  {
  };

  /* Verify complete tokens own exact decoded content and original source spans.
   * Arrange: Place one literal token at a nonzero offset followed by unread bytes.
   * Act: Consume it through the complete lexer with an active warning observer.
   * Expect: Exact decoded units, both quotes in the span, unchanged input and suffix.
   */
  TEST_P(J5_014_StringLexer, DecodesOwnedTokenAndExactSpan)
  {
    /* Arrange */
    const auto& param = GetParam();
    lexer_provider provider;
    warning_log log;
    const std::string input = std::string("XX") + param.literal + "Z";
    const auto before = input;
    cursor source({input.data(), input.size()});
    source.advance(2);
    /* Act */
    auto actual = consume_string(source, param.mode, provider.callbacks(), 64, log.observer());
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    const auto& token = **actual.value_if();
    expect_units(token.value, param.units);
    EXPECT_EQ(token.byte_offset, 2u);
    EXPECT_EQ(token.byte_length, param.literal.size());
    EXPECT_EQ(token.warning_count, 0u);
    EXPECT_FALSE(token.first_warning.has_value());
    EXPECT_EQ(log.m_count, 0u);
    EXPECT_EQ(source.offset(), 2u + param.literal.size());
    EXPECT_EQ(*source.peek(), 'Z');
    EXPECT_EQ(input, before);
  }
  INSTANTIATE_TEST_SUITE_P(
    Literals, J5_014_StringLexer,
    ::testing::Values(
      string_case{"EmptyJson5", "\"\"", {}, syntax_mode::json5},
      string_case{"EmptyStrict", "\"\"", {}, syntax_mode::strict_json},
      string_case{"EmptySingle", "''", {}, syntax_mode::json5},
      string_case{"OppositeQuote", "'A\"B'", u"A\"B", syntax_mode::json5},
      string_case{"StrictOppositeQuote", "\"A'B\"", u"A'B", syntax_mode::strict_json},
      string_case{
        "JsonEscapes", R"json("\"\\\/\b\f\n\r\t")json", {0x22, 0x5C, 0x2F, 8, 12, 10, 13, 9}, syntax_mode::strict_json},
      string_case{"Json5Escapes",
                  R"json('\'\"\v\0\a\e\/\ ')json",
                  {0x27, 0x22, 11, 0, 0x61, 0x65, 0x2F, 0x20},
                  syntax_mode::json5},
      string_case{"MixedHexUnicode",
                  R"json("\x00\xFf\u0041\uD800\uDC00")json",
                  {0, 0xFF, 0x41, 0xD800, 0xDC00},
                  syntax_mode::json5},
      string_case{
        "StrictUnicode", R"json("\u0000\u0041\uD83C\uDFBC")json", {0, 0x41, 0xD83C, 0xDFBC}, syntax_mode::strict_json},
      string_case{"LoneHigh", R"json("\uD800")json", {0xD800}, syntax_mode::json5},
      string_case{"LoneLowStrict", R"json("\uDFFF")json", {0xDFFF}, syntax_mode::strict_json},
      string_case{"ReversedUnits", R"json("\uDC00\uD800")json", {0xDC00, 0xD800}, syntax_mode::json5},
      string_case{"RawSupplementary", "\"\xF0\x9F\x8E\xBC\"", {0xD83C, 0xDFBC}, syntax_mode::strict_json},
      string_case{"EscapedSupplementary", "\"\\\xF0\x9F\x8E\xBC\"", {0xD83C, 0xDFBC}, syntax_mode::json5},
      string_case{"RawControls",
                  std::string("\"A\0\t\x01"
                              "B\"",
                              7),
                  {u'A', 0, 9, 1, u'B'},
                  syntax_mode::json5},
      string_case{"CommentText", "\"/*x*/ //y\"", u"/*x*/ //y", syntax_mode::json5},
      string_case{"AllContinuations",
                  "\"A\\\nB\\\rC\\\r\nD\\\xE2\x80\xA8"
                  "E\\\xE2\x80\xA9"
                  "F\"",
                  u"ABCDEF", syntax_mode::json5}),
    [](const ::testing::TestParamInfo<string_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Invalid complete-string inputs with independent error locations.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017.
   */
  struct failure_case
  {
      const char* name;    // Diagnostic parameter name.
      std::string literal; // Input malformed or mode-forbidden string.
      syntax_mode mode;    // Input syntax selection.
      error_code code;     // Expected failure category.
      std::size_t offset;  // Expected byte offset relative to the input literal.
  };
  class J5_014_StringFailure : public ::testing::TestWithParam<failure_case>
  {
  };

  /* Verify lexical failure rolls back the entire token and releases partial storage.
   * Arrange: Put malformed input at a nonzero offset with storage and warning capture.
   * Act: Consume the complete string.
   * Expect: Exact located error, original caller position, no warnings and no live storage.
   */
  TEST_P(J5_014_StringFailure, ReportsErrorAndRollsBackWholeToken)
  {
    /* Arrange */
    const auto& param = GetParam();
    lexer_provider provider;
    warning_log log;
    const std::string input = std::string("XX") + param.literal;
    const auto before = input;
    cursor source({input.data(), input.size()});
    source.advance(2);
    /* Act */
    auto actual = consume_string(source, param.mode, provider.callbacks(), 64, log.observer());
    /* Expect */
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, param.code);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 2u + param.offset);
    EXPECT_EQ(source.offset(), 2u);
    EXPECT_EQ(log.m_count, 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
    EXPECT_EQ(input, before);
  }
  INSTANTIATE_TEST_SUITE_P(
    Invalid, J5_014_StringFailure,
    ::testing::Values(
      failure_case{"UnclosedDouble", "\"abc", syntax_mode::json5, error_code::syntax, 4},
      failure_case{"UnclosedSingle", "'abc", syntax_mode::json5, error_code::syntax, 4},
      failure_case{"RawLf", "\"a\nb\"", syntax_mode::json5, error_code::syntax, 2},
      failure_case{"RawCr", "\"a\rb\"", syntax_mode::json5, error_code::syntax, 2},
      failure_case{"StrictTab", "\"a\tb\"", syntax_mode::strict_json, error_code::syntax, 2},
      failure_case{"StrictNul", std::string("\"A\0B\"", 5), syntax_mode::strict_json, error_code::syntax, 2},
      failure_case{"StrictSingle", "'a'", syntax_mode::strict_json, error_code::syntax, 0},
      failure_case{"TrailingBackslash", "\"A\\", syntax_mode::json5, error_code::syntax, 3},
      failure_case{"DigitEight", "\"\\8\"", syntax_mode::json5, error_code::syntax, 2},
      failure_case{"DigitNine", "\"\\9\"", syntax_mode::json5, error_code::syntax, 2},
      failure_case{"ZeroDigit", "\"\\01\"", syntax_mode::json5, error_code::syntax, 3},
      failure_case{"HexLetter", "\"\\xG1\"", syntax_mode::json5, error_code::syntax, 3},
      failure_case{"HexTruncated", "\"\\x4", syntax_mode::json5, error_code::syntax, 4},
      failure_case{"UnicodeLetter", "\"\\u12G4\"", syntax_mode::json5, error_code::syntax, 5},
      failure_case{"UnicodeBrace", "\"\\u{41}\"", syntax_mode::json5, error_code::syntax, 3},
      failure_case{"StrictVerticalTab", "\"\\v\"", syntax_mode::strict_json, error_code::syntax, 2},
      failure_case{"StrictNulEscape", "\"\\0\"", syntax_mode::strict_json, error_code::syntax, 2},
      failure_case{"StrictNonEscape", "\"\\a\"", syntax_mode::strict_json, error_code::syntax, 2},
      failure_case{"StrictHex", "\"\\x41\"", syntax_mode::strict_json, error_code::syntax, 2},
      failure_case{"StrictContinuation", "\"\\\n\"", syntax_mode::strict_json, error_code::syntax, 2},
      failure_case{"RawInvalidUtf8", "\"A\x80\"", syntax_mode::json5, error_code::encoding, 2},
      failure_case{"EscapedInvalidUtf8", "\"\\\x80\"", syntax_mode::json5, error_code::encoding, 2},
      failure_case{"HexInvalidUtf8", "\"\\x\xED\xA0\x80\"", syntax_mode::json5, error_code::encoding, 3},
      failure_case{"UnicodeInvalidUtf8", "\"\\u0\xC0\xAF\"", syntax_mode::json5, error_code::encoding, 4}),
    [](const ::testing::TestParamInfo<failure_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify nonquote input does not allocate, skip trivia or notify observers.
   * Arrange: Select every nonquote initial byte in both modes and empty views.
   * Act: Attempt full string recognition with a failing provider.
   * Expect: Successful no-match and no observable state changes.
   */
  TEST(J5_014_StringBoundary, LeavesNonquotesAndEmptyInputUntouched)
  {
    /* Arrange */
    lexer_provider provider;
    provider.m_fail_on = 1;
    warning_log log;
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (unsigned int byte = 0; byte <= 255; ++byte)
      {
        if (byte == '"' || byte == '\'')
        {
          continue;
        }
        const char input = static_cast<char>(byte);
        cursor source({&input, 1});
        /* Act */
        auto actual = consume_string(source, mode, provider.callbacks(), 64, log.observer());
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 0u);
      }
      const char hidden = '"';
      for (const auto view : {input_view{}, input_view{&hidden, 0}})
      {
        cursor source(view);
        auto actual = consume_string(source, mode, provider.callbacks(), 64, log.observer());
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
      }
    }
    EXPECT_EQ(provider.m_attempts, 0u);
    EXPECT_EQ(log.m_count, 0u);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify every proper byte prefix of a combined string fails atomically.
   * Arrange: Combine escaped quotes, hex, Unicode pairs, CRLF continuation and raw UTF-8.
   * Act: Bound the input at each byte before the sole unescaped closing quote.
   * Expect: No partial token, syntax/encoding within bounds, no callbacks and cleanup.
   */
  TEST(J5_014_StringBoundary, RejectsEveryCombinedStringPrefixAtomically)
  {
    /* Arrange */
    const std::string literal =
      std::string(R"json("A\"B\x41\uD83C\uDFBC)json") + "\\\r\n\xF0\x9F\x8E\xBC\xE2\x80\xA8\"";
    const std::string input = std::string("XX") + literal;
    for (std::size_t length = 1; length < literal.size(); ++length)
    {
      lexer_provider provider;
      warning_log log;
      cursor source({input.data(), 2 + length});
      source.advance(2);
      /* Act */
      auto actual = consume_string(source, syntax_mode::json5, provider.callbacks(), 64, log.observer());
      /* Expect */
      ASSERT_FALSE(actual.has_value()) << length;
      EXPECT_TRUE(actual.error_if()->code == error_code::syntax || actual.error_if()->code == error_code::encoding);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_GE(actual.error_if()->byte_offset, 2u);
      EXPECT_LE(actual.error_if()->byte_offset, 2u + length);
      EXPECT_EQ(source.offset(), 2u);
      EXPECT_EQ(log.m_count, 0u);
      EXPECT_EQ(provider.m_live_bytes, 0u);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify closing recognition stops before unrelated bytes and adjacent strings.
   * Arrange: Supply consecutive string tokens followed by malformed UTF-8.
   * Act: Consume exactly two strings in sequence.
   * Expect: Correct per-token spans and the malformed suffix remains unread.
   */
  TEST(J5_014_StringBoundary, StopsAtClosingQuoteWithoutValidatingSuffix)
  {
    /* Arrange */
    lexer_provider provider;
    const std::string input = "\"A\"'B'\x80";
    cursor source({input.data(), input.size()});
    /* Act */
    auto first = consume_string(source, syntax_mode::json5, provider.callbacks(), 1);
    auto second = consume_string(source, syntax_mode::json5, provider.callbacks(), 1);
    /* Expect */
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(first.value_if()->has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(second.value_if()->has_value());
    expect_units((**first.value_if()).value, u"A");
    expect_units((**second.value_if()).value, u"B");
    EXPECT_EQ((**first.value_if()).byte_offset, 0u);
    EXPECT_EQ((**second.value_if()).byte_offset, 3u);
    EXPECT_EQ((**first.value_if()).byte_length, 3u);
    EXPECT_EQ((**second.value_if()).byte_length, 3u);
    EXPECT_EQ(source.offset(), 6u);
    EXPECT_EQ(*source.peek(), input[6]);
  }

  /* ================================= End Test_J5_014 ================================= */

  /* =================================== Test_J5_017 =================================== */

  /* Verify warnings are ordered, source-based, optional and independent of content.
   * Arrange: Mix raw separators with escaped/continued equivalents and other escapes.
   * Act: Parse with a warning observer, then with callbacks disabled.
   * Expect: Exactly two raw warnings, identical owned content and summary in both runs.
   */
  TEST(J5_017_StringLexer, DeliversOnlyRawWarningsAfterSuccessfulConstruction)
  {
    /* Arrange */
    lexer_provider provider;
    warning_log log;
    log.m_provider = &provider;
    std::string literal =
      std::string("\"") + "\xE2\x80\xA8" + R"json(\u2029)json" + "\\\xE2\x80\xA8" + R"json(\x41\a)json";
    const auto last_warning_offset = literal.size();
    literal += "\xE2\x80\xA9\"";
    cursor source({literal.data(), literal.size()});
    /* Act */
    auto actual = consume_string(source, syntax_mode::json5, provider.callbacks(), 8, log.observer());
    cursor no_observer_source({literal.data(), literal.size()});
    auto no_observer = consume_string(no_observer_source, syntax_mode::json5, provider.callbacks(), 8, {&log, nullptr});
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    ASSERT_TRUE(no_observer.has_value());
    ASSERT_TRUE(no_observer.value_if()->has_value());
    const auto& token = **actual.value_if();
    const auto& quiet = **no_observer.value_if();
    expect_units(token.value, {0x2028, 0x2029, 0x41, 0x61, 0x2029});
    expect_units(quiet.value, {0x2028, 0x2029, 0x41, 0x61, 0x2029});
    EXPECT_EQ(token.warning_count, 2u);
    EXPECT_EQ(quiet.warning_count, 2u);
    ASSERT_TRUE(token.first_warning.has_value());
    ASSERT_TRUE(quiet.first_warning.has_value());
    EXPECT_EQ(token.first_warning->code, string_warning_code::raw_line_separator);
    EXPECT_EQ(token.first_warning->byte_offset, 1u);
    EXPECT_EQ(quiet.first_warning->byte_offset, 1u);
    ASSERT_EQ(log.m_count, 2u);
    EXPECT_EQ(log.m_items[0].code, string_warning_code::raw_line_separator);
    EXPECT_EQ(log.m_items[0].byte_offset, 1u);
    EXPECT_EQ(log.m_items[0].byte_length, 3u);
    EXPECT_EQ(log.m_items[1].code, string_warning_code::raw_paragraph_separator);
    EXPECT_EQ(log.m_items[1].byte_offset, last_warning_offset);
    EXPECT_EQ(log.m_items[1].byte_length, 3u);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify raw separators are valid strict JSON without JSON5 warning callbacks.
   * Arrange: Put both raw separators in a strict string with an active observer.
   * Act: Parse and export the owned value.
   * Expect: Successful unchanged bytes and no warning summary or notifications.
   */
  TEST(J5_017_StringLexer, StrictSeparatorsDoNotWarn)
  {
    /* Arrange */
    lexer_provider provider;
    warning_log log;
    const std::string input = "\"\xE2\x80\xA8\xE2\x80\xA9\"";
    cursor source({input.data(), input.size()});
    /* Act */
    auto actual = consume_string(source, syntax_mode::strict_json, provider.callbacks(), 2, log.observer());
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    const auto& token = **actual.value_if();
    expect_units(token.value, {0x2028, 0x2029});
    EXPECT_EQ(token.warning_count, 0u);
    EXPECT_FALSE(token.first_warning.has_value());
    EXPECT_EQ(log.m_count, 0u);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a warning seen before a later lexical error never escapes the failed parse.
   * Arrange: Follow a raw separator with EOF, malformed escape, control or bad UTF-8.
   * Act: Parse each complete input attempt with an active observer.
   * Expect: Failure, no callbacks, original position and no live partial storage.
   */
  TEST(J5_017_StringLexer, SuppressesWarningsWhenLaterLexingFails)
  {
    /* Arrange */
    for (const std::string suffix : {std::string{}, std::string("\\xG1\""), std::string("\n\""), std::string("\x80\"")})
    {
      lexer_provider provider;
      warning_log log;
      const std::string input = std::string("\"\xE2\x80\xA8") + suffix;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_string(source, syntax_mode::json5, provider.callbacks(), 64, log.observer());
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(log.m_count, 0u);
      EXPECT_EQ(source.offset(), 0u);
      EXPECT_EQ(provider.m_live_bytes, 0u);
    }
  }

  /* ================================= End Test_J5_017 ================================= */

  /* =================================== Test_J5_040 =================================== */

  /* Verify token ownership survives input destruction and token/result moves.
   * Arrange: Parse mutable input containing NUL and an escaped surrogate pair.
   * Act: Overwrite and destroy input, move the token out, then move it again.
   * Expect: Exact owned units, retained numeric spans and one final live allocation.
   */
  TEST(J5_040_StringLexer, OwnsContentAcrossInputDestructionAndMoves)
  {
    /* Arrange */
    lexer_provider provider;
    {
      auto parsed = [&provider]()
      {
        std::string input = R"json("A\0\uD83C\uDFBC")json";
        cursor source({input.data(), input.size()});
        auto actual = consume_string(source, syntax_mode::json5, provider.callbacks(), 4);
        input.assign(input.size(), 'X');
        return actual;
      }();
      ASSERT_TRUE(parsed.has_value());
      ASSERT_TRUE(parsed.value_if()->has_value());
      const auto length = (**parsed.value_if()).byte_length;
      /* Act */
      string_token first(std::move(**parsed.value_if()));
      string_token moved(std::move(first));
      /* Expect */
      expect_units(moved.value, {u'A', 0, 0xD83C, 0xDFBC});
      EXPECT_EQ(moved.byte_offset, 0u);
      EXPECT_EQ(moved.byte_length, length);
      EXPECT_EQ(first.value.size(), 0u);
      EXPECT_EQ((**parsed.value_if()).value.size(), 0u);
      EXPECT_GT(provider.m_live_bytes, 0u);
    }
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }

  /* ================================= End Test_J5_040 ================================= */

  /* =================================== Test_J5_046 =================================== */

  /* Verify each actual growth allocation can fail with full token rollback.
   * Arrange: Measure allocation requests for a string mixing warnings and escapes.
   * Act: Fail each request in turn, then retry the unchanged cursor after exhaustion.
   * Expect: Located allocation failure, no warnings/leaks, followed by exact retry success.
   */
  TEST(J5_046_StringLexer, FailsEveryAllocationAtomicallyAndAllowsRetry)
  {
    /* Arrange */
    const std::string input = "\"\xE2\x80\xA8\\\xF0\x9F\x8E\xBC"
                              "ABCDEF\"";
    std::size_t request_count{};
    {
      lexer_provider baseline;
      cursor source({input.data(), input.size()});
      auto actual = consume_string(source, syntax_mode::json5, baseline.callbacks(), 9);
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      request_count = baseline.m_attempts;
    }
    ASSERT_GT(request_count, 1u);
    for (std::size_t failure = 1; failure <= request_count; ++failure)
    {
      lexer_provider provider;
      provider.m_fail_on = failure;
      warning_log log;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_string(source, syntax_mode::json5, provider.callbacks(), 9, log.observer());
      /* Expect */
      ASSERT_FALSE(actual.has_value()) << failure;
      EXPECT_EQ(actual.error_if()->code, error_code::allocation_failure);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_LT(actual.error_if()->byte_offset, input.size());
      EXPECT_EQ(source.offset(), 0u);
      EXPECT_EQ(log.m_count, 0u);
      EXPECT_EQ(provider.m_live_bytes, 0u);
      EXPECT_EQ(provider.m_allocations, provider.m_releases);
      provider.m_fail_on = 0;
      auto retried = consume_string(source, syntax_mode::json5, provider.callbacks(), 9, log.observer());
      ASSERT_TRUE(retried.has_value());
      ASSERT_TRUE(retried.value_if()->has_value());
      expect_units((**retried.value_if()).value, {0x2028, 0xD83C, 0xDFBC, u'A', u'B', u'C', u'D', u'E', u'F'});
      EXPECT_EQ(log.m_count, 1u);
      EXPECT_TRUE(source.is_at_end());
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify exact decoded limits for raw and escaped supplementary values.
   * Arrange: Use raw scalar, non-escape scalar, escaped pair and warned ASCII suffix.
   * Act: Parse at and below each required unit count.
   * Expect: Exact success at the limit or located resource failure with full rollback.
   */
  TEST(J5_046_StringLexer, EnforcesLimitsWithoutPartialTokensOrWarnings)
  {
    /* Arrange */
    const std::array<std::string, 4> inputs{"\"\xF0\x9F\x8E\xBC\"", "\"\\\xF0\x9F\x8E\xBC\"",
                                            R"json("\uD83C\uDFBC")json",
                                            "\"\xE2\x80\xA8"
                                            "A\""};
    for (std::size_t index = 0; index < inputs.size(); ++index)
    {
      for (std::size_t limit = 0; limit <= 2; ++limit)
      {
        lexer_provider provider;
        warning_log log;
        cursor source({inputs[index].data(), inputs[index].size()});
        /* Act */
        auto actual = consume_string(source, syntax_mode::json5, provider.callbacks(), limit, log.observer());
        /* Expect */
        ASSERT_EQ(actual.has_value(), limit == 2);
        if (limit == 2)
        {
          ASSERT_TRUE(actual.value_if()->has_value());
          EXPECT_EQ((**actual.value_if()).value.size(), 2u);
          EXPECT_TRUE(source.is_at_end());
          EXPECT_EQ(log.m_count, index == 3 ? 1u : 0u);
        }
        else
        {
          EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          const std::size_t expected = limit == 0 ? 1 : (index == 2 ? 7 : (index == 3 ? 4 : 1));
          EXPECT_EQ(actual.error_if()->byte_offset, expected);
          EXPECT_EQ(source.offset(), 0u);
          EXPECT_EQ(log.m_count, 0u);
          EXPECT_EQ(provider.m_live_bytes, 0u);
        }
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify no-content strings work without storage and missing providers fail explicitly.
   * Arrange: Use empty strings and a continuation-only string with a zero unit limit.
   * Act: Parse those strings, then nonempty content without a provider.
   * Expect: Allocation-free empty success and located allocation failure for content.
   */
  TEST(J5_046_StringLexer, HandlesEmptyContentWithoutProvider)
  {
    /* Arrange */
    for (const std::string input : {std::string("\"\""), std::string("''"), std::string("\"\\\r\n\"")})
    {
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_string(source, syntax_mode::json5, {}, 0);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ((**actual.value_if()).value.size(), 0u);
      EXPECT_TRUE(source.is_at_end());
    }
    const std::string input = "\"A\"";
    cursor source({input.data(), input.size()});
    auto actual = consume_string(source, syntax_mode::json5, {}, 1);
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, error_code::allocation_failure);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 1u);
    EXPECT_EQ(source.offset(), 0u);
  }

  /* ================================= End Test_J5_046 ================================= */
} // namespace
