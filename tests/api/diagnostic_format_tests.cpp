#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "adaptive_json5/text_position.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <clocale>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <optional>
#include <ostream>
#include <string>

namespace
{
  /**
   * @brief Count global allocations while a formatting call is observed.
   * @par Requirements
   * Issue #96; J5-045.
   * @note Only this test executable replaces the global allocation functions.
   */
  struct allocation_probe
  {
      bool m_counting{};
      std::size_t m_count{};
  };

  allocation_probe g_probe{};
} // namespace

// Replace the global allocator for this executable so formatting calls can be shown to allocate nothing.
void* operator new(std::size_t size)
{
  if (g_probe.m_counting)
  {
    ++g_probe.m_count;
  }

  void* memory = std::malloc((size == 0U) ? 1U : size);

  // Host test executables may stop on exhaustion; the library itself never reaches this allocator.
  if (memory == nullptr)
  {
    std::abort();
  }

  return memory;
}

void operator delete(void* memory) noexcept
{
  std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
  std::free(memory);
}

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;
  /* ==================================== Fixtures ===================================== */
  constexpr std::size_t c_buffer_size = 160U;

  /**
   * @brief Format into a large buffer and return the text, failing the test on an error.
   * @par Requirements
   * Issue #96.
   */
  std::string formatted(const result<std::size_t>& length, const char* buffer)
  {
    EXPECT_TRUE(length.has_value());
    if (!length.has_value())
    {
      return {};
    }
    EXPECT_EQ(*length.value_if(), std::strlen(buffer));
    return std::string{buffer};
  }

  /**
   * @brief Format an error with fixed caller positions.
   * @par Requirements
   * Issue #96.
   */
  std::string format_at(const error& failure, text_position primary, std::optional<text_position> first = std::nullopt)
  {
    char buffer[c_buffer_size]{};
    return formatted(format_error_at(failure, primary, first, buffer, sizeof(buffer)), buffer);
  }

  /**
   * @brief Format an error located in input.
   * @par Requirements
   * Issue #96.
   */
  std::string format_in(const error& failure, const std::string& input)
  {
    char buffer[c_buffer_size]{};
    return formatted(format_error(failure, {input.data(), input.size()}, buffer, sizeof(buffer)), buffer);
  }

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

  constexpr text_position c_position{2U, 5U};
  /* ================================== End Fixtures =================================== */
  /* ==================================== Test_D96 ===================================== */
  /**
   * @brief One error and its exact formatted text at line 2, column 5.
   * @par Requirements
   * Issue #96; J5-043.
   */
  struct message_case
  {
      const char* name;     // Case identifier.
      error failure;        // Input error.
      const char* expected; // Expected complete message.
  };
  // Print the case name in failure output instead of the error fields.
  void PrintTo(const message_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class D96_FormatMessage : public testing::TestWithParam<message_case>
  {
  };
  /*
   * Verify every error code, syntax reason and numeric reason has its fixed text.
   * Arrange: One error per category or reason; located errors sit at line 2, column 5.
   * Act: Format through format_error_at.
   * Expect: The exact message; unlocated errors print no position.
   */
  TEST_P(D96_FormatMessage, ProducesFixedText)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    /* Act */
    const std::string text = format_at(fixture.failure, c_position);
    /* Expect */
    EXPECT_EQ(text, fixture.expected);
  }
  constexpr std::size_t c_at = 9U;
  INSTANTIATE_TEST_SUITE_P(
    ErrorCodes, D96_FormatMessage,
    testing::Values(
      message_case{"SyntaxWithoutReason", error::at_offset(error_code::syntax, c_at),
                   "line: 2, column: 5: syntax error"},
      message_case{"Encoding", error::at_offset(error_code::encoding, c_at), "line: 2, column: 5: invalid UTF-8"},
      message_case{"DuplicateWithoutFirstPosition", error::at_offset(error_code::duplicate_policy, c_at),
                   "line: 2, column: 5: duplicate member name"},
      message_case{"WrongTypeOrAccess", error::without_location(error_code::wrong_type_or_access),
                   "wrong type or invalid access"},
      message_case{"NumericWithoutReason", error::without_location(error_code::numeric_conversion),
                   "number cannot be converted"},
      message_case{"ResourceLimit", error::at_offset(error_code::resource_limit, c_at),
                   "line: 2, column: 5: resource limit exceeded"},
      message_case{"AllocationFailure", error::without_location(error_code::allocation_failure), "allocation failure"},
      message_case{"UnknownCode", error::without_location(static_cast<error_code>(99)), "unknown error"}),
    case_name<message_case>);
  INSTANTIATE_TEST_SUITE_P(
    NumericReasons, D96_FormatMessage,
    testing::Values(
      message_case{"NonIntegral", error::numeric_failure(numeric_conversion_reason::non_integral),
                   "number is not an integer"},
      message_case{"OutOfRange", error::numeric_failure(numeric_conversion_reason::out_of_range),
                   "number is out of range"},
      message_case{"NonFinite", error::numeric_failure(numeric_conversion_reason::non_finite), "number is not finite"},
      message_case{"Underflow", error::numeric_failure(numeric_conversion_reason::underflow),
                   "number underflows to zero"},
      message_case{"UnsupportedDestination", error::numeric_failure(numeric_conversion_reason::unsupported_destination),
                   "unsupported numeric destination"},
      message_case{"UnknownNumericReason", error::numeric_failure(static_cast<numeric_conversion_reason>(99)),
                   "number cannot be converted"}),
    case_name<message_case>);
  INSTANTIATE_TEST_SUITE_P(
    SyntaxReasons, D96_FormatMessage,
    testing::Values(
      message_case{"UnexpectedEnd", error::syntax_at(c_at, syntax_error_reason::unexpected_end),
                   "line: 2, column: 5: unexpected end of input"},
      message_case{"UnexpectedCharacter", error::syntax_at(c_at, syntax_error_reason::unexpected_character),
                   "line: 2, column: 5: unexpected character"},
      message_case{"UnexpectedComma", error::syntax_at(c_at, syntax_error_reason::unexpected_comma),
                   "line: 2, column: 5: unexpected comma"},
      message_case{"UnexpectedColon", error::syntax_at(c_at, syntax_error_reason::unexpected_colon),
                   "line: 2, column: 5: unexpected colon"},
      message_case{"UnexpectedClosingBracket", error::syntax_at(c_at, syntax_error_reason::unexpected_closing_bracket),
                   "line: 2, column: 5: unexpected closing bracket"},
      message_case{"MissingColon", error::syntax_at(c_at, syntax_error_reason::missing_colon),
                   "line: 2, column: 5: missing colon"},
      message_case{"MissingComma", error::syntax_at(c_at, syntax_error_reason::missing_comma),
                   "line: 2, column: 5: missing comma"},
      message_case{"MissingValue", error::syntax_at(c_at, syntax_error_reason::missing_value),
                   "line: 2, column: 5: missing value"},
      message_case{"ExpectedMemberName", error::syntax_at(c_at, syntax_error_reason::expected_member_name),
                   "line: 2, column: 5: expected member name"},
      message_case{"UnterminatedString", error::syntax_at(c_at, syntax_error_reason::unterminated_string),
                   "line: 2, column: 5: unterminated string"},
      message_case{"UnterminatedComment", error::syntax_at(c_at, syntax_error_reason::unterminated_comment),
                   "line: 2, column: 5: unterminated comment"},
      message_case{"LineTerminatorInString", error::syntax_at(c_at, syntax_error_reason::line_terminator_in_string),
                   "line: 2, column: 5: line terminator in string"},
      message_case{"ControlCharacterInString", error::syntax_at(c_at, syntax_error_reason::control_character_in_string),
                   "line: 2, column: 5: control character in string"},
      message_case{"InvalidEscape", error::syntax_at(c_at, syntax_error_reason::invalid_escape),
                   "line: 2, column: 5: invalid escape sequence"},
      message_case{"InvalidNumber", error::syntax_at(c_at, syntax_error_reason::invalid_number),
                   "line: 2, column: 5: invalid number"},
      message_case{"InvalidIdentifier", error::syntax_at(c_at, syntax_error_reason::invalid_identifier),
                   "line: 2, column: 5: invalid identifier"},
      message_case{"UnknownLiteral", error::syntax_at(c_at, syntax_error_reason::unknown_literal),
                   "line: 2, column: 5: unknown literal"},
      message_case{"NotAllowedInStrictJson", error::syntax_at(c_at, syntax_error_reason::not_allowed_in_strict_json),
                   "line: 2, column: 5: not allowed in strict JSON"},
      message_case{"EmptyInput", error::syntax_at(c_at, syntax_error_reason::empty_input),
                   "line: 2, column: 5: empty input"},
      message_case{"TrailingContent", error::syntax_at(c_at, syntax_error_reason::trailing_content),
                   "line: 2, column: 5: unexpected content after value"},
      message_case{"UnknownSyntaxReason", error::syntax_at(c_at, static_cast<syntax_error_reason>(99)),
                   "line: 2, column: 5: syntax error"},
      // A reason never overrides a different category.
      message_case{"ReasonIgnoredForOtherCode",
                   error{error_code::encoding, c_at, true, std::nullopt, std::nullopt, std::nullopt,
                         syntax_error_reason::missing_colon},
                   "line: 2, column: 5: invalid UTF-8"}),
    case_name<message_case>);
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief One warning code and its exact formatted text at line 2, column 5.
   * @par Requirements
   * Issue #96; J5-017.
   */
  struct warning_case
  {
      const char* name;         // Case identifier.
      string_warning_code code; // Input warning category.
      const char* expected;     // Expected complete message.
  };
  // Print the case name in failure output.
  void PrintTo(const warning_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class D96_FormatWarning : public testing::TestWithParam<warning_case>
  {
  };
  /*
   * Verify every string warning code has its fixed text with the warning marker.
   * Arrange: A warning per code at line 2, column 5.
   * Act: Format through format_warning_at.
   * Expect: The exact message.
   */
  TEST_P(D96_FormatWarning, ProducesFixedText)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    char buffer[c_buffer_size]{};
    /* Act */
    const auto length = format_warning_at(string_warning{fixture.code, 9U, 3U}, c_position, buffer, sizeof(buffer));
    /* Expect */
    EXPECT_EQ(formatted(length, buffer), fixture.expected);
  }
  INSTANTIATE_TEST_SUITE_P(
    Codes, D96_FormatWarning,
    testing::Values(warning_case{"RawLineSeparator", string_warning_code::raw_line_separator,
                                 "line: 2, column: 5: warning: raw U+2028 line separator in string"},
                    warning_case{"RawParagraphSeparator", string_warning_code::raw_paragraph_separator,
                                 "line: 2, column: 5: warning: raw U+2029 paragraph separator in string"},
                    warning_case{"UnpairedSurrogateEscape", string_warning_code::unpaired_surrogate_escape,
                                 "line: 2, column: 5: warning: unpaired surrogate escape"},
                    warning_case{"UnknownWarning", static_cast<string_warning_code>(99),
                                 "line: 2, column: 5: warning: string warning"}),
    case_name<warning_case>);
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief One input whose error offset maps to a known line and column.
   * @par Requirements
   * Issue #96; J5-044.
   */
  struct position_case
  {
      const char* name;   // Case identifier, naming what precedes the error.
      std::string input;  // Original input bytes.
      std::size_t offset; // Input error byte offset.
      const char* prefix; // Expected position prefix of the message.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const position_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class D96_FormatPosition : public testing::TestWithParam<position_case>
  {
  };
  /*
   * Verify format_error locates 1-based lines and code-point columns in the original input.
   * Arrange: Inputs with each line break kind, multibyte and invalid bytes before the error, and end of input.
   * Act: Format an unexpected-character error at the offset.
   * Expect: The exact line and column prefix followed by the reason text.
   */
  TEST_P(D96_FormatPosition, LocatesLineAndColumn)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    const error failure = error::syntax_at(fixture.offset, syntax_error_reason::unexpected_character);
    /* Act */
    const std::string text = format_in(failure, fixture.input);
    /* Expect */
    EXPECT_EQ(text, std::string{fixture.prefix} + ": unexpected character");
  }
  INSTANTIATE_TEST_SUITE_P(
    Positions, D96_FormatPosition,
    testing::Values(position_case{"FirstCharacter", "@", 0U, "line: 1, column: 1"},
                    position_case{"LineFeed", "ab\n@", 3U, "line: 2, column: 1"},
                    position_case{"CarriageReturn", "a\r@", 2U, "line: 2, column: 1"},
                    position_case{"CrLfCountsOnce", "a\r\n@", 3U, "line: 2, column: 1"},
                    position_case{"LineSeparator", "a\xE2\x80\xA8@", 4U, "line: 2, column: 1"},
                    position_case{"ParagraphSeparator", "a\xE2\x80\xA9@", 4U, "line: 2, column: 1"},
                    position_case{"TwoByteCharacter", "\xC3\xA9@", 2U, "line: 1, column: 2"},
                    position_case{"ThreeByteCharacter", "\xE2\x82\xAC@", 3U, "line: 1, column: 2"},
                    position_case{"FourByteCharacter", "\xF0\x9F\x98\x80@", 4U, "line: 1, column: 2"},
                    position_case{"InvalidByteIsOneColumn", "\xFF@", 1U, "line: 1, column: 2"},
                    position_case{"EndOfInput", "abc", 3U, "line: 1, column: 4"},
                    position_case{"MultiDigit", std::string(11U, '\n') + std::string(33U, 'x') + "@", 44U,
                                  "line: 12, column: 34"}),
    case_name<position_case>);
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify real parse failures format with the issue's example shape and duplicate positions.
   * Arrange: A doubled comma, and a repeated name under reject_duplicates.
   * Act: Parse, then format the returned error against the same input.
   * Expect: The located reason text, and both duplicate positions.
   */
  TEST(D96_FormatParse, FormatsParserErrors)
  {
    /* Arrange */
    const std::string comma{"[1,,2]"};
    const std::string duplicate{"{\"a\":1,\n \"a\":2}"};
    document_provider provider;
    parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto comma_parsed = parse_document({comma.data(), comma.size()}, options, accounting);
    options.duplicate_names = duplicate_name_policy::reject_duplicates;
    const auto duplicate_parsed = parse_document({duplicate.data(), duplicate.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(comma_parsed.has_value());
    EXPECT_EQ(format_in(*comma_parsed.error_if(), comma), "line: 1, column: 4: unexpected comma");
    ASSERT_FALSE(duplicate_parsed.has_value());
    EXPECT_EQ(format_in(*duplicate_parsed.error_if(), duplicate),
              "line: 2, column: 2: duplicate member name (first at line: 1, column: 2)");
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify the simple and positioned forms produce identical text.
   * Arrange: Located, unlocated, duplicate and warning diagnostics over one multi-line input.
   * Act: Format each with the simple form, and with the positioned form using locate() results.
   * Expect: Identical strings and lengths.
   */
  TEST(D96_FormatForms, SimpleAndPositionedFormsMatch)
  {
    /* Arrange */
    const std::string input{"{\n  \"a\": 1,\n  \"a\": 2\n}"};
    const input_view view{input.data(), input.size()};
    const error located = error::syntax_at(13U, syntax_error_reason::missing_comma);
    const error unlocated = error::without_location(error_code::allocation_failure);
    const error duplicate = error::duplicate_member(source_span{4U, 3U}, source_span{14U, 3U});
    const string_warning warning{string_warning_code::unpaired_surrogate_escape, 15U, 6U};
    /* Act */
    const std::string located_simple = format_in(located, input);
    const std::string located_at = format_at(located, *locate(view, 13U).value_if());
    const std::string unlocated_simple = format_in(unlocated, input);
    const std::string unlocated_at = format_at(unlocated, text_position{});
    const std::string duplicate_simple = format_in(duplicate, input);
    const std::string duplicate_at = format_at(duplicate, *locate(view, 14U).value_if(), *locate(view, 4U).value_if());
    char simple_buffer[c_buffer_size]{};
    char at_buffer[c_buffer_size]{};
    const auto warning_simple = format_warning(warning, view, simple_buffer, sizeof(simple_buffer));
    const auto warning_at = format_warning_at(warning, *locate(view, 15U).value_if(), at_buffer, sizeof(at_buffer));
    /* Expect */
    EXPECT_EQ(located_simple, located_at);
    EXPECT_EQ(unlocated_simple, unlocated_at);
    EXPECT_EQ(duplicate_simple, duplicate_at);
    EXPECT_EQ(formatted(warning_simple, simple_buffer), formatted(warning_at, at_buffer));
    EXPECT_EQ(duplicate_simple, "line: 3, column: 3: duplicate member name (first at line: 2, column: 3)");
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify caller storage handling: size query, exact fit, truncation and no writes beyond capacity.
   * Arrange: One message of known length and buffers of several capacities filled with a marker.
   * Act: Format into each capacity.
   * Expect: The full length every time; text and NUL within capacity; bytes beyond capacity untouched.
   */
  TEST(D96_FormatBuffer, TruncatesSafelyAndReportsRequiredLength)
  {
    /* Arrange */
    const error failure = error::syntax_at(0U, syntax_error_reason::unexpected_comma);
    const std::string expected{"line: 1, column: 1: unexpected comma"};
    const std::string input{","};
    const input_view view{input.data(), input.size()};
    /* Act */
    const auto query = format_error(failure, view, nullptr, 0U);
    /* Expect */
    ASSERT_TRUE(query.has_value());
    EXPECT_EQ(*query.value_if(), expected.size());
    for (const std::size_t capacity : {std::size_t{1U}, std::size_t{2U}, expected.size(), expected.size() + 1U})
    {
      /* Arrange */
      char buffer[c_buffer_size];
      std::memset(buffer, '#', sizeof(buffer));
      /* Act */
      const auto length = format_error(failure, view, buffer, capacity);
      /* Expect */
      ASSERT_TRUE(length.has_value()) << "capacity " << capacity;
      EXPECT_EQ(*length.value_if(), expected.size()) << "capacity " << capacity;
      const std::size_t stored = (expected.size() < capacity) ? expected.size() : (capacity - 1U);
      EXPECT_EQ(std::string(buffer, stored), expected.substr(0U, stored)) << "capacity " << capacity;
      EXPECT_EQ(buffer[stored], '\0') << "capacity " << capacity;
      for (std::size_t index = capacity; index < sizeof(buffer); ++index)
      {
        EXPECT_EQ(buffer[index], '#') << "capacity " << capacity << " index " << index;
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify invalid arguments return wrong_type_or_access without writing.
   * Arrange: A null buffer with nonzero capacity, and offsets beyond the input.
   * Act: Call each form.
   * Expect: wrong_type_or_access from every call; the marker buffer is unchanged.
   */
  TEST(D96_FormatArguments, RejectsNullStorageAndOffsetsBeyondInput)
  {
    /* Arrange */
    const std::string input{"[1]"};
    const input_view view{input.data(), input.size()};
    const error beyond = error::syntax_at(4U, syntax_error_reason::unexpected_end);
    const error first_beyond = error::duplicate_member(source_span{9U, 1U}, source_span{1U, 1U});
    const string_warning warning_beyond{string_warning_code::raw_line_separator, 4U, 3U};
    const error valid = error::syntax_at(0U, syntax_error_reason::unexpected_end);
    char buffer[8];
    std::memset(buffer, '#', sizeof(buffer));
    /* Act */
    const auto null_error = format_error(valid, view, nullptr, 4U);
    const auto null_error_at = format_error_at(valid, text_position{}, std::nullopt, nullptr, 4U);
    const auto null_warning_at =
      format_warning_at(string_warning{string_warning_code::raw_line_separator, 0U, 3U}, text_position{}, nullptr, 4U);
    const auto offset_error = format_error(beyond, view, buffer, sizeof(buffer));
    const auto first_error = format_error(first_beyond, view, buffer, sizeof(buffer));
    const auto offset_warning = format_warning(warning_beyond, view, buffer, sizeof(buffer));
    /* Expect */
    for (const auto* outcome :
         {&null_error, &null_error_at, &null_warning_at, &offset_error, &first_error, &offset_warning})
    {
      ASSERT_FALSE(outcome->has_value());
      EXPECT_EQ(outcome->error_if()->code, error_code::wrong_type_or_access);
    }
    for (const char byte : buffer)
    {
      EXPECT_EQ(byte, '#');
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify output does not depend on the C locale.
   * Arrange: An error on line 1234 and the environment's native locale selected globally.
   * Act: Format before and after switching the locale.
   * Expect: Identical text with plain ASCII digits and no grouping separators.
   */
  TEST(D96_FormatLocale, IgnoresGlobalLocale)
  {
    /* Arrange */
    const std::string input = std::string(1233U, '\n') + "@";
    const error failure = error::syntax_at(1233U, syntax_error_reason::unexpected_character);
    const std::string saved{std::setlocale(LC_ALL, nullptr)};
    /* Act */
    const std::string classic = format_in(failure, input);
    std::setlocale(LC_ALL, "");
    const std::string native = format_in(failure, input);
    std::setlocale(LC_ALL, saved.c_str());
    /* Expect */
    EXPECT_EQ(classic, "line: 1234, column: 1: unexpected character");
    EXPECT_EQ(native, classic);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify formatting performs no allocation in either form.
   * Arrange: Located, duplicate and warning diagnostics over a small input.
   * Act: Count global allocations while formatting each, including a size query and truncation.
   * Expect: Zero allocations.
   */
  TEST(D96_FormatAllocation, AllocatesNothing)
  {
    /* Arrange */
    const char input[] = "{\"a\":1,\n\"a\":2}";
    const input_view view{input, sizeof(input) - 1U};
    const error located = error::syntax_at(3U, syntax_error_reason::missing_colon);
    const error duplicate = error::duplicate_member(source_span{1U, 3U}, source_span{8U, 3U});
    const string_warning warning{string_warning_code::raw_line_separator, 2U, 3U};
    char buffer[16];
    /* Act */
    g_probe.m_count = 0U;
    g_probe.m_counting = true;
    const bool all_succeeded =
      format_error(located, view, buffer, sizeof(buffer)).has_value() &&
      format_error(duplicate, view, nullptr, 0U).has_value() &&
      format_error_at(duplicate, text_position{2U, 1U}, text_position{1U, 2U}, buffer, sizeof(buffer)).has_value() &&
      format_warning(warning, view, buffer, sizeof(buffer)).has_value() &&
      format_warning_at(warning, text_position{1U, 3U}, buffer, 1U).has_value();
    g_probe.m_counting = false;
    /* Expect */
    EXPECT_TRUE(all_succeeded);
    EXPECT_EQ(g_probe.m_count, 0U);
  }
  /* ================================== End Test_D96 =================================== */
} // namespace
