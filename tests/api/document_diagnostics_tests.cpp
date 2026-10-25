#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/text_position.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::document_provider;

  constexpr std::size_t c_max_injected_failures = 64;

  /**
   * @brief Read an optional source span from a checked view, failing the test when absent.
   * @par Requirements
   * P039; J5-041.
   */
  source_span span_of(const result<std::optional<source_span>>& span)
  {
    EXPECT_TRUE(span.has_value());
    if (!span.has_value() || !span.value_if()->has_value())
    {
      ADD_FAILURE() << "missing source span";
      return source_span{};
    }
    return **span.value_if();
  }

  /**
   * @brief Find the smallest owned-byte ceiling that still parses the input under a retention policy.
   * @par Requirements
   * P039; J5-041; J5-046.
   * @note Success is monotonic in the ceiling, so a binary search finds the boundary.
   */
  std::size_t minimal_byte_limit(std::string_view input, source_retention_policy retention)
  {
    std::size_t low = 1U;
    std::size_t high = 1U << 20U;
    while (low < high)
    {
      const std::size_t middle = low + ((high - low) / 2U);
      document_provider provider;
      parse_options options{};
      options.limits.max_owned_bytes = middle;
      options.limits.max_input_bytes = input.size();
      options.source_text = retention;
      accounted_storage accounting{provider.callbacks(), middle};
      const bool parsed = parse_document({input.data(), input.size()}, options, accounting).has_value();
      if (parsed)
      {
        high = middle;
      }
      else
      {
        low = middle + 1U;
      }
    }
    return low;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_044 =================================== */
  /**
   * @brief Describe one rejected document and its complete diagnostic location.
   * @par Requirements
   * P039; J5-043; J5-044.
   */
  struct location_case
  {
      const char* name;
      std::string_view input; // Input complete document bytes.
      syntax_mode mode;       // Input language selection.
      error_code code;        // Expected error category.
      std::size_t offset;     // Expected zero-based primary byte offset.
      bool has_range;         // Whether a source range is expected.
      source_span range;      // Expected source range when present.
      std::size_t line;       // Expected 1-based line of the primary offset.
      std::size_t column;     // Expected 1-based column of the primary offset.
  };

  std::ostream& operator<<(std::ostream& stream, const location_case& value)
  {
    return stream << value.name;
  }

  class DiagnosticLocationUnits : public ::testing::TestWithParam<location_case>
  {
  };

  /* Verify rejected documents report exact byte offsets, ranges and line/column positions.
   * Arrange: Select the case document and mode.
   * Act: Parse it, then locate the error offset in the same bytes.
   * Expect: The case category, offset, range and position, with all storage released.
   */
  TEST_P(DiagnosticLocationUnits, ReportsExactLocation)
  {
    /* Arrange */
    const location_case& param = GetParam();
    document_provider provider;
    parse_options options{param.mode};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const input_view input{param.input.data(), param.input.size()};
    /* Act */
    const auto parsed = parse_document(input, options, accounting);
    ASSERT_FALSE(parsed.has_value());
    const error& failure = *parsed.error_if();
    const auto position = locate(input, failure.byte_offset);
    /* Expect */
    EXPECT_EQ(failure.code, param.code);
    ASSERT_TRUE(failure.has_byte_offset);
    EXPECT_EQ(failure.byte_offset, param.offset);
    ASSERT_EQ(failure.source_range.has_value(), param.has_range);
    if (param.has_range)
    {
      EXPECT_EQ(failure.source_range->byte_offset, param.range.byte_offset);
      EXPECT_EQ(failure.source_range->byte_length, param.range.byte_length);
    }
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(position.value_if()->line, param.line);
    EXPECT_EQ(position.value_if()->column, param.column);
    EXPECT_EQ(accounting.current_bytes(), 0U);
  }

  constexpr auto c_json5 = syntax_mode::json5;
  constexpr auto c_strict = syntax_mode::strict_json;
  constexpr auto c_syntax = error_code::syntax;
  constexpr auto c_encoding = error_code::encoding;

  /**
   * @brief Build a case without a source range.
   * @par Requirements
   * P039; J5-044.
   */
  constexpr location_case at(const char* name, std::string_view input, syntax_mode mode, error_code code,
                             std::size_t offset, std::size_t line, std::size_t column)
  {
    return location_case{name, input, mode, code, offset, false, source_span{}, line, column};
  }

  /**
   * @brief Build a case with a source range.
   * @par Requirements
   * P039; J5-044.
   */
  constexpr location_case ranged(const char* name, std::string_view input, std::size_t offset, source_span range,
                                 std::size_t line, std::size_t column)
  {
    return location_case{name, input, c_json5, c_syntax, offset, true, range, line, column};
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_044_DiagnosticLocation, DiagnosticLocationUnits,
    ::testing::Values(at("TokenAfterLineFeed", "[1,\n x]", c_json5, c_syntax, 5U, 2U, 2U),
                      at("TokenAfterCrLf", "[1,\r\n x]", c_json5, c_syntax, 6U, 2U, 2U),
                      at("TokenAfterCarriageReturn", "[1,\r x]", c_json5, c_syntax, 5U, 2U, 2U),
                      at("TokenAfterLineSeparator", "[1,\xE2\x80\xA8 x]", c_json5, c_syntax, 7U, 2U, 2U),
                      at("TokenAfterNonBmpString", "[\"\xF0\x9F\x98\x80\", x]", c_json5, c_syntax, 9U, 1U, 7U),
                      at("InvalidUtf8InString", "[\"a\xFF\"]", c_json5, c_encoding, 3U, 1U, 4U),
                      at("TruncatedUtf8InString", "[\"a\xE2\x80", c_json5, c_encoding, 3U, 1U, 4U),
                      at("OverlongUtf8", "[\"\xC0\x80\"]", c_json5, c_encoding, 2U, 1U, 3U),
                      at("EncodedSurrogate", "[\"\xED\xA0\x80\"]", c_json5, c_encoding, 2U, 1U, 3U),
                      at("TruncatedArrayAtEnd", "[1, 2", c_json5, c_syntax, 5U, 1U, 6U),
                      at("TruncatedObjectAtEnd", "{a: 1", c_json5, c_syntax, 5U, 1U, 6U),
                      at("EmptyInput", "", c_json5, c_syntax, 0U, 1U, 1U),
                      at("TrailingJunk", "1 2", c_json5, c_syntax, 2U, 1U, 3U),
                      at("StrictInfinity", "Infinity", c_strict, c_syntax, 0U, 1U, 1U),
                      at("StringEndsInsideEscape", "\"a\\", c_json5, c_syntax, 3U, 1U, 4U),
                      at("RawLineFeedInString", "\"a\nb\"", c_json5, c_syntax, 2U, 1U, 3U),
                      ranged("UnterminatedRootString", "\"abc", 4U, source_span{0U, 4U}, 1U, 5U),
                      ranged("UnterminatedValueString", "{a: 'x", 6U, source_span{4U, 2U}, 1U, 7U),
                      ranged("UnterminatedMemberName", "{\"ab", 4U, source_span{1U, 3U}, 1U, 5U),
                      ranged("UnterminatedCommentAfterRoot", "1 /* x", 6U, source_span{2U, 4U}, 1U, 7U),
                      ranged("UnterminatedCommentInArray", "[1, /* x", 8U, source_span{4U, 4U}, 1U, 9U),
                      ranged("UnterminatedCommentOnSecondLine", "[1,\r\n/* x", 9U, source_span{5U, 4U}, 2U, 5U)),
    [](const ::testing::TestParamInfo<location_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify duplicate rejection reports both occurrences, located on separate lines.
   * Arrange: An object repeating a name on its second line, under duplicate rejection.
   * Act: Parse, then locate both name spans.
   * Expect: duplicate_policy at the repeated name, both spans, and positions 1:2 and 2:1.
   */
  TEST(J5_044_DiagnosticLocation, LocatesBothDuplicateOccurrences)
  {
    /* Arrange */
    constexpr std::string_view c_text{"{a: 1,\na: 2}"};
    const input_view input{c_text.data(), c_text.size()};
    document_provider provider;
    parse_options options{};
    options.duplicate_names = duplicate_name_policy::reject_duplicates;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_document(input, options, accounting);
    ASSERT_FALSE(parsed.has_value());
    const error& failure = *parsed.error_if();
    ASSERT_TRUE(failure.duplicate_names.has_value());
    const auto first = locate(input, failure.duplicate_names->first.byte_offset);
    const auto repeated = locate(input, failure.duplicate_names->repeated.byte_offset);
    /* Expect */
    EXPECT_EQ(failure.code, error_code::duplicate_policy);
    EXPECT_EQ(failure.byte_offset, 7U);
    EXPECT_EQ(failure.duplicate_names->first.byte_offset, 1U);
    EXPECT_EQ(failure.duplicate_names->repeated.byte_offset, 7U);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(repeated.has_value());
    EXPECT_EQ(first.value_if()->line, 1U);
    EXPECT_EQ(first.value_if()->column, 2U);
    EXPECT_EQ(repeated.value_if()->line, 2U);
    EXPECT_EQ(repeated.value_if()->column, 1U);
  }
  /* ================================= End Test_J5_044 ================================= */
  /* =================================== Test_J5_041 =================================== */
  /* Verify exact value and member-name spans around comments, escapes, repeated names, CRLF and multibyte text.
   * Arrange: A CRLF-delimited object with a comment, an escaped quoted name, a multibyte value,
   * a repeated identifier name and a four-byte string; parse it and destroy the input.
   * Act: Move the parsed document and read spans through the moved views.
   * Expect: Every span equals the independently computed byte range of its token.
   */
  TEST(J5_041_SourceSpans, ExactAcrossTriviaEscapesMultibyteAndMoves)
  {
    /* Arrange */
    document_provider provider;
    parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    auto input = std::string{"\r\n/*c*/{\"k\\u0041\": 'v\xC3\xA9', k: [1, \"\xF0\x9F\x98\x80\"], k: 2}\r\n"};
    auto parsed = parse_document({input.data(), input.size()}, options, accounting);
    ASSERT_TRUE(parsed.has_value());
    input.assign(input.size(), '#');
    input = std::string{};
    /* Act */
    const parsed_document moved{std::move(*parsed.value_if())};
    const auto root = moved.values.root();
    ASSERT_TRUE(root.has_value());
    const auto first = root.value_if()->member_at(0U);
    const auto second = root.value_if()->member_at(1U);
    const auto third = root.value_if()->member_at(2U);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    const auto one = second.value_if()->element_at(0U);
    const auto emoji = second.value_if()->element_at(1U);
    ASSERT_TRUE(one.has_value());
    ASSERT_TRUE(emoji.has_value());
    /* Expect */
    const auto expect_span = [](const source_span& actual, std::size_t offset, std::size_t length)
    {
      EXPECT_EQ(actual.byte_offset, offset);
      EXPECT_EQ(actual.byte_length, length);
    };
    expect_span(span_of(root.value_if()->source_span()), 7U, 40U);
    expect_span(span_of(first.value_if()->member_name_source_span()), 8U, 9U);
    expect_span(span_of(first.value_if()->source_span()), 19U, 5U);
    expect_span(span_of(second.value_if()->member_name_source_span()), 26U, 1U);
    expect_span(span_of(second.value_if()->source_span()), 29U, 11U);
    expect_span(span_of(one.value_if()->source_span()), 30U, 1U);
    expect_span(span_of(emoji.value_if()->source_span()), 33U, 6U);
    expect_span(span_of(third.value_if()->member_name_source_span()), 42U, 1U);
    expect_span(span_of(third.value_if()->source_span()), 45U, 1U);
  }
  /* ----------------------------------------------------------------------------------- */

  /* Verify retained source is off by default and, when requested, survives input destruction and moves.
   * Arrange: Parse the same document with default options and with retain_copy.
   * Act: Overwrite and destroy the input, move the retaining document and locate a warning in its copy.
   * Expect: Default retains nothing; the moved copy equals the original text and maps the warning to 2:6.
   */
  TEST(J5_041_RetainedSourceParse, CopiesOnlyOnRequestAndOutlivesInput)
  {
    /* Arrange */
    const std::string original{"{a: 1,\r\nb: 'x\xE2\x80\xA8'}"};
    document_provider provider;
    parse_options discard{};
    parse_options retain{};
    retain.source_text = source_retention_policy::retain_copy;
    accounted_storage accounting{provider.callbacks(), discard.limits.max_owned_bytes};
    auto input = original;
    const auto plain = parse_document({input.data(), input.size()}, discard, accounting);
    auto kept = parse_document({input.data(), input.size()}, retain, accounting);
    ASSERT_TRUE(plain.has_value());
    ASSERT_TRUE(kept.has_value());
    /* Act */
    input.assign(input.size(), '#');
    input = std::string{};
    const parsed_document moved{std::move(*kept.value_if())};
    const input_view text = moved.source.text();
    ASSERT_EQ(moved.warnings.size(), 1U);
    const auto warning = moved.warnings.at(0U);
    ASSERT_TRUE(warning.has_value());
    const auto position = locate(text, warning.value_if()->byte_offset);
    /* Expect */
    EXPECT_FALSE(plain.value_if()->source.is_retained());
    EXPECT_TRUE(moved.source.is_retained());
    EXPECT_EQ(std::string_view(text.data, text.size), original);
    EXPECT_FALSE(kept.value_if()->source.is_retained());
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(position.value_if()->line, 2U);
    EXPECT_EQ(position.value_if()->column, 6U);
  }
  /* ----------------------------------------------------------------------------------- */

  /* Verify the retained copy is charged against max_owned_bytes.
   * Arrange: A mostly-whitespace input whose copy dwarfs its document.
   * Act: Find the smallest working byte ceiling with and without retention.
   * Expect: Retention needs a larger ceiling, at least the input size, and fails at the non-retaining minimum.
   */
  TEST(J5_041_RetainedSourceParse, ChargesCopyToOwnedByteLimit)
  {
    /* Arrange */
    const std::string input = "[1" + std::string(3000U, ' ') + "]";
    /* Act */
    const std::size_t without = minimal_byte_limit(input, source_retention_policy::discard);
    const std::size_t with = minimal_byte_limit(input, source_retention_policy::retain_copy);
    document_provider provider;
    parse_options options{};
    options.limits.max_owned_bytes = without;
    options.limits.max_input_bytes = input.size();
    options.source_text = source_retention_policy::retain_copy;
    accounted_storage accounting{provider.callbacks(), without};
    const auto denied = parse_document({input.data(), input.size()}, options, accounting);
    /* Expect */
    EXPECT_GT(with, without);
    EXPECT_GE(with, input.size());
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(accounting.current_bytes(), 0U);
  }
  /* ================================= End Test_J5_041 ================================= */
  /* =================================== Test_J5_045 =================================== */
  /* Verify every allocation failure yields a valid minimal error and releases everything, then retry succeeds.
   * Arrange: A document with nested containers, escapes, warnings, numbers and source retention.
   * Act: Fail the first, second, ... allocation in fresh providers until parsing succeeds.
   * Expect: Each failure is allocation_failure without extra metadata or outstanding bytes; success follows.
   */
  TEST(J5_045_MinimalFallback, ReportsEveryAllocationFailureWithoutLeaks)
  {
    /* Arrange */
    const std::string input{"{\"a\\u0041\": [1.5e2, 'x\xE2\x80\xA8', {b: null}], c: 'y\xE2\x80\xA9'}"};
    parse_options options{};
    options.source_text = source_retention_policy::retain_copy;
    std::size_t failures{};
    bool succeeded{};
    /* Act */
    for (std::size_t fail_on = 1U; (fail_on <= c_max_injected_failures) && !succeeded; ++fail_on)
    {
      document_provider provider;
      provider.m_fail_on = fail_on;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
      if (parsed.has_value())
      {
        succeeded = true;
        EXPECT_EQ(parsed.value_if()->warnings.size(), 2U);
        EXPECT_TRUE(parsed.value_if()->source.is_retained());
        continue;
      }
      // Only a failed parse must release everything; a successful document still owns its storage.
      EXPECT_EQ(accounting.current_bytes(), 0U) << "fail_on " << fail_on;
      const error& failure = *parsed.error_if();
      EXPECT_EQ(failure.code, error_code::allocation_failure) << "fail_on " << fail_on;
      EXPECT_FALSE(failure.duplicate_names.has_value()) << "fail_on " << fail_on;
      EXPECT_FALSE(failure.numeric_reason.has_value()) << "fail_on " << fail_on;
      EXPECT_FALSE(failure.source_range.has_value()) << "fail_on " << fail_on;
      if (failure.has_byte_offset)
      {
        EXPECT_LT(failure.byte_offset, input.size()) << "fail_on " << fail_on;
      }
      ++failures;
    }
    /* Expect */
    EXPECT_TRUE(succeeded);
    EXPECT_GT(failures, 1U);
  }
  /* ----------------------------------------------------------------------------------- */

  /* Verify a diagnostic-limit failure is reported without needing any allocation for the report.
   * Arrange: Two warnings in separate strings and a single diagnostic entry.
   * Act: Parse once.
   * Expect: resource_limit stored inline in the result, no partial document and complete cleanup.
   */
  TEST(J5_045_MinimalFallback, ReportsDiagnosticLimitWithoutExtraAllocation)
  {
    /* Arrange */
    const std::string input{"['\xE2\x80\xA8', '\xE2\x80\xA9']"};
    document_provider provider;
    parse_options options{};
    options.limits.max_diagnostics = 1U;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(parsed.value_if(), nullptr);
    EXPECT_EQ(accounting.current_bytes(), 0U);
  }
  /* ================================= End Test_J5_045 ================================= */
} // namespace
