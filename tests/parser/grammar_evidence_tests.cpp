#include "../support/document_provider.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/value/number_comparison.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <ostream>
#include <string>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;
  /* ==================================== Fixtures ===================================== */
  /**
   * @brief Check that a failed parse published nothing and released every allocation.
   * @par Requirements
   * P040; J5-051; J5-098.
   */
  void expect_released(const document_provider& provider, const accounted_storage& accounting, const char* label)
  {
    EXPECT_EQ(accounting.current_bytes(), 0U) << label;
    EXPECT_EQ(provider.m_live_bytes, 0U) << label;
    EXPECT_EQ(provider.m_allocations, provider.m_releases) << label;
  }

  /**
   * @brief Compare two parsed values structurally and record every difference as a failure.
   * @par Requirements
   * P040; J5-097.
   * @note Numbers compare by exact mathematical value; strings and member names by decoded UTF-16 units.
   * Source spans are deliberately ignored, because equivalent spellings differ in length.
   */
  void expect_same_value(const value_view& left, const value_view& right, const std::string& path)
  {
    const auto left_kind = left.kind();
    const auto right_kind = right.kind();
    ASSERT_TRUE(left_kind.has_value() && right_kind.has_value()) << path;
    ASSERT_EQ(*left_kind.value_if(), *right_kind.value_if()) << path;
    switch (*left_kind.value_if())
    {
    case value_kind::null:
      break;
    case value_kind::boolean:
      EXPECT_EQ(*left.as_boolean().value_if(), *right.as_boolean().value_if()) << path;
      break;
    case value_kind::number:
    {
      document_provider scratch;
      const auto order =
        compare_numbers(**left.as_number().value_if(), **right.as_number().value_if(), scratch.callbacks());
      ASSERT_TRUE(order.has_value()) << path;
      EXPECT_EQ(*order.value_if(), number_order::equal) << path;
      break;
    }
    case value_kind::string:
    {
      const decoded_string& a = **left.as_string().value_if();
      const decoded_string& b = **right.as_string().value_if();
      ASSERT_EQ(a.size(), b.size()) << path;
      for (std::size_t i = 0U; i < a.size(); ++i)
      {
        EXPECT_EQ(*a.unit_at(i).value_if(), *b.unit_at(i).value_if()) << path << " unit " << i;
      }
      break;
    }
    case value_kind::array:
    case value_kind::object:
    {
      const bool object = (*left_kind.value_if() == value_kind::object);
      const auto count = left.container_size();
      ASSERT_TRUE(count.has_value()) << path;
      ASSERT_EQ(*count.value_if(), *right.container_size().value_if()) << path;
      for (std::size_t i = 0U; i < *count.value_if(); ++i)
      {
        const auto a = object ? left.member_at(i) : left.element_at(i);
        const auto b = object ? right.member_at(i) : right.element_at(i);
        ASSERT_TRUE(a.has_value() && b.has_value()) << path << '/' << i;
        if (object)
        {
          const decoded_string& a_name = **a.value_if()->member_name().value_if();
          const decoded_string& b_name = **b.value_if()->member_name().value_if();
          ASSERT_EQ(a_name.size(), b_name.size()) << path << '/' << i;
          for (std::size_t u = 0U; u < a_name.size(); ++u)
          {
            EXPECT_EQ(*a_name.unit_at(u).value_if(), *b_name.unit_at(u).value_if()) << path << '/' << i;
          }
        }
        expect_same_value(*a.value_if(), *b.value_if(), path + '/' + std::to_string(i));
      }
      break;
    }
    }
  }

  /**
   * @brief Name parameterized cases after their name field.
   * @par Requirements
   * P040.
   */
  template <typename T>
  std::string case_name(const testing::TestParamInfo<T>& info)
  {
    return info.param.name;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_025 =================================== */
  /**
   * @brief One leading or misplaced byte order mark under a mode and strict BOM policy.
   * @par Requirements
   * P040; J5-025.
   */
  struct bom_case
  {
      const char* name;         // Case identifier.
      std::string input;        // Input bytes; EF BB BF is U+FEFF.
      syntax_mode mode;         // Input syntax selection.
      strict_bom_policy policy; // Input strict-mode BOM handling.
      bool accepted;            // Expected parse success.
      std::size_t offset;       // Expected root byte offset on success, or error byte offset.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const bom_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_025_DocumentBom : public testing::TestWithParam<bom_case>
  {
  };
  /*
   * Verify the BOM policy through the public entry in both modes.
   * Arrange: Inputs with a BOM at the start, twice, after whitespace and inside a container.
   * Act: Parse with the selected mode and strict_bom policy.
   * Expect: Strict mode rejects by default and skips exactly one leading BOM with skip_one, keeping
   *         original offsets; JSON5 treats every BOM as whitespace regardless of the policy.
   */
  TEST_P(J5_025_DocumentBom, AppliesPolicyAndKeepsOriginalOffsets)
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
  INSTANTIATE_TEST_SUITE_P(P040, J5_025_DocumentBom,
                           testing::Values(bom_case{"StrictDefaultRejects", "\xEF\xBB\xBF[1]", syntax_mode::strict_json,
                                                    strict_bom_policy::reject, false, 0},
                                           bom_case{"StrictSkipsOne", "\xEF\xBB\xBF[1]", syntax_mode::strict_json,
                                                    strict_bom_policy::skip_one, true, 3},
                                           bom_case{"StrictSkipThenSpace", "\xEF\xBB\xBF [1]", syntax_mode::strict_json,
                                                    strict_bom_policy::skip_one, true, 4},
                                           bom_case{"StrictSecondBomRejected", "\xEF\xBB\xBF\xEF\xBB\xBF[1]",
                                                    syntax_mode::strict_json, strict_bom_policy::skip_one, false, 3},
                                           bom_case{"StrictBomAfterSpaceRejected", " \xEF\xBB\xBF[1]",
                                                    syntax_mode::strict_json, strict_bom_policy::skip_one, false, 1},
                                           bom_case{"StrictInteriorBomRejected",
                                                    "[\xEF\xBB\xBF"
                                                    "1]",
                                                    syntax_mode::strict_json, strict_bom_policy::skip_one, false, 1},
                                           bom_case{"StrictBomOnlyHasNoValue", "\xEF\xBB\xBF", syntax_mode::strict_json,
                                                    strict_bom_policy::skip_one, false, 3},
                                           bom_case{"Json5LeadingBom", "\xEF\xBB\xBF[1]", syntax_mode::json5,
                                                    strict_bom_policy::reject, true, 3},
                                           bom_case{"Json5RepeatedBoms", "\xEF\xBB\xBF\xEF\xBB\xBF[1]",
                                                    syntax_mode::json5, strict_bom_policy::skip_one, true, 6},
                                           bom_case{"Json5InteriorBom",
                                                    "[\xEF\xBB\xBF"
                                                    "1]",
                                                    syntax_mode::json5, strict_bom_policy::reject, true, 0}),
                           case_name<bom_case>);
  /* ================================= End Test_J5_025 ================================= */
  /* =================================== Test_J5_027 =================================== */
  /**
   * @brief One JSON5-only feature, valid in JSON5 mode and rejected in strict mode.
   * @par Requirements
   * P040; J5-027.
   */
  struct mode_pair_case
  {
      const char* name;          // Case identifier, naming the JSON5-only production.
      std::string input;         // Complete input bytes.
      std::size_t strict_offset; // Expected strict error offset: number token start, escaped character or terminator.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const mode_pair_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_027_ModePair : public testing::TestWithParam<mode_pair_case>
  {
  };
  /*
   * Verify every JSON5 extension is accepted in JSON5 mode and rejected by strict mode at the feature.
   * Arrange: One minimal document per JSON5-only production.
   * Act: Parse the same bytes in JSON5 and in strict mode.
   * Expect: JSON5 success; strict syntax error at the feature's first byte and full release.
   */
  TEST_P(J5_027_ModePair, AcceptsJson5AndRejectsStrict)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    document_provider json5_provider;
    document_provider strict_provider;
    const parse_options json5_options{syntax_mode::json5};
    const parse_options strict_options{syntax_mode::strict_json};
    accounted_storage json5_accounting{json5_provider.callbacks(), json5_options.limits.max_owned_bytes};
    accounted_storage strict_accounting{strict_provider.callbacks(), strict_options.limits.max_owned_bytes};
    const input_view input{fixture.input.data(), fixture.input.size()};
    /* Act */
    const auto json5 = parse_document(input, json5_options, json5_accounting);
    const auto strict = parse_document(input, strict_options, strict_accounting);
    /* Expect */
    EXPECT_TRUE(json5.has_value());
    ASSERT_FALSE(strict.has_value());
    EXPECT_EQ(strict.error_if()->code, error_code::syntax);
    EXPECT_EQ(strict.error_if()->byte_offset, fixture.strict_offset);
    expect_released(strict_provider, strict_accounting, fixture.name);
  }
  INSTANTIATE_TEST_SUITE_P(
    P040, J5_027_ModePair,
    testing::Values(
      mode_pair_case{"LineComment", "1//c", 1}, mode_pair_case{"BlockComment", "/*c*/1", 0},
      mode_pair_case{"ArrayTrailingComma", "[1,]", 3}, mode_pair_case{"ObjectTrailingComma", "{\"a\":1,}", 7},
      mode_pair_case{"IdentifierName", "{a:1}", 1}, mode_pair_case{"EscapedIdentifierName", "{\\u0061:1}", 1},
      mode_pair_case{"SingleQuotedString", "'a'", 0}, mode_pair_case{"SingleQuotedName", "{'a':1}", 1},
      mode_pair_case{"HexNumber", "0x1F", 0}, mode_pair_case{"PlusSign", "+1", 0},
      mode_pair_case{"LeadingPoint", ".5", 0}, mode_pair_case{"TrailingPoint", "5.", 2},
      mode_pair_case{"Infinity", "Infinity", 0}, mode_pair_case{"NegativeInfinity", "-Infinity", 0},
      mode_pair_case{"NaN", "NaN", 0}, mode_pair_case{"HexEscape", "\"\\x41\"", 2},
      mode_pair_case{"VerticalTabEscape", "\"\\v\"", 2}, mode_pair_case{"SingleQuoteEscape", "\"\\'\"", 2},
      mode_pair_case{"ZeroEscape", "\"\\0\"", 2}, mode_pair_case{"NonEscapeCharacter", "\"\\q\"", 2},
      mode_pair_case{"LineContinuation", "\"a\\\nb\"", 3}, mode_pair_case{"VerticalTabSpace", "\v1", 0},
      mode_pair_case{"FormFeedSpace", "\f1", 0},
      mode_pair_case{"NoBreakSpace",
                     "\xC2\xA0"
                     "1",
                     0},
      mode_pair_case{"LineSeparatorSpace",
                     "\xE2\x80\xA8"
                     "1",
                     0},
      mode_pair_case{"ParagraphSeparatorSpace", "1\xE2\x80\xA9", 1},
      mode_pair_case{"InteriorBomSpace", "1\xEF\xBB\xBF", 1},
      mode_pair_case{"ModernZsSpace",
                     "\xE3\x80\x80"
                     "1",
                     0},
      // U+200B was Zs in Unicode 3.0 and remains JSON5 whitespace by design.
      mode_pair_case{"LegacyZeroWidthSpace",
                     "\xE2\x80\x8B"
                     "1",
                     0}),
    case_name<mode_pair_case>);
  /* ================================= End Test_J5_027 ================================= */
  /* =================================== Test_J5_009 =================================== */
  /**
   * @brief One malformed UTF-8 sequence inside a complete document.
   * @par Requirements
   * P040; J5-009.
   */
  struct encoding_case
  {
      const char* name;   // Case identifier.
      std::string input;  // Complete input bytes containing malformed UTF-8.
      std::size_t offset; // Expected byte offset of the first invalid sequence.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const encoding_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_009_DocumentEncoding : public testing::TestWithParam<encoding_case>
  {
  };
  /*
   * Verify malformed UTF-8 is an encoding error in both modes wherever it appears.
   * Arrange: Invalid bytes in strings, names, trivia and value position.
   * Act: Parse in JSON5 and strict mode.
   * Expect: encoding at the original offset of the invalid sequence and full release in each mode.
   */
  TEST_P(J5_009_DocumentEncoding, ReportsOriginalOffsetInBothModes)
  {
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      /* Arrange */
      const auto& fixture = GetParam();
      document_provider provider;
      const parse_options options{mode};
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      const auto parsed = parse_document({fixture.input.data(), fixture.input.size()}, options, accounting);
      /* Expect */
      ASSERT_FALSE(parsed.has_value());
      EXPECT_EQ(parsed.error_if()->code, error_code::encoding);
      EXPECT_EQ(parsed.error_if()->byte_offset, fixture.offset);
      expect_released(provider, accounting, fixture.name);
    }
  }
  INSTANTIATE_TEST_SUITE_P(P040, J5_009_DocumentEncoding,
                           testing::Values(encoding_case{"InvalidByteInString", "[\"\xFF\"]", 2},
                                           encoding_case{"TruncatedInString", "\"\xE2\x82\"", 1},
                                           encoding_case{"OverlongInString", "\"\xC0\x80\"", 1},
                                           encoding_case{"SurrogateInString", "\"\xED\xA0\x80\"", 1},
                                           encoding_case{"AboveMaxInString", "\"\xF4\x90\x80\x80\"", 1},
                                           encoding_case{"InvalidByteInName", "{\"a\xFF\":1}", 3},
                                           encoding_case{"InvalidByteInTrivia", "[1, \xFF]", 4},
                                           encoding_case{"InvalidValueStart", "\xFF", 0}),
                           case_name<encoding_case>);
  /* ================================= End Test_J5_009 ================================= */
  /* =================================== Test_J5_097 =================================== */
  /**
   * @brief One JSON5 spelling of the reference document.
   * @par Requirements
   * P040; J5-097.
   */
  struct equivalence_case
  {
      const char* name;  // Case identifier, naming the transformation.
      std::string input; // JSON5 spelling that must decode to the reference document.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const equivalence_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  // Reference: valid strict JSON, also parsed in JSON5 mode.
  constexpr std::string_view c_reference{R"({"name":"Ab","list":[1,true,null,"\u00e9"],"n":16,"o":{}})"};
  class J5_097_Equivalence : public testing::TestWithParam<equivalence_case>
  {
  };
  /*
   * Verify equivalent spellings decode to identical documents.
   * Arrange: The strict reference, and a JSON5 spelling changing trivia, quotes, escapes, names or numbers.
   * Act: Parse the reference in strict mode and the spelling in JSON5 mode.
   * Expect: Same kinds, order, member names, decoded strings and exact numeric values.
   */
  TEST_P(J5_097_Equivalence, DecodesToReferenceDocument)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    document_provider reference_provider;
    document_provider provider;
    const parse_options strict_options{syntax_mode::strict_json};
    const parse_options json5_options{syntax_mode::json5};
    accounted_storage reference_accounting{reference_provider.callbacks(), strict_options.limits.max_owned_bytes};
    accounted_storage accounting{provider.callbacks(), json5_options.limits.max_owned_bytes};
    /* Act */
    const auto reference =
      parse_document({c_reference.data(), c_reference.size()}, strict_options, reference_accounting);
    const auto variant = parse_document({fixture.input.data(), fixture.input.size()}, json5_options, accounting);
    /* Expect */
    ASSERT_TRUE(reference.has_value());
    ASSERT_TRUE(variant.has_value()) << fixture.name << " error at "
                                     << (variant.has_value() ? 0U : variant.error_if()->byte_offset);
    expect_same_value(*reference.value_if()->values.root().value_if(), *variant.value_if()->values.root().value_if(),
                      fixture.name);
  }
  INSTANTIATE_TEST_SUITE_P(
    P040, J5_097_Equivalence,
    testing::Values(
      equivalence_case{"ReferenceInJson5", std::string{c_reference}},
      equivalence_case{"UnquotedNamesSingleQuotes", "{name:'Ab',list:[1,true,null,'\xC3\xA9'],n:16,o:{}}"},
      equivalence_case{"TrailingCommas", "{\"name\":\"Ab\",\"list\":[1,true,null,\"\xC3\xA9\",],\"n\":16,\"o\":{},}"},
      equivalence_case{"CommentsAndWhitespace",
                       "/*h*/ {\n  \"name\" : \"Ab\", // c\n  \"list\" : [ 1 , true , null , \"\xC3\xA9\" ] ,\r\n"
                       "  \"n\"\t: 16 , \"o\" : { /*e*/ } }\n// end"},
      equivalence_case{"UnicodeWhitespace",
                       "\xEF\xBB\xBF{\xC2\xA0\"name\":\xE2\x80\xA8\"Ab\",\"list\":[1,true,null,\"\xC3\xA9\"],"
                       "\"n\":16,\"o\":{}\xE3\x80\x80}"},
      equivalence_case{"StringEscapes", "{\"\\u006eame\":\"\\x41\\u0062\",\"list\":[1,true,null,\"\\xE9\"],"
                                        "\"n\":16,\"o\":{}}"},
      equivalence_case{"IdentifierEscape", "{\\u006eame:\"Ab\",list:[1,true,null,\"\\u00E9\"],n:16,o:{}}"},
      equivalence_case{"LineContinuation", "{\"name\":\"A\\\nb\",\"list\":[1,true,null,\"\\\r\n\xC3\xA9\"],"
                                           "\"n\":16,\"o\":{}}"},
      equivalence_case{"NonEscapeCharacters", "{\"name\":\"\\Ab\",\"list\":[1,true,null,\"\\\xC3\xA9\"],"
                                              "\"n\":16,\"o\":{}}"},
      equivalence_case{"NumberSpellings",
                       "{\"name\":\"Ab\",\"list\":[+1.0,true,null,\"\xC3\xA9\"],\"n\":0x10,\"o\":{}}"},
      equivalence_case{"ExponentSpellings", "{\"name\":\"Ab\",\"list\":[10e-1,true,null,\"\xC3\xA9\"],\"n\":1.6E+1,"
                                            "\"o\":{}}"}),
    case_name<equivalence_case>);
  /* ================================= End Test_J5_097 ================================= */
  /* =================================== Test_J5_094 =================================== */
  /**
   * @brief A representative document whose every proper prefix is incomplete.
   * @par Requirements
   * P040; J5-094.
   */
  struct truncation_case
  {
      const char* name;  // Case identifier.
      std::string input; // Complete valid document; the object root makes every proper prefix invalid.
      syntax_mode mode;  // Input syntax selection.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const truncation_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_094_DocumentTruncation : public testing::TestWithParam<truncation_case>
  {
  };
  /*
   * Verify every proper prefix of a document using every production is rejected cleanly.
   * Arrange: A complete document, valid in its mode, covering comments, strings, escapes, names and numbers.
   * Act: Parse the full document, then each prefix of length 0 to size - 1.
   * Expect: The full document succeeds; every prefix fails with syntax or encoding at or before its end,
   *         with every allocation released.
   */
  TEST_P(J5_094_DocumentTruncation, RejectsEveryProperPrefix)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    const parse_options options{fixture.mode};
    {
      document_provider provider;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      ASSERT_TRUE(parse_document({fixture.input.data(), fixture.input.size()}, options, accounting).has_value());
    }
    for (std::size_t length = 0U; length < fixture.input.size(); ++length)
    {
      document_provider provider;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      const auto parsed = parse_document({fixture.input.data(), length}, options, accounting);
      /* Expect */
      ASSERT_FALSE(parsed.has_value()) << "prefix " << length;
      const error_code code = parsed.error_if()->code;
      EXPECT_TRUE((code == error_code::syntax) || (code == error_code::encoding)) << "prefix " << length;
      EXPECT_LE(parsed.error_if()->byte_offset, length) << "prefix " << length;
      expect_released(provider, accounting, fixture.name);
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    P040, J5_094_DocumentTruncation,
    testing::Values(
      truncation_case{"Json5",
                      "{/*c*/a:'x\\x41\\u00e9\xC3\xA9',\\u0062:[+0x1F,-.5e-2,Infinity,NaN,true,null,],\"c\\\n\":{},}",
                      syntax_mode::json5},
      truncation_case{"Strict", "{\"a\":\"x\\u00e9\xC3\xA9\\n\",\"b\":[-1.5e+2,0,true,false,null],\"c\":{}}",
                      syntax_mode::strict_json}),
    case_name<truncation_case>);
  /* ================================= End Test_J5_094 ================================= */
  /* =================================== Test_J5_098 =================================== */
  /**
   * @brief A representative document parsed under one retention setting.
   * @par Requirements
   * P040; J5-098.
   */
  struct fault_case
  {
      const char* name;                  // Case identifier.
      std::string input;                 // Complete valid JSON5 document.
      source_retention_policy retention; // Input source retention selection.
  };
  // Print the case name in failure output instead of raw bytes.
  void PrintTo(const fault_case& param, std::ostream* stream)
  {
    *stream << param.name;
  }
  class J5_098_DocumentFaultSweep : public testing::TestWithParam<fault_case>
  {
  };
  /*
   * Verify every reachable allocation failure in a whole parse is reported and fully cleaned up.
   * Arrange: A document with names, strings, warnings, numbers and nesting, with or without source retention.
   * Act: Fail the first, second, ... provider request until a parse succeeds.
   * Expect: Each failure is allocation_failure with no document and nothing allocated; the final parse succeeds.
   */
  TEST_P(J5_098_DocumentFaultSweep, ReleasesEverythingAtEveryAllocationFailure)
  {
    const auto& fixture = GetParam();
    parse_options options{syntax_mode::json5};
    options.source_text = fixture.retention;
    std::size_t failures = 0U;
    bool succeeded = false;
    for (std::size_t fail_on = 1U; (fail_on <= 1024U) && !succeeded; ++fail_on)
    {
      /* Arrange */
      document_provider provider;
      provider.m_fail_on = fail_on;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      /* Act */
      const auto parsed = parse_document({fixture.input.data(), fixture.input.size()}, options, accounting);
      /* Expect */
      if (parsed.has_value())
      {
        succeeded = true;
        EXPECT_EQ(parsed.value_if()->warnings.size(), 1U);
        EXPECT_EQ(parsed.value_if()->source.is_retained(), fixture.retention == source_retention_policy::retain_copy);
      }
      else
      {
        ++failures;
        EXPECT_EQ(parsed.error_if()->code, error_code::allocation_failure) << "fail_on " << fail_on;
        expect_released(provider, accounting, fixture.name);
      }
    }
    EXPECT_TRUE(succeeded);
    // The document needs several allocations, so the sweep must have injected more than one failure.
    EXPECT_GT(failures, 1U);
  }
  INSTANTIATE_TEST_SUITE_P(
    P040, J5_098_DocumentFaultSweep,
    testing::Values(fault_case{"Discard", "{a:'x\\u00e9',b:[1,0x2,{c:\"\xE2\x80\xA8\"}],'d':{e:[]},f:-Infinity}",
                               source_retention_policy::discard},
                    fault_case{"RetainCopy", "{a:'x\\u00e9',b:[1,0x2,{c:\"\xE2\x80\xA8\"}],'d':{e:[]},f:-Infinity}",
                               source_retention_policy::retain_copy}),
    case_name<fault_case>);
  /* ================================= End Test_J5_098 ================================= */
} // namespace
