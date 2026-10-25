#include "../support/document_provider.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <gtest/gtest.h>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_018 ==================================== */
  /**
   * @brief Specify decoded units and advisory spans independently of the pairing implementation.
   * @par Requirements
   * P041; J5-018; J5-041.
   */
  struct surrogate_case
  {
      const char* name;                   // Input case identifier.
      std::string_view input;             // Input complete source bytes.
      std::u16string_view units;          // Expected exact decoded UTF-16 units.
      std::array<std::size_t, 3> offsets; // Expected warning offsets in source bytes.
      std::size_t count;                  // Expected number of warnings.
      bool json5_only;                    // Input requires JSON5 syntax.
  };
  class J5_018_SurrogateWarnings : public testing::TestWithParam<surrogate_case>
  {
  };
  /*
   * Arrange: Lone, reversed, repeated and paired escapes, including empty continuations.
   * Act: Parse in each applicable mode and request checked UTF-8 export.
   * Expect: Exact units, one six-byte advisory per unpaired escape, no substitution.
   */
  TEST_P(J5_018_SurrogateWarnings, PreservesUnitsAndReportsExactEscapes)
  {
    /* Arrange */
    const auto fixture = GetParam();
    for (const auto mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      if (fixture.json5_only && mode == syntax_mode::strict_json)
      {
        continue;
      }
      SCOPED_TRACE(fixture.name);
      SCOPED_TRACE(static_cast<int>(mode));
      document_provider provider;
      parse_options options{mode};
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      const auto parsed = parse_document({fixture.input.data(), fixture.input.size()}, options, accounting);
      /* Expect */
      ASSERT_TRUE(parsed.has_value());
      const auto& warnings = parsed.value_if()->warnings;
      ASSERT_EQ(warnings.size(), fixture.count);
      for (std::size_t index{}; index < fixture.count; ++index)
      {
        const auto warning = warnings.at(index);
        ASSERT_TRUE(warning.has_value());
        EXPECT_EQ(warning.value_if()->code, string_warning_code::unpaired_surrogate_escape);
        EXPECT_EQ(warning.value_if()->byte_offset, fixture.offsets[index]);
        EXPECT_EQ(warning.value_if()->byte_length, 6u);
      }
      const auto root = parsed.value_if()->values.root();
      ASSERT_TRUE(root.has_value());
      const auto string = root.value_if()->as_string();
      ASSERT_TRUE(string.has_value());
      ASSERT_EQ((*string.value_if())->size(), fixture.units.size());
      for (std::size_t index{}; index < fixture.units.size(); ++index)
      {
        const auto unit = (*string.value_if())->unit_at(index);
        ASSERT_TRUE(unit.has_value());
        EXPECT_EQ(*unit.value_if(), fixture.units[index]);
      }
      std::array<unsigned char, 32> output{};
      output.fill(0xA5);
      const auto exported = export_utf8(**string.value_if(), output.data(), output.size());
      EXPECT_EQ(exported.has_value(), fixture.count == 0);
      if (fixture.count != 0)
      {
        ASSERT_NE(exported.error_if(), nullptr);
        EXPECT_EQ(exported.error_if()->code, error_code::encoding);
        for (const auto byte : output)
        {
          EXPECT_EQ(byte, 0xA5);
        }
      }
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    P041, J5_018_SurrogateWarnings,
    testing::Values(surrogate_case{"High", R"("\uD800")", u"\xD800", {1}, 1, false},
                    surrogate_case{"Low", R"("\uDC00")", u"\xDC00", {1}, 1, false},
                    surrogate_case{"Reversed", R"("\uDC00\uD800")", u"\xDC00\xD800", {1, 7}, 2, false},
                    surrogate_case{"RepeatedHigh", R"("\uD800\uD801\uDC00")", u"\xD800\xD801\xDC00", {1}, 1, false},
                    surrogate_case{"RepeatedLow", R"("\uD800\uDC00\uDC01")", u"\xD800\xDC00\xDC01", {13}, 1, false},
                    surrogate_case{"Pair", R"("\uD800\uDC00")", u"\xD800\xDC00", {}, 0, false},
                    surrogate_case{"LastPair", R"("\uDBFF\uDFFF")", u"\xDBFF\xDFFF", {}, 0, false},
                    surrogate_case{"RawInterrupt",
                                   R"("\uD800a\uDC00")",
                                   u"\xD800"
                                   u"a\xDC00",
                                   {1, 8},
                                   2,
                                   false},
                    surrogate_case{"EscapeInterrupt", R"("\uD800\n\uDC00")", u"\xD800\n\xDC00", {1, 9}, 2, false},
                    surrogate_case{"BmpInterrupt",
                                   R"("\uD800\u0061\uDC00")",
                                   u"\xD800"
                                   u"a\xDC00",
                                   {1, 13},
                                   2,
                                   false},
                    surrogate_case{"MultibytePrefix", "\"\xc3\xa9\\uD800\"", u"\u00e9\xD800", {3}, 1, false},
                    surrogate_case{"ContinuationPair", "\"\\uD800\\\n\\uDC00\"", u"\xD800\xDC00", {}, 0, true},
                    surrogate_case{"CrLfPair", "\"\\uD800\\\r\n\\uDC00\"", u"\xD800\xDC00", {}, 0, true},
                    surrogate_case{"ContinuedHigh", "'\\uD800\\\n'", u"\xD800", {1}, 1, true},
                    surrogate_case{"EscapedBackslash", R"("\\uD800")", u"\\uD800", {}, 0, false}),
    [](const testing::TestParamInfo<surrogate_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Unpaired escapes in a member name and value beside raw JSON5 separators.
   * Act: Parse using both syntax modes.
   * Expect: Source-ordered advisories with exact byte spans and mode-specific separators.
   */
  TEST(J5_018_CompleteWarnings, IncludesNamesAndOrdersMixedAdvisories)
  {
    /* Arrange */
    constexpr std::string_view input{"{\"\\uD800\xe2\x80\xa8\":\"\\uDC00\xe2\x80\xa9\"}"};
    for (const auto mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      document_provider provider;
      parse_options options{mode};
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
      /* Expect */
      ASSERT_TRUE(parsed.has_value());
      const bool json5 = mode == syntax_mode::json5;
      ASSERT_EQ(parsed.value_if()->warnings.size(), json5 ? 4u : 2u);
      const std::array<string_warning, 4> expected{{{string_warning_code::unpaired_surrogate_escape, 2, 6},
                                                    {string_warning_code::raw_line_separator, 8, 3},
                                                    {string_warning_code::unpaired_surrogate_escape, 14, 6},
                                                    {string_warning_code::raw_paragraph_separator, 20, 3}}};
      for (std::size_t index{}; index < parsed.value_if()->warnings.size(); ++index)
      {
        const auto warning = parsed.value_if()->warnings.at(index);
        ASSERT_TRUE(warning.has_value());
        const auto& wanted = expected[json5 ? index : index * 2];
        EXPECT_EQ(warning.value_if()->code, wanted.code);
        EXPECT_EQ(warning.value_if()->byte_offset, wanted.byte_offset);
        EXPECT_EQ(warning.value_if()->byte_length, wanted.byte_length);
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Two unpaired units and diagnostic ceilings below, at and above two.
   * Act: Parse through the public entry.
   * Expect: Explicit resource failure below the inclusive boundary, success otherwise.
   */
  TEST(J5_018_CompleteWarnings, EnforcesDiagnosticBoundary)
  {
    /* Arrange */
    constexpr std::string_view input{R"("\uDC00\uD800")"};
    for (const auto mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      for (std::size_t limit{}; limit <= 3; ++limit)
      {
        document_provider provider;
        parse_options options{mode};
        options.limits.max_diagnostics = limit;
        accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
        /* Act */
        const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
        /* Expect */
        ASSERT_EQ(parsed.has_value(), limit >= 2);
        if (limit < 2)
        {
          EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
        }
        else
        {
          EXPECT_EQ(parsed.value_if()->warnings.size(), 2u);
        }
      }
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Measure allocations for unpaired surrogates in names and values.
   * Act: Fail each allocation, then retry with the same storage and options.
   * Expect: Explicit allocation failure, no leaked storage and successful warning retention on retry.
   */
  TEST(J5_018_CompleteWarnings, FailsEveryAllocationAndAllowsRetry)
  {
    /* Arrange */
    constexpr std::string_view input{R"({"\uD800":"\uDC00\uD800"})"};
    for (const auto mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      document_provider provider;
      parse_options options{mode};
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      {
        const auto baseline = parse_document({input.data(), input.size()}, options, accounting);
        ASSERT_TRUE(baseline.has_value());
        ASSERT_EQ(baseline.value_if()->warnings.size(), 3u);
      }
      const auto attempts = provider.m_attempts;
      ASSERT_GT(attempts, 0u);
      /* Act */
      for (std::size_t failure = 1; failure <= attempts; ++failure)
      {
        SCOPED_TRACE(failure);
        provider.m_fail_on = provider.m_attempts + failure;
        {
          const auto rejected = parse_document({input.data(), input.size()}, options, accounting);
          /* Expect */
          ASSERT_FALSE(rejected.has_value());
          EXPECT_EQ(rejected.error_if()->code, error_code::allocation_failure);
        }
        EXPECT_EQ(provider.m_live_bytes, 0u);
        provider.m_fail_on = 0;
        {
          const auto retried = parse_document({input.data(), input.size()}, options, accounting);
          ASSERT_TRUE(retried.has_value());
          EXPECT_EQ(retried.value_if()->warnings.size(), 3u);
        }
        EXPECT_EQ(provider.m_live_bytes, 0u);
      }
    }
  }
  /* ================================= End Test_J5_018 ================================== */
} // namespace
