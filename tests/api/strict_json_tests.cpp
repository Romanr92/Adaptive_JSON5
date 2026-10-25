#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/parser.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <ostream>
#include <string>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;
  /* ==================================== Fixtures ===================================== */
  /**
   * @brief Check that a failed parse published nothing and released every allocation.
   * @par Requirements
   * P042; J5-051.
   */
  void expect_released(const document_provider& provider, const accounted_storage& accounting, const char* label)
  {
    EXPECT_EQ(accounting.current_bytes(), 0U) << label;
    EXPECT_EQ(provider.m_live_bytes, 0U) << label;
    EXPECT_EQ(provider.m_allocations, provider.m_releases) << label;
  }

  /**
   * @brief Name parameterized cases after their name field.
   * @par Requirements
   * P042.
   */
  template <typename T>
  std::string case_name(const testing::TestParamInfo<T>& info)
  {
    return info.param.name;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_005 =================================== */
  /**
   * @brief One strict JSON document covering a JSON grammar branch.
   * @par Requirements
   * P042; J5-005; J5-010; J5-021.
   */
  struct strict_accept_case
  {
      const char* name;  // Case identifier, naming the JSON production.
      std::string input; // Complete strict JSON input bytes.
      value_kind root;   // Expected root value kind.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const strict_accept_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_005_StrictAccept : public testing::TestWithParam<strict_accept_case>
  {
  };
  /*
   * Verify strict mode accepts every JSON grammar branch and JSON5 accepts the same bytes.
   * Arrange: One minimal document per JSON value, container, number and whitespace production.
   * Act: Parse the same bytes in strict mode and in JSON5 mode.
   * Expect: Both succeed with the expected root kind.
   */
  TEST_P(J5_005_StrictAccept, AcceptsJsonProductionInBothModes)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    const input_view input{fixture.input.data(), fixture.input.size()};
    for (const syntax_mode mode : {syntax_mode::strict_json, syntax_mode::json5})
    {
      document_provider provider;
      const parse_options options{mode};
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      const auto parsed = parse_document(input, options, accounting);
      /* Expect */
      ASSERT_TRUE(parsed.has_value()) << fixture.name;
      const auto kind = parsed.value_if()->values.root().value_if()->kind();
      ASSERT_TRUE(kind.has_value());
      EXPECT_EQ(*kind.value_if(), fixture.root) << fixture.name;
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    P042, J5_005_StrictAccept,
    testing::Values(
      strict_accept_case{"Null", "null", value_kind::null}, strict_accept_case{"True", "true", value_kind::boolean},
      strict_accept_case{"False", "false", value_kind::boolean},
      strict_accept_case{"EmptyString", "\"\"", value_kind::string},
      strict_accept_case{"EmptyArray", "[]", value_kind::array},
      strict_accept_case{"EmptyObject", "{}", value_kind::object},
      strict_accept_case{"NestedContainers", "{\"a\":[{\"b\":[]},{}],\"c\":{\"d\":null}}", value_kind::object},
      strict_accept_case{"ArrayOfEveryValue", "[null,true,false,0,\"s\",[],{}]", value_kind::array},
      strict_accept_case{"Zero", "0", value_kind::number}, strict_accept_case{"NegativeZero", "-0", value_kind::number},
      strict_accept_case{"Integer", "123", value_kind::number},
      strict_accept_case{"NegativeInteger", "-9", value_kind::number},
      strict_accept_case{"Fraction", "0.25", value_kind::number},
      strict_accept_case{"LowerExponent", "1e5", value_kind::number},
      strict_accept_case{"UpperExponent", "1E5", value_kind::number},
      strict_accept_case{"PlusExponent", "1e+5", value_kind::number},
      strict_accept_case{"MinusExponent", "-1.5e-5", value_kind::number},
      strict_accept_case{"ZeroExponentDigits", "2e00", value_kind::number},
      strict_accept_case{"RawDelete", "\"\x7F\"", value_kind::string},
      strict_accept_case{"RawLineSeparator", "\"\xE2\x80\xA8\"", value_kind::string},
      strict_accept_case{"RawNonAscii", "\"\xC3\xA9\xF0\x9F\x98\x80\"", value_kind::string},
      strict_accept_case{"AllWhitespace", " \t\r\n[ \t\r\n1 \t\r\n, \t\r\n2 \t\r\n] \t\r\n", value_kind::array},
      strict_accept_case{"WhitespaceAroundColon", "{ \"a\" \t:\n1 }", value_kind::object}),
    case_name<strict_accept_case>);
  /* ================================= End Test_J5_005 ================================= */
  /* =================================== Test_J5_015 =================================== */
  /*
   * Verify every JSON escape decodes to its exact UTF-16 unit in strict mode.
   * Arrange: One string holding all eight simple escapes, a BMP \u escape and a surrogate-pair escape.
   * Act: Parse it in strict mode.
   * Expect: The decoded units match exactly, including both surrogate halves.
   */
  TEST(J5_015_StrictEscapes, DecodesEveryJsonEscape)
  {
    /* Arrange */
    const std::string input{"\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0041\\uD83D\\uDE00\""};
    const std::vector<char16_t> expected{u'"', u'\\', u'/', u'\b', u'\f', u'\n', u'\r', u'\t', u'A', 0xD83D, 0xDE00};
    document_provider provider;
    const parse_options options{syntax_mode::strict_json};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const auto text = parsed.value_if()->values.root().value_if()->as_string();
    ASSERT_TRUE(text.has_value());
    const decoded_string& decoded = **text.value_if();
    ASSERT_EQ(decoded.size(), expected.size());
    for (std::size_t index = 0U; index < expected.size(); ++index)
    {
      EXPECT_EQ(*decoded.unit_at(index).value_if(), expected[index]) << "unit " << index;
    }
  }
  /* ================================= End Test_J5_015 ================================= */
  /* =================================== Test_J5_021 =================================== */
  /**
   * @brief One input rejected by strict mode at an exact offset.
   * @par Requirements
   * P042; J5-014; J5-021; J5-022.
   */
  struct strict_reject_case
  {
      const char* name;         // Case identifier, naming the rejected form.
      std::string input;        // Complete input bytes.
      bool json5_accepts;       // Expected JSON5 outcome for the same bytes.
      std::size_t strict_error; // Expected strict syntax error byte offset.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const strict_reject_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_021_StrictReject : public testing::TestWithParam<strict_reject_case>
  {
  };
  /*
   * Verify JSON-only rejections and remaining mode pairs at the documented offset.
   * Arrange: Malformed numbers, signed non-finite forms and raw control characters in strings.
   * Act: Parse the same bytes in strict mode and in JSON5 mode.
   * Expect: Strict syntax error at the offset with full release; JSON5 matches its own grammar.
   */
  TEST_P(J5_021_StrictReject, RejectsAtDocumentedOffset)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    const input_view input{fixture.input.data(), fixture.input.size()};
    document_provider strict_provider;
    document_provider json5_provider;
    const parse_options strict_options{syntax_mode::strict_json};
    const parse_options json5_options{syntax_mode::json5};
    accounted_storage strict_accounting{strict_provider.callbacks(), strict_options.limits.max_owned_bytes};
    accounted_storage json5_accounting{json5_provider.callbacks(), json5_options.limits.max_owned_bytes};
    /* Act */
    const auto strict = parse_document(input, strict_options, strict_accounting);
    const auto json5 = parse_document(input, json5_options, json5_accounting);
    /* Expect */
    ASSERT_FALSE(strict.has_value());
    EXPECT_EQ(strict.error_if()->code, error_code::syntax);
    EXPECT_EQ(strict.error_if()->byte_offset, fixture.strict_error);
    expect_released(strict_provider, strict_accounting, fixture.name);
    EXPECT_EQ(json5.has_value(), fixture.json5_accepts);
  }
  INSTANTIATE_TEST_SUITE_P(
    P042, J5_021_StrictReject,
    testing::Values(
      // Number errors shared with JSON5 report the offending character.
      strict_reject_case{"LeadingZero", "01", false, 1}, strict_reject_case{"NegativeLeadingZero", "-01", false, 2},
      strict_reject_case{"LoneMinus", "-", false, 1}, strict_reject_case{"MissingExponentDigits", "1e", false, 2},
      strict_reject_case{"SignedExponentWithoutDigits", "1e+", false, 3},
      strict_reject_case{"FractionWithoutDigitsBeforeExponent", "1.e5", true, 2},
      // Signed non-finite forms report the token start, the sign.
      strict_reject_case{"PlusInfinity", "+Infinity", true, 0}, strict_reject_case{"MinusNaN", "-NaN", true, 0},
      strict_reject_case{"PlusNaN", "+NaN", true, 0},
      // JSON forbids raw U+0000-U+001F in strings; JSON5 forbids only line terminators.
      strict_reject_case{"RawTab", "\"a\tb\"", true, 2},
      strict_reject_case{"RawNul", std::string{"\"\0\"", 3}, true, 1},
      strict_reject_case{"RawUnitSeparator", "\"\x1F\"", true, 1},
      strict_reject_case{"RawControlInName", "{\"a\x01\":1}", true, 3}),
    case_name<strict_reject_case>);
  /* ================================= End Test_J5_021 ================================= */
  /* =================================== Test_J5_025 =================================== */
  /**
   * @brief One BOM placement under a mode and strict BOM policy.
   * @par Requirements
   * P042; J5-025; J5-026.
   */
  struct strict_bom_case
  {
      const char* name;         // Case identifier.
      std::string input;        // Input bytes; EF BB BF is U+FEFF.
      syntax_mode mode;         // Input syntax selection.
      strict_bom_policy policy; // Input strict-mode BOM handling.
      bool accepted;            // Expected parse success.
      std::size_t offset;       // Expected root byte offset on success, or syntax error byte offset.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const strict_bom_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_025_StrictBom : public testing::TestWithParam<strict_bom_case>
  {
  };
  /*
   * Verify the BOM decisions accepted for P042.
   * Arrange: A BOM followed by whitespace only, and JSON5 inputs under both strict_bom policies.
   * Act: Parse with the selected mode and policy.
   * Expect: Strict skip_one leaves an empty document that fails at the end of input; JSON5 ignores the
   *         policy, so skip_one and reject produce identical outcomes.
   */
  TEST_P(J5_025_StrictBom, AppliesAcceptedEdgeDecisions)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    document_provider provider;
    parse_options options{fixture.mode};
    options.strict_bom = fixture.policy;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_document({fixture.input.data(), fixture.input.size()}, options, accounting);
    /* Expect */
    ASSERT_EQ(parsed.has_value(), fixture.accepted);
    if (fixture.accepted)
    {
      const auto span = parsed.value_if()->values.root().value_if()->source_span();
      ASSERT_TRUE(span.has_value() && span.value_if()->has_value());
      EXPECT_EQ((**span.value_if()).byte_offset, fixture.offset);
    }
    else
    {
      EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
      EXPECT_EQ(parsed.error_if()->byte_offset, fixture.offset);
      expect_released(provider, accounting, fixture.name);
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    P042, J5_025_StrictBom,
    testing::Values(
      strict_bom_case{"StrictSkipThenWhitespaceOnly", "\xEF\xBB\xBF \t\r\n", syntax_mode::strict_json,
                      strict_bom_policy::skip_one, false, 7},
      strict_bom_case{"StrictRejectBeforeWhitespaceOnly", "\xEF\xBB\xBF \t\r\n", syntax_mode::strict_json,
                      strict_bom_policy::reject, false, 0},
      strict_bom_case{"Json5SkipOneLeadingBom", "\xEF\xBB\xBF[1]", syntax_mode::json5, strict_bom_policy::skip_one,
                      true, 3},
      strict_bom_case{"Json5RejectLeadingBom", "\xEF\xBB\xBF[1]", syntax_mode::json5, strict_bom_policy::reject, true,
                      3},
      strict_bom_case{"Json5SkipOneBomOnly", "\xEF\xBB\xBF", syntax_mode::json5, strict_bom_policy::skip_one, false, 3},
      strict_bom_case{"Json5RejectBomOnly", "\xEF\xBB\xBF", syntax_mode::json5, strict_bom_policy::reject, false, 3}),
    case_name<strict_bom_case>);
  /* ================================= End Test_J5_025 ================================= */
  /* =================================== Test_J5_098 =================================== */
  /*
   * Verify every reachable allocation failure in a strict parse is reported and fully cleaned up.
   * Arrange: A strict document with names, escapes, numbers and nesting.
   * Act: Fail the first, second, ... provider request until a parse succeeds.
   * Expect: Each failure is allocation_failure with everything released; the final parse succeeds.
   */
  TEST(J5_098_StrictFaultSweep, ReleasesEverythingAtEveryAllocationFailure)
  {
    const std::string input{"{\"a\":\"x\\u00e9\\n\",\"b\":[1,-2.5e3,{\"c\":\"\xE2\x80\xA8\"}],\"d\":{\"e\":[]}}"};
    const parse_options options{syntax_mode::strict_json};
    std::size_t failures = 0U;
    bool succeeded = false;
    for (std::size_t fail_on = 1U; (fail_on <= 1024U) && !succeeded; ++fail_on)
    {
      /* Arrange */
      document_provider provider;
      provider.m_fail_on = fail_on;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
      /* Expect */
      if (parsed.has_value())
      {
        succeeded = true;
      }
      else
      {
        ++failures;
        EXPECT_EQ(parsed.error_if()->code, error_code::allocation_failure) << "fail_on " << fail_on;
        expect_released(provider, accounting, "StrictFaultSweep");
      }
    }
    EXPECT_TRUE(succeeded);
    // The document needs several allocations, so the sweep must have injected more than one failure.
    EXPECT_GT(failures, 1U);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Verify strict mode honors a configured limit inclusively and recovers after the failure.
   * Arrange: A strict document and an input byte limit at its exact size and one byte below.
   * Act: Parse with each allowance using the same accounting.
   * Expect: One byte below fails with resource_limit and nothing retained; the exact size succeeds.
   */
  TEST(J5_098_StrictLimit, EnforcesInclusiveInputBoundary)
  {
    /* Arrange */
    const std::string input{"{\"a\":[1,2]}"};
    document_provider provider;
    parse_options options{syntax_mode::strict_json};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    options.limits.max_input_bytes = input.size() - 1U;
    /* Act */
    const auto below = parse_document({input.data(), input.size()}, options, accounting);
    options.limits.max_input_bytes = input.size();
    const auto exact = parse_document({input.data(), input.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(below.has_value());
    EXPECT_EQ(below.error_if()->code, error_code::resource_limit);
    EXPECT_TRUE(exact.has_value());
  }
  /* ================================= End Test_J5_098 ================================= */
} // namespace
