#include "adaptive_json5/detail/lexer/string/line_continuation.hpp"
#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"
#include "adaptive_json5/detail/memory/decoded_string_builder.hpp"
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
   * @brief Supply host-only storage for visible component composition tests.
   * @par Requirements
   * P019; J5-017.
   */
  void* allocate_test_storage(void*, std::size_t bytes, std::size_t alignment) noexcept
  {
    return alignment <= alignof(std::max_align_t) ? std::malloc(bytes) : nullptr;
  }

  /**
   * @brief Release the allocation through its original test provider.
   * @par Requirements
   * P019; J5-017.
   */
  void release_test_storage(void*, void* address, std::size_t, std::size_t) noexcept
  {
    std::free(address);
  }

  constexpr storage_callbacks c_test_storage{nullptr, &allocate_test_storage, &release_test_storage};

  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_017 =================================== */

  static_assert(noexcept(consume_line_continuation(std::declval<cursor&>(), syntax_mode::json5)));

  /**
   * @brief Continuation spellings and independent source-span expectations.
   * @par Requirements
   * P019; J5-017.
   */
  struct continuation_case
  {
      const char* name;   // Diagnostic parameter name.
      std::string bytes;  // Input line-terminator sequence.
      std::size_t length; // Expected consumed bytes, excluding backslash.
  };
  class J5_017_Continuation : public ::testing::TestWithParam<continuation_case>
  {
  };

  /* Verify all five sequences are consumed completely only in JSON5 mode.
   * Arrange: Put each sequence at a nonzero offset after backslash with a suffix.
   * Act: Recognize it in both syntax modes and in an exact-length JSON5 view.
   * Expect: Exact JSON5 span or strict syntax with no advancement; suffix unread.
   */
  TEST_P(J5_017_Continuation, ConsumesExactSequenceOrRejectsStrictMode)
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
      const auto actual = consume_line_continuation(source, mode);
      /* Expect */
      if (mode == syntax_mode::json5)
      {
        ASSERT_TRUE(actual.has_value());
        ASSERT_TRUE(actual.value_if()->has_value());
        EXPECT_EQ((**actual.value_if()).byte_offset, 2u);
        EXPECT_EQ((**actual.value_if()).byte_length, param.length);
        EXPECT_EQ(source.offset(), 2u + param.length);
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
    cursor exact({input.data(), 2 + param.length});
    exact.advance(2);
    const auto bounded = consume_line_continuation(exact, syntax_mode::json5);
    ASSERT_TRUE(bounded.has_value());
    ASSERT_TRUE(bounded.value_if()->has_value());
    EXPECT_EQ((**bounded.value_if()).byte_length, param.length);
    EXPECT_TRUE(exact.is_at_end());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify continuation removal preserves surrounding content and indentation.
   * Arrange: Use a quoted A, backslash/terminator, space and B with owned storage.
   * Act: Call existing boundary, continuation and builder APIs explicitly.
   * Expect: Exact A-space-B content with no terminator or backslash units.
   * This fixed call sequence is not a full string-parser implementation.
   */
  TEST_P(J5_017_Continuation, JoinsContentAndPreservesFollowingSpace)
  {
    /* Arrange */
    const auto& param = GetParam();
    const std::string input = std::string("\"A\\") + param.bytes + " B\"";
    cursor source({input.data(), input.size()});
    decoded_string_builder builder(c_test_storage, 3);
    const auto opening = consume_string_opening(source, syntax_mode::json5);
    ASSERT_TRUE(opening.has_value());
    ASSERT_TRUE(opening.value_if()->has_value());
    /* Act */
    const auto first = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(first.value_if()->kind, string_piece_kind::character);
    ASSERT_TRUE(builder.append_scalar(first.value_if()->scalar).has_value());
    const auto handoff = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    ASSERT_TRUE(handoff.has_value());
    ASSERT_EQ(handoff.value_if()->kind, string_piece_kind::escape_start);
    const auto continuation = consume_line_continuation(source, syntax_mode::json5);
    ASSERT_TRUE(continuation.has_value());
    ASSERT_TRUE(continuation.value_if()->has_value());
    EXPECT_EQ(builder.size(), 1u);
    const auto space = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    ASSERT_TRUE(space.has_value());
    ASSERT_EQ(space.value_if()->kind, string_piece_kind::character);
    ASSERT_TRUE(builder.append_scalar(space.value_if()->scalar).has_value());
    const auto last = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    ASSERT_TRUE(last.has_value());
    ASSERT_EQ(last.value_if()->kind, string_piece_kind::character);
    ASSERT_TRUE(builder.append_scalar(last.value_if()->scalar).has_value());
    const auto closing = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    auto text = builder.finish();
    std::array<unsigned char, 4> output{0xA5, 0xA5, 0xA5, 0xA5};
    const auto exported = export_utf8(text, output.data(), output.size());
    /* Expect */
    EXPECT_EQ((**continuation.value_if()).byte_offset, 3u);
    EXPECT_EQ((**continuation.value_if()).byte_length, param.length);
    ASSERT_TRUE(closing.has_value());
    EXPECT_EQ(closing.value_if()->kind, string_piece_kind::closing_quote);
    EXPECT_TRUE(source.is_at_end());
    EXPECT_EQ(text.size(), 3u);
    ASSERT_TRUE(exported.has_value());
    EXPECT_EQ(*exported.value_if(), 3u);
    EXPECT_EQ(output, (std::array<unsigned char, 4>{0x41, 0x20, 0x42, 0xA5}));
  }
  INSTANTIATE_TEST_SUITE_P(Sequences, J5_017_Continuation,
                           ::testing::Values(continuation_case{"Lf", "\n", 1}, continuation_case{"Cr", "\r", 1},
                                             continuation_case{"CrLf", "\r\n", 2},
                                             continuation_case{"LineSeparator", "\xE2\x80\xA8", 3},
                                             continuation_case{"ParagraphSeparator", "\xE2\x80\xA9", 3}),
                           [](const ::testing::TestParamInfo<continuation_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify CR lookahead is bounded and consumes only an immediately adjacent LF.
   * Arrange: Restrict CRLF to a CR-only view, and separate CR/LF with a space.
   * Act: Recognize continuations in each view.
   * Expect: One-byte CR spans and no consumption of hidden or separated suffixes.
   */
  TEST(J5_017_ContinuationBoundary, BoundsCrLookahead)
  {
    /* Arrange */
    const std::array<char, 4> input{'\\', '\r', '\n', 'Z'};
    const std::array<char, 4> separated{'\\', '\r', ' ', '\n'};
    for (const auto view : {input_view{input.data(), 2}, input_view{separated.data(), separated.size()}})
    {
      cursor source(view);
      source.advance(1);
      /* Act */
      const auto actual = consume_line_continuation(source, syntax_mode::json5);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ((**actual.value_if()).byte_offset, 1u);
      EXPECT_EQ((**actual.value_if()).byte_length, 1u);
      EXPECT_EQ(source.offset(), 2u);
      EXPECT_EQ(source.remaining(), view.size - 2);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify recognition consumes only one sequence rather than a run of lines.
   * Arrange: Place an additional terminator immediately after a complete sequence.
   * Act: Consume one continuation.
   * Expect: The complete first sequence only, with the extra raw terminator unread.
   */
  TEST(J5_017_ContinuationBoundary, LeavesAdditionalTerminatorsUnread)
  {
    /* Arrange */
    for (const std::string first : {std::string("\n"), std::string("\r\n"), std::string("\xE2\x80\xA8")})
    {
      const std::string input = std::string("\\") + first + "\n";
      cursor source({input.data(), input.size()});
      source.advance(1);
      /* Act */
      const auto actual = consume_line_continuation(source, syntax_mode::json5);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ((**actual.value_if()).byte_length, first.size());
      EXPECT_EQ(source.offset(), 1u + first.size());
      EXPECT_EQ(*source.peek(), '\n');
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify all nonterminating ASCII characters remain available to other helpers.
   * Arrange: Select every ASCII byte except LF/CR in both syntax modes.
   * Act: Attempt continuation recognition after backslash.
   * Expect: Successful no-match without advancement, including NUL, VT and FF.
   */
  TEST(J5_017_ContinuationBoundary, LeavesNonterminatorsUnread)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (unsigned int byte = 0; byte < 128; ++byte)
      {
        if (byte == 0x0A || byte == 0x0D)
        {
          continue;
        }
        const std::array<char, 2> input{'\\', static_cast<char>(byte)};
        cursor source({input.data(), input.size()});
        source.advance(1);
        /* Act */
        const auto actual = consume_line_continuation(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value()) << byte;
        EXPECT_FALSE(actual.value_if()->has_value()) << byte;
        EXPECT_EQ(source.offset(), 1u);
      }
      for (const std::string input : {std::string("\xC2\xA0"), std::string("\xF0\x9F\x8E\xBC")})
      {
        cursor source({input.data(), input.size()});
        const auto actual = consume_line_continuation(source, mode);
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 0u);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  class J5_017_ContinuationEncoding : public ::testing::TestWithParam<std::string>
  {
  };

  /* Verify malformed source cannot be consumed as a continuation.
   * Arrange: Put malformed UTF-8 after a backslash at a nonzero offset.
   * Act: Recognize under both syntax modes.
   * Expect: Encoding at the sequence start without changing the cursor.
   */
  TEST_P(J5_017_ContinuationEncoding, ReportsEncodingWithoutAdvancing)
  {
    /* Arrange */
    const std::string input = std::string("X\\") + GetParam();
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      source.advance(2);
      /* Act */
      const auto actual = consume_line_continuation(source, mode);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::encoding);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 2u);
      EXPECT_EQ(source.offset(), 2u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(Malformed, J5_017_ContinuationEncoding,
                           ::testing::Values(std::string("\x80"), std::string("\xC0\xAF"), std::string("\xED\xA0\x80"),
                                             std::string("\xF4\x90\x80\x80"), std::string("\xE2\x28\xA8"),
                                             std::string("\xFF")));

  /* ----------------------------------------------------------------------------------- */

  /* Verify explicit input bounds distinguish EOF from a truncated Unicode separator.
   * Arrange: Hide bytes beyond each proper prefix of U+2028/U+2029 after backslash.
   * Act: Recognize each prefix in both modes, including default empty input.
   * Expect: EOF syntax or encoding at the exact start with no advancement.
   */
  TEST(J5_017_ContinuationBoundary, RejectsEofAndTruncatedSeparatorPrefixes)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const std::string separator : {std::string("\xE2\x80\xA8"), std::string("\xE2\x80\xA9")})
      {
        const std::string input = std::string("X\\") + separator;
        for (std::size_t length = 0; length < separator.size(); ++length)
        {
          cursor source({input.data(), 2 + length});
          source.advance(2);
          /* Act */
          const auto actual = consume_line_continuation(source, mode);
          /* Expect */
          ASSERT_FALSE(actual.has_value());
          EXPECT_EQ(actual.error_if()->code, length == 0 ? error_code::syntax : error_code::encoding);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, 2u);
          EXPECT_EQ(source.offset(), 2u);
        }
      }
      cursor empty;
      const auto actual = consume_line_continuation(empty, mode);
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 0u);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a complete CR continuation does not validate unrelated trailing bytes.
   * Arrange: Follow CR with malformed UTF-8 instead of LF.
   * Act: Consume the complete CR sequence in JSON5.
   * Expect: One-byte success leaving malformed source for its next consumer.
   */
  TEST(J5_017_ContinuationBoundary, LeavesMalformedSuffixAfterCompleteCr)
  {
    /* Arrange */
    const std::array<char, 3> input{'\\', '\r', static_cast<char>(0x80)};
    cursor source({input.data(), input.size()});
    source.advance(1);
    /* Act */
    const auto actual = consume_line_continuation(source, syntax_mode::json5);
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_TRUE(actual.value_if()->has_value());
    EXPECT_EQ((**actual.value_if()).byte_length, 1u);
    EXPECT_EQ(source.offset(), 2u);
    EXPECT_EQ(*source.peek(), input[2]);
  }

  /* ================================= End Test_J5_017 ================================= */
} // namespace
