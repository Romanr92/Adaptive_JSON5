#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <ostream>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;
  /* ==================================== Fixtures ===================================== */
  /**
   * @brief Name parameterized cases after their name field.
   * @par Requirements
   * Issue #96.
   */
  template <typename T>
  std::string case_name(const testing::TestParamInfo<T>& info)
  {
    return info.param.name;
  }
  /* ================================== End Fixtures =================================== */
  /* ==================================== Test_D96 ===================================== */
  // The syntax factories stay usable in constant expressions and fix the error code to syntax.
  constexpr error c_located = error::syntax_at(5U, syntax_error_reason::missing_colon);
  static_assert((c_located.code == error_code::syntax) && c_located.has_byte_offset && (c_located.byte_offset == 5U));
  static_assert(c_located.syntax_reason == syntax_error_reason::missing_colon);
  static_assert(!c_located.source_range.has_value());
  constexpr error c_ranged = error::syntax_at_range(4U, source_span{0U, 4U}, syntax_error_reason::unterminated_string);
  static_assert((c_ranged.code == error_code::syntax) && (c_ranged.byte_offset == 4U));
  static_assert(c_ranged.source_range.has_value() && (c_ranged.source_range->byte_length == 4U));
  static_assert(c_ranged.syntax_reason == syntax_error_reason::unterminated_string);
  // Every other factory leaves the reason empty.
  static_assert(!error::at_offset(error_code::syntax, 1U).syntax_reason.has_value());
  static_assert(!error::at_range(error_code::syntax, 1U, source_span{0U, 1U}).syntax_reason.has_value());
  static_assert(!error::without_location(error_code::allocation_failure).syntax_reason.has_value());
  static_assert(!error::duplicate_member(source_span{1U, 1U}, source_span{5U, 1U}).syntax_reason.has_value());
  static_assert(!error::numeric_failure(numeric_conversion_reason::out_of_range).syntax_reason.has_value());

  /**
   * @brief One rejected document and the reason its parse must report.
   * @par Requirements
   * Issue #96; J5-043; J5-044.
   */
  struct reason_case
  {
      const char* name;           // Case identifier, naming the reason and its trigger.
      std::string input;          // Complete input bytes.
      syntax_mode mode;           // Input syntax selection.
      std::size_t offset;         // Expected error byte offset.
      syntax_error_reason reason; // Expected syntax reason.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const reason_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class D96_SyntaxReason : public testing::TestWithParam<reason_case>
  {
  };
  /*
   * Verify every grammar failure reported through the public entry carries its specific reason.
   * Arrange: One minimal document per reason and trigger, in the mode that rejects it.
   * Act: Parse the complete document.
   * Expect: A syntax error at the exact offset with the expected reason, and no numeric metadata.
   */
  TEST_P(D96_SyntaxReason, ReportsSpecificReasonAtOffset)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    document_provider provider;
    const parse_options options{fixture.mode};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_document({fixture.input.data(), fixture.input.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    const error& failure = *parsed.error_if();
    EXPECT_EQ(failure.code, error_code::syntax);
    EXPECT_EQ(failure.byte_offset, fixture.offset);
    ASSERT_TRUE(failure.syntax_reason.has_value());
    EXPECT_EQ(*failure.syntax_reason, fixture.reason);
    EXPECT_FALSE(failure.numeric_reason.has_value());
  }
  constexpr syntax_mode c_json5 = syntax_mode::json5;
  constexpr syntax_mode c_strict = syntax_mode::strict_json;
  INSTANTIATE_TEST_SUITE_P(
    Reasons, D96_SyntaxReason,
    testing::Values(
      reason_case{"EndAfterComma", "[1,", c_json5, 3, syntax_error_reason::unexpected_end},
      reason_case{"EndAfterMinus", "-", c_json5, 1, syntax_error_reason::unexpected_end},
      reason_case{"EndInsideUnicodeEscape", "\"\\u12", c_json5, 5, syntax_error_reason::unexpected_end},
      reason_case{"EndAfterHexPrefix", "0x", c_json5, 2, syntax_error_reason::unexpected_end},
      reason_case{"CharacterAtRoot", "@", c_json5, 0, syntax_error_reason::unexpected_character},
      reason_case{"CharacterAfterNumber", "1x", c_json5, 1, syntax_error_reason::unexpected_character},
      reason_case{"CommaFirstInArray", "[,1]", c_json5, 1, syntax_error_reason::unexpected_comma},
      reason_case{"DoubledComma", "[1,,2]", c_json5, 3, syntax_error_reason::unexpected_comma},
      reason_case{"ColonInArray", "[1:2]", c_json5, 2, syntax_error_reason::unexpected_colon},
      reason_case{"BraceClosesArray", "[1}", c_json5, 2, syntax_error_reason::unexpected_closing_bracket},
      reason_case{"BraceOpensEmptyArray", "[}", c_json5, 1, syntax_error_reason::unexpected_closing_bracket},
      reason_case{"BracketClosesObject", "{\"a\":1]", c_json5, 6, syntax_error_reason::unexpected_closing_bracket},
      reason_case{"ValueWithoutColon", "{\"a\" 1}", c_json5, 5, syntax_error_reason::missing_colon},
      reason_case{"CloseWithoutColon", "{\"a\"}", c_json5, 4, syntax_error_reason::missing_colon},
      reason_case{"ArrayItemsWithoutComma", "[1 2]", c_json5, 3, syntax_error_reason::missing_comma},
      reason_case{"MembersWithoutComma", "{\"a\":1 \"b\":2}", c_json5, 7, syntax_error_reason::missing_comma},
      reason_case{"CloseAfterColon", "{\"a\":}", c_json5, 5, syntax_error_reason::missing_value},
      reason_case{"CommaAfterColon", "{\"a\":,}", c_json5, 5, syntax_error_reason::missing_value},
      reason_case{"NumericMemberName", "{1:2}", c_json5, 1, syntax_error_reason::expected_member_name},
      reason_case{"UnterminatedString", "\"abc", c_json5, 4, syntax_error_reason::unterminated_string},
      reason_case{"UnterminatedComment", "1/*x", c_json5, 4, syntax_error_reason::unterminated_comment},
      reason_case{"RawLineFeedInString", "\"a\nb\"", c_json5, 2, syntax_error_reason::line_terminator_in_string},
      reason_case{"StrictRawCarriageReturn", "\"a\rb\"", c_strict, 2, syntax_error_reason::line_terminator_in_string},
      reason_case{"StrictRawTab", "\"a\tb\"", c_strict, 2, syntax_error_reason::control_character_in_string},
      reason_case{"BadHexEscapeDigit", "\"\\x4G\"", c_json5, 4, syntax_error_reason::invalid_escape},
      reason_case{"BadUnicodeEscapeDigit", "\"\\u12G4\"", c_json5, 5, syntax_error_reason::invalid_escape},
      reason_case{"DigitEscape", "\"\\1\"", c_json5, 2, syntax_error_reason::invalid_escape},
      reason_case{"OctalZeroEscape", "\"\\01\"", c_json5, 3, syntax_error_reason::invalid_escape},
      reason_case{"LeadingZero", "01", c_json5, 1, syntax_error_reason::invalid_number},
      reason_case{"MissingExponentDigit", "1e+x", c_json5, 3, syntax_error_reason::invalid_number},
      reason_case{"BadHexDigit", "0xG", c_json5, 2, syntax_error_reason::invalid_number},
      reason_case{"NonUnicodeNameEscape", "{a\\x41:1}", c_json5, 2, syntax_error_reason::invalid_identifier},
      reason_case{"MisspelledNull", "nul", c_json5, 0, syntax_error_reason::unknown_literal},
      reason_case{"InfinitySuffix", "Infinityx", c_json5, 8, syntax_error_reason::unknown_literal},
      reason_case{"StrictUnquotedName", "{a:1}", c_strict, 1, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictEscapedName", "{\\u0061:1}", c_strict, 1, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictNumericName", "{1:2}", c_strict, 1, syntax_error_reason::expected_member_name},
      reason_case{"StrictArrayTrailingComma", "[1,]", c_strict, 3, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictObjectTrailingComma", "{\"a\":1,}", c_strict, 7,
                  syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictSingleQuote", "'a'", c_strict, 0, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictHexNumber", "0x1", c_strict, 0, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictPlusSign", "+1", c_strict, 0, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictInfinity", "Infinity", c_strict, 0, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictComment", "/*c*/1", c_strict, 0, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictVerticalTabEscape", "\"\\v\"", c_strict, 2, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"StrictLineContinuation", "\"a\\\nb\"", c_strict, 3, syntax_error_reason::not_allowed_in_strict_json},
      reason_case{"EmptyInput", "", c_json5, 0, syntax_error_reason::empty_input},
      reason_case{"WhitespaceOnly", "  ", c_json5, 2, syntax_error_reason::empty_input},
      reason_case{"CommentOnly", "/*c*/", c_json5, 5, syntax_error_reason::empty_input},
      reason_case{"SecondRootValue", "1 2", c_json5, 2, syntax_error_reason::trailing_content},
      reason_case{"SecondRootContainer", "{} {}", c_json5, 3, syntax_error_reason::trailing_content}),
    case_name<reason_case>);
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify unterminated tokens keep their range alongside the reason.
   * Arrange: An unterminated string and an unterminated block comment.
   * Act: Parse each document.
   * Expect: The reason, the end-of-input offset and a range starting at the opening delimiter.
   */
  TEST(D96_SyntaxReasonRange, KeepsRangeForUnterminatedTokens)
  {
    /* Arrange */
    const std::string string_input{"[\"abc"};
    const std::string comment_input{"1 /*x"};
    document_provider provider;
    const parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto string_parsed = parse_document({string_input.data(), string_input.size()}, options, accounting);
    const auto comment_parsed = parse_document({comment_input.data(), comment_input.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(string_parsed.has_value());
    EXPECT_EQ(string_parsed.error_if()->syntax_reason, syntax_error_reason::unterminated_string);
    EXPECT_EQ(string_parsed.error_if()->byte_offset, 5U);
    ASSERT_TRUE(string_parsed.error_if()->source_range.has_value());
    EXPECT_EQ(string_parsed.error_if()->source_range->byte_offset, 1U);
    EXPECT_EQ(string_parsed.error_if()->source_range->byte_length, 4U);
    ASSERT_FALSE(comment_parsed.has_value());
    EXPECT_EQ(comment_parsed.error_if()->syntax_reason, syntax_error_reason::unterminated_comment);
    EXPECT_EQ(comment_parsed.error_if()->byte_offset, 5U);
    ASSERT_TRUE(comment_parsed.error_if()->source_range.has_value());
    EXPECT_EQ(comment_parsed.error_if()->source_range->byte_offset, 2U);
    EXPECT_EQ(comment_parsed.error_if()->source_range->byte_length, 3U);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify non-syntax failures never carry a syntax reason.
   * Arrange: Malformed UTF-8 and a document larger than the input limit.
   * Act: Parse each document.
   * Expect: encoding and resource_limit codes with no syntax reason.
   */
  TEST(D96_SyntaxReasonScope, LeavesOtherCategoriesWithoutReason)
  {
    /* Arrange */
    const std::string bad_utf8{"\"\xC0\x80\""};
    const std::string large{"[1,2,3]"};
    document_provider provider;
    parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto encoding = parse_document({bad_utf8.data(), bad_utf8.size()}, options, accounting);
    options.limits.max_input_bytes = large.size() - 1U;
    const auto limited = parse_document({large.data(), large.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(encoding.has_value());
    EXPECT_EQ(encoding.error_if()->code, error_code::encoding);
    EXPECT_FALSE(encoding.error_if()->syntax_reason.has_value());
    ASSERT_FALSE(limited.has_value());
    EXPECT_EQ(limited.error_if()->code, error_code::resource_limit);
    EXPECT_FALSE(limited.error_if()->syntax_reason.has_value());
  }
  /* ================================== End Test_D96 =================================== */
} // namespace
