#include "adaptive_json5/detail/lexer/string/string_boundary.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /* =================================== Test_J5_014 =================================== */

  static_assert(noexcept(consume_string_opening(std::declval<cursor&>(), syntax_mode::json5)));
  static_assert(noexcept(consume_string_piece(std::declval<cursor&>(), string_quote::single, syntax_mode::json5)));

  /**
   * @brief Opening-delimiter inputs and independent mode expectations.
   * @par Requirements
   * P014; J5-014.
   */
  struct opening_case
  {
      const char* name;   // Diagnostic parameter name.
      char delimiter;     // Input quote byte.
      syntax_mode mode;   // Input syntax selection.
      bool accepted;      // Expected opening recognition versus syntax error.
      string_quote quote; // Expected delimiter when accepted.
  };

  class J5_014_Opening : public ::testing::TestWithParam<opening_case>
  {
  };

  /* Verify opening quotes follow the selected mode and preserve suffixes.
   * Arrange: Place a quote at a nonzero source offset with an unread suffix.
   * Act: Recognize the opening delimiter.
   * Expect: One-byte success or located strict-mode failure without advancement.
   */
  TEST_P(J5_014_Opening, RecognizesOrRejectsAtOriginalOffset)
  {
    /* Arrange */
    const auto& param = GetParam();
    const std::array<char, 3> input{'X', param.delimiter, 'Z'};
    cursor source({input.data(), input.size()});
    source.advance(1);
    /* Act */
    const auto actual = consume_string_opening(source, param.mode);
    /* Expect */
    ASSERT_EQ(actual.has_value(), param.accepted);
    if (param.accepted)
    {
      ASSERT_TRUE(actual.value_if()->has_value());
      EXPECT_EQ(**actual.value_if(), param.quote);
      EXPECT_EQ(source.offset(), 2u);
      ASSERT_TRUE(source.peek().has_value());
      EXPECT_EQ(*source.peek(), 'Z');
    }
    else
    {
      ASSERT_NE(actual.error_if(), nullptr);
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 1u);
      EXPECT_EQ(source.offset(), 1u);
    }
    EXPECT_EQ(input[1], param.delimiter);
  }

  INSTANTIATE_TEST_SUITE_P(
    Modes, J5_014_Opening,
    ::testing::Values(opening_case{"Json5Single", '\'', syntax_mode::json5, true, string_quote::single},
                      opening_case{"Json5Double", '"', syntax_mode::json5, true, string_quote::double_quote},
                      opening_case{"StrictDouble", '"', syntax_mode::strict_json, true, string_quote::double_quote},
                      opening_case{"StrictSingle", '\'', syntax_mode::strict_json, false, string_quote::single}),
    [](const ::testing::TestParamInfo<opening_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify the opener leaves all nonquote bytes and empty views untouched.
   * Arrange: Select every byte other than quotes in both modes and empty views.
   * Act: Attempt opening recognition.
   * Expect: Successful no-match without skipping trivia or validating other tokens.
   */
  TEST(J5_014_OpeningBoundary, LeavesNonquotesAndEmptyViewsUnread)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (unsigned int byte = 0; byte <= 255; ++byte)
      {
        if (byte == '\'' || byte == '"')
        {
          continue;
        }
        const char input = static_cast<char>(byte);
        cursor source({&input, 1});
        /* Act */
        const auto actual = consume_string_opening(source, mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value()) << byte;
        EXPECT_FALSE(actual.value_if()->has_value()) << byte;
        EXPECT_EQ(source.offset(), 0u) << byte;
      }
      const char hidden_quote = '"';
      for (const auto view : {input_view{}, input_view{&hidden_quote, 0}})
      {
        cursor source(view);
        const auto actual = consume_string_opening(source, mode);
        ASSERT_TRUE(actual.has_value());
        EXPECT_FALSE(actual.value_if()->has_value());
        EXPECT_EQ(source.offset(), 0u);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Source character spellings and independent decoded scalar expectations.
   * @par Requirements
   * P014; J5-014.
   */
  struct character_case
  {
      const char* name;  // Diagnostic parameter name.
      std::string bytes; // Input UTF-8 bytes for one source character.
      char32_t scalar;   // Expected Unicode scalar.
  };

  class J5_014_Character : public ::testing::TestWithParam<character_case>
  {
  };

  /* Verify ordinary characters preserve original byte spans and input.
   * Arrange: Place a literal UTF-8 character between an opening quote and suffix.
   * Act: Read the character in each valid mode and delimiter combination.
   * Expect: Exact scalar and byte span with the suffix unread and input unchanged.
   */
  TEST_P(J5_014_Character, PreservesScalarSpanAndSuffix)
  {
    /* Arrange */
    const auto& param = GetParam();
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const char quote : {'"', '\''})
      {
        if (mode == syntax_mode::strict_json && quote == '\'')
        {
          continue;
        }
        const std::string input = std::string("X") + quote + param.bytes + "Z";
        const auto before = input;
        cursor source({input.data(), input.size()});
        source.advance(1);
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        ASSERT_TRUE(opening.value_if()->has_value());
        /* Act */
        const auto actual = consume_string_piece(source, **opening.value_if(), mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_EQ(actual.value_if()->kind, string_piece_kind::character);
        EXPECT_EQ(actual.value_if()->scalar, param.scalar);
        EXPECT_EQ(actual.value_if()->byte_offset, 2u);
        EXPECT_EQ(actual.value_if()->byte_length, param.bytes.size());
        EXPECT_EQ(source.offset(), 2u + param.bytes.size());
        ASSERT_TRUE(source.peek().has_value());
        EXPECT_EQ(*source.peek(), 'Z');
        EXPECT_EQ(input, before);
      }
    }
  }
  INSTANTIATE_TEST_SUITE_P(Scalars, J5_014_Character,
                           ::testing::Values(character_case{"Space", " ", U' '}, character_case{"Ascii", "A", U'A'},
                                             character_case{"Delete", std::string(1, '\x7F'), 0x7F},
                                             character_case{"TwoBytes", "\xC2\xA0", 0xA0},
                                             character_case{"ThreeBytes", "\xE2\x82\xAC", 0x20AC},
                                             character_case{"Supplementary", "\xF0\x9F\x8E\xBC", 0x1F3BC},
                                             character_case{"LineSeparator", "\xE2\x80\xA8", 0x2028},
                                             character_case{"ParagraphSeparator", "\xE2\x80\xA9", 0x2029}),
                           [](const ::testing::TestParamInfo<character_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify every raw ASCII control follows the selected grammar.
   * Arrange: Place each U+0000-U+001F after a valid opener in both modes.
   * Act: Read a piece with an explicit length including embedded zero.
   * Expect: JSON5 admits controls except CR/LF; strict JSON rejects all atomically.
   */
  TEST(J5_014_Controls, DistinguishesAllRawControlsByMode)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (unsigned int byte = 0; byte <= 0x1F; ++byte)
      {
        const std::array<char, 3> input{'"', static_cast<char>(byte), 'Z'};
        cursor source({input.data(), input.size()});
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        /* Act */
        const auto actual = consume_string_piece(source, **opening.value_if(), mode);
        /* Expect */
        const bool accepted = mode == syntax_mode::json5 && byte != 0x0A && byte != 0x0D;
        ASSERT_EQ(actual.has_value(), accepted) << byte;
        if (accepted)
        {
          EXPECT_EQ(actual.value_if()->kind, string_piece_kind::character);
          EXPECT_EQ(actual.value_if()->scalar, static_cast<char32_t>(byte));
          EXPECT_EQ(actual.value_if()->byte_offset, 1u);
          EXPECT_EQ(actual.value_if()->byte_length, 1u);
          EXPECT_EQ(source.offset(), 2u);
        }
        else
        {
          EXPECT_EQ(actual.error_if()->code, error_code::syntax);
          EXPECT_TRUE(actual.error_if()->has_byte_offset);
          EXPECT_EQ(actual.error_if()->byte_offset, 1u);
          EXPECT_EQ(source.offset(), 1u);
        }
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify matching quotes close, opposite quotes remain content, and empty strings close.
   * Arrange: Select each permitted delimiter and place the opposite quote inside it.
   * Act: Consume opener, opposite quote, closer and an adjacent empty string.
   * Expect: Correct piece kinds, one-byte spans and unread following token.
   */
  TEST(J5_014_Delimiters, MatchesOnlyOpeningQuoteAndHandlesEmptyStrings)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const char quote : {'"', '\''})
      {
        if (mode == syntax_mode::strict_json && quote == '\'')
        {
          continue;
        }
        const char opposite = quote == '"' ? '\'' : '"';
        const std::array<char, 6> input{quote, opposite, quote, quote, quote, 'Z'};
        cursor source({input.data(), input.size()});
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        /* Act */
        const auto content = consume_string_piece(source, **opening.value_if(), mode);
        const auto close = consume_string_piece(source, **opening.value_if(), mode);
        const auto empty_open = consume_string_opening(source, mode);
        ASSERT_TRUE(empty_open.has_value());
        ASSERT_TRUE(empty_open.value_if()->has_value());
        const auto empty_close = consume_string_piece(source, **empty_open.value_if(), mode);
        /* Expect */
        ASSERT_TRUE(content.has_value());
        EXPECT_EQ(content.value_if()->kind, string_piece_kind::character);
        EXPECT_EQ(content.value_if()->scalar, static_cast<char32_t>(opposite));
        ASSERT_TRUE(close.has_value());
        EXPECT_EQ(close.value_if()->kind, string_piece_kind::closing_quote);
        EXPECT_EQ(close.value_if()->byte_offset, 2u);
        EXPECT_EQ(close.value_if()->byte_length, 1u);
        ASSERT_TRUE(empty_close.has_value());
        EXPECT_EQ(empty_close.value_if()->kind, string_piece_kind::closing_quote);
        EXPECT_EQ(empty_close.value_if()->byte_offset, 4u);
        EXPECT_EQ(source.offset(), 5u);
        EXPECT_EQ(*source.peek(), 'Z');
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify backslash handoff does not consume or decode the following escape byte.
   * Arrange: Place backslash before a quote, letter or line terminator.
   * Act: Consume only the escape introducer in either mode.
   * Expect: escape_start and its exact span; the next byte remains unread.
   */
  TEST(J5_014_EscapeBoundary, HandsOffWithoutDecodingEscape)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const char next : {'"', '\'', 'n', '\n', '\r'})
      {
        const std::array<char, 3> input{'"', '\\', next};
        cursor source({input.data(), input.size()});
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        /* Act */
        const auto actual = consume_string_piece(source, **opening.value_if(), mode);
        /* Expect */
        ASSERT_TRUE(actual.has_value());
        EXPECT_EQ(actual.value_if()->kind, string_piece_kind::escape_start);
        EXPECT_EQ(actual.value_if()->scalar, U'\\');
        EXPECT_EQ(actual.value_if()->byte_offset, 1u);
        EXPECT_EQ(actual.value_if()->byte_length, 1u);
        EXPECT_EQ(source.offset(), 2u);
        EXPECT_EQ(*source.peek(), next);
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  class J5_014_Encoding : public ::testing::TestWithParam<std::string>
  {
  };

  /* Verify malformed UTF-8 inside a string reports the original byte offset.
   * Arrange: Place a malformed sequence after opener and an ordinary character.
   * Act: Read the valid character then attempt the malformed one in both modes.
   * Expect: Encoding error at the sequence start and unchanged failing position.
   */
  TEST_P(J5_014_Encoding, RejectsMalformedInputWithoutAdvancing)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      const std::string input = std::string("X\"A") + GetParam();
      cursor source({input.data(), input.size()});
      source.advance(1);
      const auto opening = consume_string_opening(source, mode);
      ASSERT_TRUE(opening.has_value());
      ASSERT_TRUE(consume_string_piece(source, **opening.value_if(), mode).has_value());
      /* Act */
      const auto actual = consume_string_piece(source, **opening.value_if(), mode);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::encoding);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 3u);
      EXPECT_EQ(source.offset(), 3u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(Malformed, J5_014_Encoding,
                           ::testing::Values(std::string("\x80"), std::string("\xC0\xAF"), std::string("\xED\xA0\x80"),
                                             std::string("\xF4\x90\x80\x80"), std::string("\xE2\x28\xA1"),
                                             std::string("\xC2"), std::string("\xE2\x82"),
                                             std::string("\xF0\x9F\x8E")));

  /* ----------------------------------------------------------------------------------- */

  /* Verify EOF and every truncation of ordinary string contents stay bounded.
   * Arrange: Restrict an otherwise closed UTF-8 string to each shorter prefix.
   * Act: Read only boundary pieces until a missing/truncated character is reported.
   * Expect: EOF syntax or truncated UTF-8 errors at exact offsets without rollback
   * of earlier successful pieces; the hidden closing quote cannot be consumed.
   */
  TEST(J5_014_Truncation, RespectsEveryExplicitLengthPrefix)
  {
    /* Arrange */
    const std::string input = "\"A\xF0\x9F\x8E\xBC\"";
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (std::size_t length = 1; length < input.size(); ++length)
      {
        cursor source({input.data(), length});
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        /* Act */
        while (source.offset() < length)
        {
          const auto before = source.offset();
          const auto piece = consume_string_piece(source, **opening.value_if(), mode);
          if (!piece.has_value())
          {
            /* Expect */
            EXPECT_EQ(piece.error_if()->code, error_code::encoding);
            EXPECT_TRUE(piece.error_if()->has_byte_offset);
            EXPECT_EQ(piece.error_if()->byte_offset, 2u);
            EXPECT_EQ(source.offset(), before);
            break;
          }
          EXPECT_EQ(piece.value_if()->kind, string_piece_kind::character);
        }
        if (source.offset() == length)
        {
          const auto eof = consume_string_piece(source, **opening.value_if(), mode);
          ASSERT_FALSE(eof.has_value());
          EXPECT_EQ(eof.error_if()->code, error_code::syntax);
          EXPECT_TRUE(eof.error_if()->has_byte_offset);
          EXPECT_EQ(eof.error_if()->byte_offset, length);
          EXPECT_EQ(source.offset(), length);
        }
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify CRLF rejection consumes neither byte.
   * Arrange: Open a string immediately followed by a raw CRLF sequence.
   * Act: Attempt one content piece in each syntax mode.
   * Expect: Syntax at CR with both bytes still available to the caller.
   */
  TEST(J5_014_Controls, RejectsCrLfWithoutConsumingEitherByte)
  {
    /* Arrange */
    const std::array<char, 4> input{'"', '\r', '\n', '"'};
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      cursor source({input.data(), input.size()});
      const auto opening = consume_string_opening(source, mode);
      ASSERT_TRUE(opening.has_value());
      /* Act */
      const auto actual = consume_string_piece(source, **opening.value_if(), mode);
      /* Expect */
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_EQ(actual.error_if()->byte_offset, 1u);
      EXPECT_EQ(source.offset(), 1u);
      EXPECT_EQ(*source.peek(), '\r');
      EXPECT_EQ(*source.peek(1), '\n');
    }
  }

  /* ================================= End Test_J5_014 ================================= */
} // namespace
