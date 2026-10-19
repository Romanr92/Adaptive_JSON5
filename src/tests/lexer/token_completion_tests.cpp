// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/scalar_lexer.hpp"
#include "adaptive_json5/detail/lexer/trivia.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
namespace
{
  using namespace adaptive_json5;
  /* ===================================== Fixtures ===================================== */
  /**
   * @brief Host storage with per-request failure injection and release validation.
   * @par Requirements
   * P028; J5-040; J5-046; J5-051.
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
       * P028; J5-046.
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
       * P028; J5-040; J5-051.
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
       * P028; J5-040.
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
       * P028; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &release};
      }
  };

  /**
   * @brief Compare exact decoded units using only the public checked accessor.
   * @par Requirements
   * P028; J5-024.
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

  /* =================================== End Fixtures =================================== */
  /* ==================================== Test_J5_024 ==================================== */
  static_assert(noexcept(check_unquoted_token_boundary(std::declval<cursor>(), syntax_mode::json5)));
  static_assert(noexcept(consume_number(std::declval<cursor&>(), syntax_mode::json5)));
  static_assert(noexcept(consume_scalar(std::declval<cursor&>(), syntax_mode::json5, {}, 0)));
  static_assert(std::is_nothrow_move_constructible_v<scalar_token>);
  static_assert(!std::is_copy_constructible_v<scalar_token>);
  /**
   * @brief Explicit boundary acceptance per syntax mode.
   * @par Requirements
   * P028; J5-024.
   */
  struct boundary_case
  {
      const char* name;   // Input case label.
      std::string suffix; // Input suffix bytes.
      bool json5_ok;      // Expected JSON5 boundary acceptance.
      bool json_ok;       // Expected strict JSON boundary acceptance.
  };
  class J5_024_Boundary : public testing::TestWithParam<std::tuple<boundary_case, syntax_mode>>
  {
  };
  /* Check immediate boundaries without consuming punctuation or trivia.
   * Arrange: Position after a multibyte prefix before each literal suffix.
   * Act: Inspect the boundary in the selected mode.
   * Expect: Mode-specific success or syntax at the original offset; cursor/input unchanged.
   */
  TEST_P(J5_024_Boundary, ChecksModeAndPreservesCursor)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    const auto input = std::string(u8"\u03B1:") + item.suffix;
    const auto original = input;
    cursor source({input.data(), input.size()});
    source.advance(3);
    /* Act */
    const auto actual = check_unquoted_token_boundary(source, mode);
    /* Expect */
    const bool accepted = mode == syntax_mode::json5 ? item.json5_ok : item.json_ok;
    EXPECT_EQ(actual.has_value(), accepted);
    if (!accepted)
    {
      ASSERT_NE(actual.error_if(), nullptr);
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 3u);
    }
    EXPECT_EQ(source.offset(), 3u);
    EXPECT_EQ(input, original);
  }
  INSTANTIATE_TEST_SUITE_P(
    P028, J5_024_Boundary,
    testing::Combine(
      testing::Values(boundary_case{"Eof", "", true, true}, boundary_case{"Space", " ", true, true},
                      boundary_case{"Tab", "\t", true, true}, boundary_case{"Cr", "\r", true, true},
                      boundary_case{"Lf", "\n", true, true}, boundary_case{"LeftBrace", "{", true, true},
                      boundary_case{"RightBrace", "}", true, true}, boundary_case{"LeftBracket", "[", true, true},
                      boundary_case{"RightBracket", "]", true, true}, boundary_case{"Comma", ",", true, true},
                      boundary_case{"Colon", ":", true, true}, boundary_case{"Vt", "\v", true, false},
                      boundary_case{"Ff", "\f", true, false}, boundary_case{"Nbsp", u8"\u00A0", true, false},
                      boundary_case{"Bom", u8"\uFEFF", true, false}, boundary_case{"Zs", u8"\u2000", true, false},
                      boundary_case{"Ls", u8"\u2028", true, false}, boundary_case{"Ps", u8"\u2029", true, false},
                      boundary_case{"LineComment", "//", true, false}, boundary_case{"BlockOpener", "/*", true, false},
                      boundary_case{"Slash", "/", false, false}, boundary_case{"NotComment", "/x", false, false},
                      boundary_case{"Letter", "f", false, false}, boundary_case{"Digit", "2", false, false},
                      boundary_case{"Underscore", "_", false, false}, boundary_case{"Point", ".", false, false},
                      boundary_case{"Plus", "+", false, false}, boundary_case{"Minus", "-", false, false},
                      boundary_case{"Quote", "\"", false, false}, boundary_case{"SingleQuote", "'", false, false},
                      boundary_case{"Backslash", "\\", false, false},
                      boundary_case{"Nul", std::string(1, '\0'), false, false}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<boundary_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief Explicit scalar/numeric rejection expectations.
   * @par Requirements
   * P028; J5-023; J5-024.
   */
  struct rejection_case
  {
      const char* name;         // Input case label.
      std::string text;         // Input source spelling.
      std::size_t json5_offset; // Expected JSON5 error byte offset.
      std::size_t json_offset;  // Expected strict JSON error byte offset.
      bool numeric;             // Input is also attempted through the public number wrapper.
  };
  class J5_024_TokenRejection : public testing::TestWithParam<std::tuple<rejection_case, syntax_mode>>
  {
  };
  /* Never publish a valid prefix of an invalid complete spelling.
   * Arrange: Provide explicit invalid spellings at a nonzero input offset.
   * Act: Consume a scalar and, where applicable, a numeric token.
   * Expect: Exact syntax location, original cursor and balanced temporary storage.
   */
  TEST_P(J5_024_TokenRejection, RejectsInvalidSpellingAtomically)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    const std::string input = "!!" + item.text;
    lexer_provider provider;
    cursor source({input.data(), input.size()});
    source.advance(2);
    const auto expected = 2 + (mode == syntax_mode::json5 ? item.json5_offset : item.json_offset);
    /* Act */
    auto actual = consume_scalar(source, mode, provider.callbacks(), 64);
    /* Expect */
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, error_code::syntax);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, expected);
    EXPECT_EQ(source.offset(), 2u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
    if (item.numeric)
    {
      auto number = consume_number(source, mode);
      ASSERT_FALSE(number.has_value());
      EXPECT_EQ(number.error_if()->code, error_code::syntax);
      EXPECT_TRUE(number.error_if()->has_byte_offset);
      EXPECT_EQ(number.error_if()->byte_offset, expected);
      EXPECT_EQ(source.offset(), 2u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    P028, J5_024_TokenRejection,
    testing::Combine(
      testing::Values(
        rejection_case{"TrueSuffix", "truex", 0, 0, false}, rejection_case{"Unknown", "hello", 0, 0, false},
        rejection_case{"WrongCase", "True", 0, 0, false}, rejection_case{"EscapedTrue", R"(tr\u0075e)", 0, 0, false},
        rejection_case{"NumberLetter", "1foo", 1, 1, true}, rejection_case{"Binary", "0b1", 1, 1, true},
        rejection_case{"OctalPrefix", "0o7", 1, 1, true}, rejection_case{"OctalDigits", "077", 1, 1, true},
        rejection_case{"Separator", "1_0", 1, 1, true}, rejection_case{"HexSuffix", "0x1z", 3, 0, true},
        rejection_case{"HexIncomplete", "0x", 2, 0, true}, rejection_case{"InfinitySuffix", "Infinityx", 8, 0, true},
        rejection_case{"NanPoint", "NaN.0", 3, 0, true}, rejection_case{"PlusPlus", "++1", 1, 0, true},
        rejection_case{"DoublePoint", "1.2.3", 3, 3, true}, rejection_case{"NumericEscape", R"(1\u0061)", 1, 1, true},
        rejection_case{"NumericQuote", "1\"x\"", 1, 1, true}, rejection_case{"LiteralPoint", "true.", 4, 4, false},
        rejection_case{"LiteralQuote", "true\"x\"", 4, 4, false}, rejection_case{"StringName", R"("a"x)", 3, 3, false},
        rejection_case{"AdjacentStrings", R"("a""b")", 3, 3, false},
        rejection_case{"StringPoint", R"("a".)", 3, 3, false}, rejection_case{"StrictSingleQuote", "'a'x", 3, 0, false},
        rejection_case{"LoneSlash", "1/", 1, 1, true}, rejection_case{"NotComment", "1/x", 1, 1, true},
        rejection_case{"NulSuffix", std::string("1\0", 2), 1, 1, true}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<rejection_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ----------------------------------------------------------------------------------- */
  /* Return exact variant alternatives and leave admitted boundaries unconsumed.
   * Arrange: Valid literals, each numeric family and strings with caller storage.
   * Act: Recognize at a nonzero offset.
   * Expect: Exact variant, kind/content/span and cursor at the comma.
   */
  TEST(J5_024_Scalar, ReturnsExactAlternativesAndSpans)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& spelling : {std::string{"null"}, std::string{"true"}, std::string{"false"}})
      {
        lexer_provider provider;
        const auto input = "!!" + spelling + ",";
        cursor source({input.data(), input.size()});
        source.advance(2);
        /* Act */
        auto actual = consume_scalar(source, mode, provider.callbacks(), spelling.size());
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        const auto* token = std::get_if<literal_token>(&**actual.value_if());
        ASSERT_NE(token, nullptr);
        EXPECT_EQ(token->kind, spelling == "null"   ? literal_kind::null_literal
                               : spelling == "true" ? literal_kind::true_literal
                                                    : literal_kind::false_literal);
        EXPECT_EQ(token->byte_offset, 2u);
        EXPECT_EQ(token->byte_length, spelling.size());
        EXPECT_EQ(source.offset(), 2 + spelling.size());
        EXPECT_EQ(source.peek(), std::optional<char>{','});
        EXPECT_EQ(provider.m_live_bytes, 0u);
      }
      const std::string input = R"("a\u0062",)";
      lexer_provider provider;
      cursor source({input.data(), input.size()});
      auto actual = consume_scalar(source, mode, provider.callbacks(), 2);
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      const auto* token = std::get_if<string_token>(&**actual.value_if());
      ASSERT_NE(token, nullptr);
      expect_units(token->value, u"ab");
      EXPECT_EQ(token->byte_offset, 0u);
      EXPECT_EQ(token->byte_length, 9u);
      EXPECT_EQ(source.offset(), 9u);
    }
    const std::pair<std::string, number_kind> numbers[] = {{"-12.5e+2", number_kind::decimal},
                                                           {"+0XAf", number_kind::hexadecimal},
                                                           {"-Infinity", number_kind::infinity},
                                                           {"+NaN", number_kind::nan}};
    for (const auto& item : numbers)
    {
      const auto input = "!!" + item.first + ",";
      cursor source({input.data(), input.size()});
      source.advance(2);
      auto actual = consume_scalar(source, syntax_mode::json5, {}, 0);
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      const auto* token = std::get_if<number_token>(&**actual.value_if());
      ASSERT_NE(token, nullptr);
      EXPECT_EQ(token->kind, item.second);
      EXPECT_EQ(token->byte_offset, 2u);
      EXPECT_EQ(token->byte_length, item.first.size());
      EXPECT_EQ(source.offset(), 2 + item.first.size());
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* EOF and structural tokens remain available without storage requests.
   * Arrange: Empty views and all structural punctuation in both modes.
   * Act: Consume a possible scalar.
   * Expect: Successful no-match, original cursor and no allocation.
   */
  TEST(J5_024_Scalar, PreservesStructuralNoMatch)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& input : {std::string{}, std::string{"{"}, std::string{"}"}, std::string{"["}, std::string{"]"},
                                std::string{":"}, std::string{","}})
      {
        lexer_provider provider;
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_scalar(source, mode, provider.callbacks(), 0);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 0u);
        EXPECT_EQ(provider.m_attempts, 0u);
      }
      cursor empty({nullptr, 0});
      auto actual = consume_scalar(empty, mode, {}, 0);
      ASSERT_TRUE(actual.has_value());
      EXPECT_FALSE(actual.value_if()->has_value());
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Preserve encoding failures after otherwise valid scalar spellings.
   * Arrange: Malformed/truncated UTF-8 after numbers, literals and quoted strings.
   * Act: Consume through both scalar and boundary APIs.
   * Expect: Encoding errors at original suffix offset and full cursor/storage rollback.
   */
  TEST(J5_024_ScalarEncoding, RollsBackMalformedBoundary)
  {
    /* Arrange */
    const std::string invalid[] = {"\x80", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82"};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const std::string prefix : {std::string{"1"}, std::string{"true"}, std::string{R"("a")"}})
      {
        for (const auto& bytes : invalid)
        {
          lexer_provider provider;
          const auto input = "!!" + prefix + bytes;
          cursor source({input.data(), input.size()});
          source.advance(2);
          /* Act */
          auto actual = consume_scalar(source, mode, provider.callbacks(), 32);
          cursor boundary_source = source;
          boundary_source.advance(prefix.size());
          const auto boundary = check_unquoted_token_boundary(boundary_source, mode);
          /* Expect */
          ASSERT_FALSE(actual.has_value());
          EXPECT_EQ(actual.error_if()->code, error_code::encoding);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, 2 + prefix.size());
          EXPECT_EQ(source.offset(), 2u);
          ASSERT_FALSE(boundary.has_value());
          EXPECT_EQ(boundary.error_if()->code, error_code::encoding);
          EXPECT_EQ(boundary.error_if()->byte_offset, 2 + prefix.size());
          EXPECT_EQ(boundary_source.offset(), 2 + prefix.size());
          EXPECT_EQ(provider.m_live_bytes, 0u);
        }
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Never concatenate scalar pieces across skipped trivia.
   * Arrange: Numbers or incomplete literal names separated by whitespace/comments.
   * Act: Consume the first component, explicitly skip trivia, then inspect the next component.
   * Expect: Distinct source spans or immediate syntax failure, never a joined spelling.
   */
  TEST(J5_024_ScalarTrivia, DoesNotJoinSeparatedSpellings)
  {
    /* Arrange */
    for (const auto& separator : {std::string{" "}, std::string{"/*x*/"}, std::string{"//x\n"}})
    {
      lexer_provider provider;
      const auto input = "1" + separator + "2";
      cursor source({input.data(), input.size()});
      /* Act */
      auto first = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 8);
      /* Expect */
      ASSERT_TRUE(first.has_value());
      ASSERT_TRUE(first.value_if()->has_value());
      EXPECT_EQ(source.offset(), 1u);
      const auto* first_number = std::get_if<number_token>(&**first.value_if());
      ASSERT_NE(first_number, nullptr);
      EXPECT_EQ(first_number->byte_length, 1u);
      const auto trivia = skip_trivia(source, syntax_mode::json5);
      ASSERT_TRUE(trivia.has_value());
      EXPECT_EQ(trivia.value_if()->byte_length, separator.size());
      auto second = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 8);
      ASSERT_TRUE(second.has_value());
      ASSERT_TRUE(second.value_if()->has_value());
      const auto* second_number = std::get_if<number_token>(&**second.value_if());
      ASSERT_NE(second_number, nullptr);
      EXPECT_EQ(second_number->byte_offset, 1 + separator.size());
      EXPECT_EQ(second_number->byte_length, 1u);
      const auto split = "tr" + separator + "ue";
      cursor split_source({split.data(), split.size()});
      auto rejected = consume_scalar(split_source, syntax_mode::json5, provider.callbacks(), 8);
      ASSERT_FALSE(rejected.has_value());
      EXPECT_EQ(rejected.error_if()->code, error_code::syntax);
      EXPECT_EQ(split_source.offset(), 0u);
    }
  }

  /* ----------------------------------------------------------------------------------- */
  /* Apply mode-specific boundaries equally to all scalar alternatives.
   * Arrange: Valid number/literal/string spellings with JSON5-only whitespace/comments.
   * Act: Consume the same source in both modes.
   * Expect: JSON5 success stops at the boundary; strict mode fails there atomically.
   */
  TEST(J5_024_Scalar, AppliesModeSpecificBoundariesToEveryAlternative)
  {
    /* Arrange */
    for (const std::string spelling : {std::string{"1"}, std::string{"true"}, std::string{R"("a")"}})
    {
      for (const std::string ending : {std::string{"/*x*/"}, std::string{"//x"}, std::string{u8"\u00A0"}})
      {
        for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
        {
          lexer_provider provider;
          const auto input = spelling + ending;
          cursor source({input.data(), input.size()});
          /* Act */
          auto actual = consume_scalar(source, mode, provider.callbacks(), 8);
          /* Expect */
          if (mode == syntax_mode::json5)
          {
            ASSERT_TRUE(actual.has_value());
            ASSERT_TRUE(actual.value_if()->has_value());
            EXPECT_EQ(source.offset(), spelling.size());
          }
          else
          {
            ASSERT_FALSE(actual.has_value());
            EXPECT_EQ(actual.error_if()->code, error_code::syntax);
            EXPECT_TRUE(actual.error_if()->has_byte_offset);
            EXPECT_EQ(actual.error_if()->byte_offset, spelling.size());
            EXPECT_EQ(source.offset(), 0u);
            EXPECT_EQ(provider.m_live_bytes, 0u);
          }
        }
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Reject truncated string/number components without bypassing the selected reader.
   * Arrange: Explicit incomplete string, escape and exponent source views.
   * Act: Consume scalar tokens in both modes.
   * Expect: Located syntax failure, original cursor and complete storage cleanup.
   */
  TEST(J5_024_Scalar, PropagatesIncompleteComponentFailures)
  {
    /* Arrange */
    const std::string inputs[] = {R"("abc)", R"("a\u00)", "1e", "1e+", "-"};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const auto& input : inputs)
      {
        lexer_provider provider;
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_scalar(source, mode, provider.callbacks(), 32);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::syntax);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_EQ(actual.error_if()->byte_offset, input.size());
        EXPECT_EQ(source.offset(), 0u);
        EXPECT_EQ(provider.m_live_bytes, 0u);
      }
    }
  }

  /* ================================== End Test_J5_024 ================================== */
  /* ==================================== Test_J5_046 ==================================== */
  /* Fail every observed string/literal allocation and retry without partial tokens.
   * Arrange: Measure actual requests, then inject each failure for strings and literals.
   * Act: Recognize, fail, and retry from the unchanged caller cursor.
   * Expect: Explicit allocation failure, balanced cleanup and successful retry.
   */
  TEST(J5_046_ScalarStorage, FailsEveryAllocationAndAllowsRetry)
  {
    /* Arrange */
    for (const std::string input : {std::string{R"("abcdefghijklmnopq")"}, std::string{"false"}})
    {
      std::size_t attempts{};
      {
        lexer_provider provider;
        cursor source({input.data(), input.size()});
        auto actual = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 32);
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        attempts = provider.m_attempts;
      }
      ASSERT_GT(attempts, 0u);
      for (std::size_t failure = 1; failure <= attempts; ++failure)
      {
        lexer_provider provider;
        provider.m_fail_on = failure;
        cursor source({input.data(), input.size()});
        /* Act */
        auto actual = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 32);
        /* Expect */
        ASSERT_FALSE(actual.has_value());
        EXPECT_EQ(actual.error_if()->code, error_code::allocation_failure);
        EXPECT_TRUE(actual.error_if()->has_byte_offset);
        EXPECT_LT(actual.error_if()->byte_offset, input.size());
        EXPECT_EQ(source.offset(), 0u);
        EXPECT_EQ(provider.m_live_bytes, 0u);
        EXPECT_EQ(provider.m_allocations, provider.m_releases);
        provider.m_fail_on = 0;
        auto retry = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 32);
        ASSERT_TRUE(retry.has_value());
        ASSERT_TRUE(retry.value_if()->has_value());
        EXPECT_EQ(source.offset(), input.size());
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /* Enforce decoded storage limits and release content after later boundary failure.
   * Arrange: A literal, owned string and warning-bearing string with explicit limits.
   * Act: Recognize with insufficient limits or invalid following source.
   * Expect: Located errors, no partial cursor commit and no leaked owned storage.
   */
  TEST(J5_046_ScalarStorage, EnforcesLimitsAndReleasesBoundaryFailures)
  {
    /* Arrange */
    for (const std::string input : {std::string{"false"}, std::string{R"("abcde")"}})
    {
      lexer_provider provider;
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 4);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, input[0] == 'f' ? 4u : 5u);
      EXPECT_EQ(source.offset(), 0u);
      EXPECT_EQ(provider.m_live_bytes, 0u);
    }
    const std::string input = u8"\"a\u2028\"x";
    lexer_provider provider;
    cursor source({input.data(), input.size()});
    auto actual = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 8);
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, error_code::syntax);
    EXPECT_EQ(actual.error_if()->byte_offset, 6u);
    EXPECT_EQ(source.offset(), 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_GT(provider.m_allocations, 0u);
    cursor missing({"true", 4});
    auto no_provider = consume_scalar(missing, syntax_mode::json5, {}, 4);
    ASSERT_FALSE(no_provider.has_value());
    EXPECT_EQ(no_provider.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(missing.offset(), 0u);
  }
  /* ================================== End Test_J5_046 ================================== */
  /* ==================================== Test_J5_040 ==================================== */
  /* Move a warning-bearing string variant independently of caller input.
   * Arrange: Mutable source containing a raw line separator and a storage provider.
   * Act: Recognize, move the scalar, overwrite and destroy the source bytes.
   * Expect: Owned decoded units and original warning metadata survive; storage releases once.
   */
  TEST(J5_040_ScalarOwnership, PreservesMovedStringAndWarnings)
  {
    /* Arrange */
    lexer_provider provider;
    std::optional<scalar_token> retained;
    {
      std::string input = u8"\"a\u2028b\",";
      cursor source({input.data(), input.size()});
      /* Act */
      auto actual = consume_scalar(source, syntax_mode::json5, provider.callbacks(), 3);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      retained.emplace(std::move(**actual.value_if()));
      EXPECT_EQ(source.offset(), 7u);
      input.assign(input.size(), '?');
    }
    const auto* string = std::get_if<string_token>(&*retained);
    ASSERT_NE(string, nullptr);
    expect_units(string->value, u"a\u2028b");
    EXPECT_EQ(string->warning_count, 1u);
    ASSERT_TRUE(string->first_warning.has_value());
    EXPECT_EQ(string->byte_length, 7u);
    retained.reset();
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }
  /* ================================== End Test_J5_040 ================================== */
} // namespace
