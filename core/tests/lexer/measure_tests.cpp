#include "../../../tests/shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "input/cursor.hpp"
#include "lexer/identifier/identifier_lexer.hpp"
#include "lexer/member_name_lexer.hpp"
#include "lexer/scalar_lexer.hpp"
#include "lexer/scalar_token.hpp"
#include "lexer/string/string_lexer.hpp"

#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <optional>
#include <ostream>
#include <variant>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  constexpr std::size_t c_max_string_units = 4;
  constexpr std::size_t c_max_identifier_units = 4;
  constexpr std::size_t c_max_exponent_digits = 3;

  /**
   * @brief Expect two failures to carry the same code, location, range and reason.
   * @par Requirements
   * P028; J5-043; J5-044; J5-047.
   */
  void expect_same_error(const error& measured, const error& consumed)
  {
    EXPECT_EQ(measured.code, consumed.code);
    EXPECT_EQ(measured.has_byte_offset, consumed.has_byte_offset);
    EXPECT_EQ(measured.byte_offset, consumed.byte_offset);
    EXPECT_EQ(measured.syntax_reason, consumed.syntax_reason);
    ASSERT_EQ(measured.source_range.has_value(), consumed.source_range.has_value());
    if (measured.source_range.has_value())
    {
      EXPECT_EQ(measured.source_range->byte_offset, consumed.source_range->byte_offset);
      EXPECT_EQ(measured.source_range->byte_length, consumed.source_range->byte_length);
    }
  }

  /**
   * @brief Describe one input given to a measuring function and its consuming counterpart.
   * @par Requirements
   * P028; P033; J5-046; J5-047.
   */
  struct measure_case
  {
      const char* name;
      const char* input; // NUL-terminated input bytes.
      syntax_mode mode;  // Selected syntax.
  };

  std::ostream& operator<<(std::ostream& stream, const measure_case& value)
  {
    return stream << value.name;
  }

  class MeasureScalarUnits : public ::testing::TestWithParam<measure_case>
  {
  };

  class MeasureMemberNameUnits : public ::testing::TestWithParam<measure_case>
  {
  };
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_047 =================================== */

  /* Verify measure_scalar reports exactly what consume_scalar reports, without a storage provider.
   * Arrange: The case input, a guarded provider for the consuming call and equal component limits.
   * Act: Measure and consume from separate cursors at offset zero.
   * Expect: Same outcome, error fields, cursor position, kind, span, decoded units and warning count.
   */
  TEST_P(MeasureScalarUnits, MatchesConsumeScalar)
  {
    /* Arrange */
    const measure_case& param = GetParam();
    const input_view input{param.input, std::strlen(param.input)};
    guard_provider provider;
    cursor measuring{input};
    cursor consuming{input};
    /* Act */
    const auto measured =
      measure_scalar(measuring, param.mode, c_max_string_units, c_max_identifier_units, c_max_exponent_digits);
    const auto consumed = consume_scalar(consuming, param.mode, provider.callbacks(), c_max_string_units,
                                         c_max_identifier_units, c_max_exponent_digits);
    /* Expect */
    ASSERT_EQ(measured.has_value(), consumed.has_value());
    EXPECT_EQ(measuring.offset(), consuming.offset());
    if (!measured.has_value())
    {
      expect_same_error(*measured.error_if(), *consumed.error_if());
      return;
    }
    ASSERT_EQ(measured.value_if()->has_value(), consumed.value_if()->has_value());
    if (!measured.value_if()->has_value())
    {
      return;
    }
    const scalar_measure& measure = **measured.value_if();
    const scalar_token& token = **consumed.value_if();
    ASSERT_EQ(measure.index(), token.index());
    if (const auto* string = std::get_if<string_measure>(&measure); string != nullptr)
    {
      const auto& expected = std::get<string_token>(token);
      EXPECT_EQ(string->units, expected.value.size());
      EXPECT_EQ(string->byte_offset, expected.byte_offset);
      EXPECT_EQ(string->byte_length, expected.byte_length);
      EXPECT_EQ(string->warning_count, expected.warning_count);
      EXPECT_EQ(string->first_warning.has_value(), expected.first_warning.has_value());
    }
    else if (const auto* number = std::get_if<number_token>(&measure); number != nullptr)
    {
      const auto& expected = std::get<number_token>(token);
      EXPECT_EQ(number->kind, expected.kind);
      EXPECT_EQ(number->byte_offset, expected.byte_offset);
      EXPECT_EQ(number->byte_length, expected.byte_length);
    }
    else
    {
      const auto& literal = std::get<literal_token>(measure);
      const auto& expected = std::get<literal_token>(token);
      EXPECT_EQ(literal.kind, expected.kind);
      EXPECT_EQ(literal.byte_offset, expected.byte_offset);
      EXPECT_EQ(literal.byte_length, expected.byte_length);
    }
  }

  // Limits: 4 string units, 4 identifier units, 3 exponent digits.
  INSTANTIATE_TEST_SUITE_P(
    J5_047_MeasureScalar, MeasureScalarUnits,
    ::testing::Values(measure_case{"EmptyInput", "", syntax_mode::json5},
                      measure_case{"StructuralComma", ",", syntax_mode::json5},
                      measure_case{"PlainString", "\"ab\",", syntax_mode::json5},
                      measure_case{"EscapedString", "'\\u00e9x'", syntax_mode::json5},
                      measure_case{"SurrogatePairString", "\"\\uD83D\\uDE00\"", syntax_mode::json5},
                      measure_case{"RawSeparatorWarning",
                                   "'a\xE2\x80\xA8"
                                   "b'",
                                   syntax_mode::json5},
                      measure_case{"StringAtUnitLimit", "\"abcd\"", syntax_mode::json5},
                      measure_case{"StringOverUnitLimit", "\"abcde\"", syntax_mode::json5},
                      measure_case{"UnterminatedString", "\"ab", syntax_mode::json5},
                      measure_case{"InvalidEscape", "\"\\x\"", syntax_mode::strict_json},
                      measure_case{"SingleQuoteStrict", "'a'", syntax_mode::strict_json},
                      measure_case{"Decimal", "12.5e3]", syntax_mode::json5},
                      measure_case{"Hexadecimal", "0x1F", syntax_mode::json5},
                      measure_case{"ExponentOverLimit", "1e1234", syntax_mode::json5},
                      measure_case{"Infinity", "-Infinity", syntax_mode::json5},
                      measure_case{"NumberBoundary", "1x", syntax_mode::json5},
                      measure_case{"TrueLiteral", "true}", syntax_mode::json5},
                      measure_case{"NullLiteralStrict", "null", syntax_mode::strict_json},
                      measure_case{"LiteralBoundary", "trueA", syntax_mode::json5},
                      measure_case{"UnknownWord", "foo]", syntax_mode::json5},
                      measure_case{"EscapedLiteralWord", "tru\\u0065", syntax_mode::json5},
                      measure_case{"UnknownWordOverIdentifierLimit", "abcdefg", syntax_mode::json5},
                      measure_case{"InvalidIdentifierEscape", "a\\x", syntax_mode::json5},
                      measure_case{"UnexpectedCharacter", "@", syntax_mode::json5}),
    [](const ::testing::TestParamInfo<measure_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify measure_member_name reports exactly what consume_member_name reports, without a storage provider.
   * Arrange: The case input, a guarded provider for the consuming call and an equal unit limit.
   * Act: Measure and consume from separate cursors at offset zero.
   * Expect: Same outcome, error fields, cursor position, span, decoded units and warning count.
   */
  TEST_P(MeasureMemberNameUnits, MatchesConsumeMemberName)
  {
    /* Arrange */
    const measure_case& param = GetParam();
    const input_view input{param.input, std::strlen(param.input)};
    guard_provider provider;
    cursor measuring{input};
    cursor consuming{input};
    /* Act */
    const auto measured = detail::measure_member_name(measuring, param.mode, c_max_string_units);
    const auto consumed = detail::consume_member_name(consuming, param.mode, provider.callbacks(), c_max_string_units);
    /* Expect */
    ASSERT_EQ(measured.has_value(), consumed.has_value());
    EXPECT_EQ(measuring.offset(), consuming.offset());
    if (!measured.has_value())
    {
      expect_same_error(*measured.error_if(), *consumed.error_if());
      return;
    }
    ASSERT_EQ(measured.value_if()->has_value(), consumed.value_if()->has_value());
    if (!measured.value_if()->has_value())
    {
      return;
    }
    const detail::member_name_measure& measure = **measured.value_if();
    const detail::member_name_token& token = **consumed.value_if();
    EXPECT_EQ(measure.units, token.value.size());
    EXPECT_EQ(measure.byte_offset, token.byte_offset);
    EXPECT_EQ(measure.byte_length, token.byte_length);
    EXPECT_EQ(measure.warning_count, token.warning_count);
  }

  // Limit: 4 decoded units per name.
  INSTANTIATE_TEST_SUITE_P(J5_047_MeasureMemberName, MeasureMemberNameUnits,
                           ::testing::Values(measure_case{"QuotedName", "\"ab\":", syntax_mode::json5},
                                             measure_case{"QuotedNameStrict", "\"ab\":", syntax_mode::strict_json},
                                             measure_case{"SingleQuotedWarning",
                                                          "'a\xE2\x80\xA8':", syntax_mode::json5},
                                             measure_case{"IdentifierName", "port:", syntax_mode::json5},
                                             measure_case{"EscapedIdentifierName", "p\\u006frt:", syntax_mode::json5},
                                             measure_case{"IdentifierOverLimit", "ports:", syntax_mode::json5},
                                             measure_case{"QuotedOverLimit", "\"abcde\":", syntax_mode::json5},
                                             measure_case{"IdentifierNameStrict", "port:", syntax_mode::strict_json},
                                             measure_case{"EscapeStartStrict", "\\u0061:", syntax_mode::strict_json},
                                             measure_case{"DigitIsNoName", "1:", syntax_mode::json5},
                                             measure_case{"DigitIsNoNameStrict", "1:", syntax_mode::strict_json},
                                             measure_case{"ColonIsNoName", ":", syntax_mode::json5},
                                             measure_case{"InvalidIdentifierEscape", "\\u0031:", syntax_mode::json5},
                                             measure_case{"UnterminatedName", "\"ab", syntax_mode::json5}),
                           [](const ::testing::TestParamInfo<measure_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify the string and identifier measurements carry the consuming tokens' metadata.
   * Arrange: A string with two raw separators and an escaped identifier.
   * Act: Measure and consume each.
   * Expect: Equal units, spans, warning counts, first-warning span and escape flag.
   */
  TEST(J5_047_MeasureLexers, CarryConsumingMetadata)
  {
    /* Arrange */
    const char text[] = "\"a\xE2\x80\xA8\xE2\x80\xA9\"";
    const char word[] = "a\\u0062c:";
    const input_view string_input{text, sizeof(text) - 1U};
    const input_view word_input{word, sizeof(word) - 1U};
    guard_provider provider;
    cursor string_measuring{string_input};
    cursor string_consuming{string_input};
    cursor word_measuring{word_input};
    cursor word_consuming{word_input};
    /* Act */
    const auto measured_string = measure_string(string_measuring, syntax_mode::json5, 8U);
    const auto consumed_string = consume_string(string_consuming, syntax_mode::json5, provider.callbacks(), 8U);
    const auto measured_word = measure_identifier(word_measuring, 8U);
    const auto consumed_word = consume_identifier(word_consuming, provider.callbacks(), 8U);
    /* Expect */
    ASSERT_TRUE(measured_string.has_value() && measured_string.value_if()->has_value());
    ASSERT_TRUE(consumed_string.has_value() && consumed_string.value_if()->has_value());
    const string_measure& string = **measured_string.value_if();
    const string_token& token = **consumed_string.value_if();
    EXPECT_EQ(string.units, 3U);
    EXPECT_EQ(string.units, token.value.size());
    EXPECT_EQ(string.byte_length, token.byte_length);
    EXPECT_EQ(string.warning_count, 2U);
    EXPECT_EQ(string.warning_count, token.warning_count);
    ASSERT_TRUE(string.first_warning.has_value() && token.first_warning.has_value());
    EXPECT_EQ(string.first_warning->byte_offset, token.first_warning->byte_offset);
    EXPECT_EQ(string_measuring.offset(), string_consuming.offset());
    ASSERT_TRUE(measured_word.has_value() && measured_word.value_if()->has_value());
    ASSERT_TRUE(consumed_word.has_value() && consumed_word.value_if()->has_value());
    const identifier_measure& identifier = **measured_word.value_if();
    EXPECT_EQ(identifier.units, 3U);
    EXPECT_EQ(identifier.units, (**consumed_word.value_if()).value.size());
    EXPECT_EQ(identifier.byte_length, (**consumed_word.value_if()).byte_length);
    EXPECT_TRUE(identifier.contains_escape);
    EXPECT_EQ(word_measuring.offset(), 8U);
    EXPECT_EQ(word_measuring.offset(), word_consuming.offset());
  }
  /* ================================= End Test_J5_047 ================================= */
} // namespace
