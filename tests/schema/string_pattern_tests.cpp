#include "adaptive_json5/schema.hpp"
#include "schema_run.hpp"

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::run_schema;
  using adaptive_json5_test::schema_verdict;

  struct verdict_case
  {
      const char* name;
      const char* schema;
      const char* instance;
      bool valid;
  };

  /* =================================== Test_P049_Length ==================================== */
  class P049_Length : public testing::TestWithParam<verdict_case>
  {
  };

  /*
   * Arrange: Strings with ASCII, multi-byte, astral, escaped and NUL characters at the bounds.
   * Act: Validate minLength/maxLength.
   * Expect: Length counts Unicode code points, not UTF-8 bytes or UTF-16 units (J5-060).
   */
  TEST_P(P049_Length, CountsCodePoints)
  {
    EXPECT_EQ(schema_verdict(GetParam().schema, GetParam().instance), std::optional<bool>{GetParam().valid})
      << GetParam().schema << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P049_Length,
    testing::Values(verdict_case{"min_equal", R"({"minLength": 2})", R"("ab")", true},
                    verdict_case{"min_below", R"({"minLength": 2})", R"("a")", false},
                    verdict_case{"max_equal", R"({"maxLength": 2})", R"("ab")", true},
                    verdict_case{"max_above", R"({"maxLength": 2})", R"("abc")", false},
                    verdict_case{"multibyte_is_one", R"({"maxLength": 1})", R"("é")", true},
                    verdict_case{"astral_is_one", R"({"maxLength": 1})", R"("😀")", true},
                    verdict_case{"astral_escape_is_one", R"({"maxLength": 1})", R"("😀")", true},
                    verdict_case{"astral_pair_two", R"({"minLength": 2})", R"("😀😀")", true},
                    verdict_case{"combining_counts_twice", R"({"maxLength": 1})", R"("é")", false},
                    verdict_case{"nul_counts", R"({"minLength": 1})", R"("\u0000")", true},
                    verdict_case{"empty_min_zero", R"({"minLength": 0})", R"("")", true},
                    verdict_case{"integer_spelled_float", R"({"maxLength": 2.0})", R"("ab")", true},
                    verdict_case{"huge_max", R"({"maxLength": 100000000000000000000000})", R"("ab")", true},
                    verdict_case{"not_a_string", R"({"maxLength": 0})", "12345", true}),
    [](const testing::TestParamInfo<verdict_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: Length keywords that are negative, fractional or not numbers.
   * Act: Compile.
   * Expect: invalid_schema (keyword_shape).
   */
  TEST(P049_LengthShape, RejectsMalformedLengths)
  {
    for (const char* schema : {R"({"minLength": -1})", R"({"maxLength": 1.5})", R"({"minLength": "2"})"})
    {
      const auto outcome = run_schema(schema, R"("x")");
      EXPECT_EQ(outcome.reason, std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape}) << schema;
    }
  }

  /* =================================== Test_P049_Pattern ==================================== */
  struct pattern_case
  {
      const char* name;
      const char* pattern;  // JSON string contents (JSON escapes apply).
      const char* instance; // JSON string contents.
      bool matches;
  };

  class P049_Pattern : public testing::TestWithParam<pattern_case>
  {
  };

  /*
   * Arrange: Patterns covering every supported construct.
   * Act: Validate a string against {"pattern": ...}.
   * Expect: ECMA-262 semantics on code points; patterns are unanchored (J5-060).
   */
  TEST_P(P049_Pattern, MatchesEcma262)
  {
    const std::string schema = std::string{R"({"pattern": ")"} + GetParam().pattern + "\"}";
    const std::string instance = std::string{"\""} + GetParam().instance + "\"";
    EXPECT_EQ(schema_verdict(schema, instance), std::optional<bool>{GetParam().matches})
      << GetParam().pattern << " / " << GetParam().instance;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P049_Pattern,
    testing::Values(
      pattern_case{"unanchored", "b", "abc", true}, pattern_case{"literal_missing", "d", "abc", false},
      pattern_case{"anchored_start", "^b", "abc", false}, pattern_case{"anchored_both", "^abc$", "abc", true},
      pattern_case{"end_anchor", "c$", "abc", true}, pattern_case{"end_anchor_no", "b$", "abc", false},
      pattern_case{"empty_pattern", "", "anything", true}, pattern_case{"empty_text", "^$", "", true},
      pattern_case{"dot", "^a.c$", "abc", true}, pattern_case{"dot_newline", "^a.c$", "a\\nc", false},
      pattern_case{"dot_astral", "^.$", "😀", true}, pattern_case{"star", "^ab*c$", "ac", true},
      pattern_case{"plus", "^ab+c$", "ac", false}, pattern_case{"question", "^ab?c$", "abbc", false},
      pattern_case{"braces_exact", "^a{3}$", "aaa", true}, pattern_case{"braces_exact_no", "^a{3}$", "aa", false},
      pattern_case{"braces_range", "^a{2,3}$", "aaaa", false}, pattern_case{"braces_open", "^a{2,}$", "aaaaa", true},
      pattern_case{"braces_zero", "^ba{0}c$", "bc", true}, pattern_case{"lazy", "^a+?$", "aaa", true},
      pattern_case{"lazy_star", "a*?b", "aab", true}, pattern_case{"brace_literal", "a{x", "a{x", true},
      pattern_case{"brace_literal_unclosed", "^a{2$", "a{2", true}, pattern_case{"close_brace_literal", "}", "}", true},
      pattern_case{"bracket_literal", "]", "]", true}, pattern_case{"alternation", "^(cat|dog)$", "dog", true},
      pattern_case{"alternation_no", "^(cat|dog)$", "cow", false},
      pattern_case{"empty_alternative", "^(a|)$", "", true},
      pattern_case{"three_alternatives", "^(a|b|c)+$", "abcab", true},
      pattern_case{"non_capturing", "^(?:ab)+$", "abab", true},
      pattern_case{"nested_groups", "^((a)(b))+$", "abab", true}, pattern_case{"class", "^[abc]+$", "cab", true},
      pattern_case{"class_range", "^[a-f0-9]+$", "deadbeef42", true},
      pattern_case{"class_negated", "^[^0-9]+$", "abc1", false}, pattern_case{"class_empty", "[]", "a", false},
      pattern_case{"class_negated_empty", "^[^]$", "x", true}, pattern_case{"class_dash_end", "^[a-]+$", "a-a", true},
      pattern_case{"class_escape", "^[\\\\]]+$", "]]", true}, pattern_case{"class_digit", "^[\\\\d.]+$", "1.5", true},
      pattern_case{"class_not_digit", "^[\\\\D]+$", "a1", false},
      pattern_case{"class_shorthand_dash", "^[\\\\d-z]+$", "1-z", true},
      pattern_case{"class_backspace", "^[\\\\b]$", "\\b", true}, pattern_case{"digit", "^\\\\d+$", "123", true},
      pattern_case{"not_digit", "^\\\\D+$", "12", false}, pattern_case{"word", "^\\\\w+$", "a_1", true},
      pattern_case{"not_word", "\\\\W", "abc", false}, pattern_case{"space", "^\\\\s$", "\\u00a0", true},
      pattern_case{"not_space", "^\\\\S+$", "a b", false},
      pattern_case{"word_boundary", "\\\\bcat\\\\b", "a cat!", true},
      pattern_case{"word_boundary_no", "\\\\bcat\\\\b", "concatenate", false},
      pattern_case{"not_word_boundary", "\\\\Bcat\\\\B", "concatenate", true},
      pattern_case{"escaped_dot", "^a\\\\.b$", "axb", false}, pattern_case{"hex_escape", "^\\\\x41$", "A", true},
      pattern_case{"unicode_escape", "^\\\\u00e9$", "é", true}, pattern_case{"control_escape", "^\\\\t$", "\\t", true},
      pattern_case{"nul_escape", "^\\\\0$", "\\u0000", true}, pattern_case{"identity_escape", "^\\\\/$", "/", true},
      pattern_case{"astral_literal", "^😀+$", "😀😀", true}, pattern_case{"astral_class", "^[😀-😂]$", "😁", true},
      pattern_case{"nested_star", "^(a*)*b$", "aaab", true}, pattern_case{"nested_star_no", "^(a*)*b$", "aaaa", false},
      pattern_case{"pathological", "^(a|a)*b$", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaac", false},
      pattern_case{"control_upper", "^\\\\cJ$", "\\n", true}, pattern_case{"control_lower", "^\\\\cj$", "\\n", true},
      pattern_case{"control_annex_b", "^\\\\c1$", "\\\\c1", true},
      pattern_case{"control_at_end", "\\\\c$", "\\\\c", true},
      pattern_case{"control_class_digit", "^[\\\\c1]$", "\\u0011", true},
      pattern_case{"control_class_underscore", "^[\\\\c_]$", "\\u001f", true},
      pattern_case{"property_letter", "^\\\\p{L}+$", "école", true},
      pattern_case{"property_letter_no", "^\\\\p{L}$", "1", false},
      pattern_case{"property_long_name", "^\\\\p{Letter}cole$", "école", true},
      pattern_case{"property_digit", "^\\\\p{digit}+$", "٢٣", true},
      pattern_case{"property_negated", "^\\\\P{L}+$", "123", true},
      pattern_case{"property_negated_no", "^\\\\P{L}$", "a", false},
      pattern_case{"property_gc_form", "^\\\\p{gc=Lu}$", "A", true},
      pattern_case{"property_gc_form_no", "^\\\\p{General_Category=Lu}$", "a", false},
      pattern_case{"property_script", "^\\\\p{Script=Greek}+$", "αβ", true},
      pattern_case{"property_script_no", "^\\\\p{sc=Grek}$", "a", false},
      pattern_case{"property_in_class", "^[\\\\p{Nd}a]+$", "a1a", true},
      pattern_case{"property_negated_class", "^[^\\\\p{L}]$", "1", true},
      pattern_case{"property_negated_class_no", "^[^\\\\p{L}]$", "x", false},
      pattern_case{"property_any", "^\\\\p{Any}$", "😀", true},
      pattern_case{"property_not_any", "^\\\\P{Any}$", "a", false},
      pattern_case{"property_ascii", "^\\\\p{ASCII}+$", "abc", true},
      pattern_case{"property_ascii_no", "^\\\\p{ASCII}$", "é", false},
      pattern_case{"property_not_ascii", "^\\\\P{ASCII}$", "é", true},
      pattern_case{"property_assigned_no", "^\\\\p{Assigned}$", "\\u0378", false},
      pattern_case{"property_unassigned", "^\\\\p{Cn}$", "\\u0378", true},
      pattern_case{"property_other_group", "^\\\\p{C}$", "\\u0378", true},
      pattern_case{"property_unknown_script", "^\\\\p{sc=Zzzz}$", "\\u0378", true},
      pattern_case{"property_not_unknown_script", "^\\\\P{sc=Unknown}$", "a", true},
      pattern_case{"property_last_code_point", "^\\\\p{Co}$", "\\udbff\\udffd", true}),
    [](const testing::TestParamInfo<pattern_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: Malformed patterns and patterns using features the engine does not support.
   * Act: Compile.
   * Expect: invalid_schema with invalid_pattern or unsupported_pattern, never a silent pass.
   */
  TEST(P049_PatternErrors, ReportsMalformedAndUnsupported)
  {
    struct error_case
    {
        const char* pattern;
        invalid_schema_reason reason;
    };
    for (const error_case& entry :
         {error_case{"(", invalid_schema_reason::invalid_pattern},
          error_case{"a)", invalid_schema_reason::invalid_pattern},
          error_case{"[a", invalid_schema_reason::invalid_pattern},
          error_case{"*a", invalid_schema_reason::invalid_pattern},
          error_case{"a**", invalid_schema_reason::invalid_pattern},
          error_case{"^*", invalid_schema_reason::invalid_pattern},
          error_case{"a{3,2}", invalid_schema_reason::invalid_pattern},
          error_case{"[z-a]", invalid_schema_reason::invalid_pattern},
          error_case{"a\\\\", invalid_schema_reason::invalid_pattern},
          error_case{"(a)\\\\1", invalid_schema_reason::unsupported_pattern},
          error_case{"(?=a)", invalid_schema_reason::unsupported_pattern},
          error_case{"(?!a)", invalid_schema_reason::unsupported_pattern},
          error_case{"(?<=a)b", invalid_schema_reason::unsupported_pattern},
          error_case{"(?<name>a)", invalid_schema_reason::unsupported_pattern},
          error_case{"\\\\p{Bogus}", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\pL", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{L", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{}", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{é}", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{gc=Bogus}", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{sc=Bogus}", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{Foo=Bar}", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{scx=Bogus}", invalid_schema_reason::invalid_pattern},
          error_case{"\\\\p{Alphabetic}", invalid_schema_reason::unsupported_pattern},
          error_case{"\\\\p{White_Space}", invalid_schema_reason::unsupported_pattern},
          error_case{"\\\\p{scx=Greek}", invalid_schema_reason::unsupported_pattern},
          error_case{"\\\\p{Script_Extensions=Greek}", invalid_schema_reason::unsupported_pattern},
          error_case{"[\\\\B]", invalid_schema_reason::unsupported_pattern},
          error_case{"\\\\01", invalid_schema_reason::unsupported_pattern}})
    {
      const std::string schema = std::string{R"({"pattern": ")"} + entry.pattern + "\"}";
      const auto outcome = run_schema(schema, R"("x")");
      EXPECT_EQ(outcome.error, std::optional<schema_error_code>{schema_error_code::invalid_schema}) << entry.pattern;
      EXPECT_EQ(outcome.reason, std::optional<invalid_schema_reason>{entry.reason}) << entry.pattern;
    }
    EXPECT_EQ(run_schema(R"({"pattern": 5})", R"("x")").reason,
              std::optional<invalid_schema_reason>{invalid_schema_reason::keyword_shape});
    EXPECT_EQ(run_schema(R"({"pattern": "a{1000000}"})", R"("x")").error,
              std::optional<schema_error_code>{schema_error_code::resource_limit});
    const std::string long_name = R"({"pattern": "\\p{)" + std::string(70U, 'L') + "}\"}";
    EXPECT_EQ(run_schema(long_name, R"("x")").reason,
              std::optional<invalid_schema_reason>{invalid_schema_reason::invalid_pattern});
  }

  /*
   * Arrange: A long text and a small regex step budget.
   * Act: Validate a pattern against it.
   * Expect: resource_limit when the budget is exhausted; a verdict when it suffices (J5-065).
   */
  TEST(P049_PatternErrors, EnforcesTheRegexBudget)
  {
    const std::string text = "\"" + std::string(200U, 'a') + "\"";
    validation_budget small{};
    small.max_regex_steps = 50U;
    const auto limited = run_schema(R"({"pattern": "(a|a)*b"})", text, small);
    EXPECT_EQ(limited.error, std::optional<schema_error_code>{schema_error_code::resource_limit});
    const auto enough = run_schema(R"({"pattern": "(a|a)*b"})", text);
    ASSERT_FALSE(enough.error.has_value());
    EXPECT_FALSE(enough.valid);
    const std::vector<schema_keyword> expected{schema_keyword::pattern};
    EXPECT_EQ(enough.keywords, expected);
  }

  /*
   * Arrange: Length and pattern keywords with a non-string instance.
   * Act: Validate.
   * Expect: String keywords do not apply to other types.
   */
  TEST(P049_PatternErrors, IgnoresNonStrings)
  {
    EXPECT_EQ(schema_verdict(R"({"pattern": "^x$", "minLength": 5})", "[1]"), std::optional<bool>{true});
  }
} // namespace
