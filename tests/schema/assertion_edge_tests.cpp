#include "adaptive_json5/schema.hpp"
#include "schema_run.hpp"

#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::run_schema;
  using adaptive_json5_test::schema_outcome;

  /**
   * @brief Expected result of one edge case: a verdict, or an error code (and invalid-schema reason).
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-057; J5-060.
   */
  struct edge_case
  {
      const char* name;
      const char* schema;
      const char* instance;
      std::optional<bool> valid;
      std::optional<schema_error_code> error;
      std::optional<invalid_schema_reason> reason;
  };

  /**
   * @brief A case that validates with the given verdict.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-057.
   */
  edge_case verdict(const char* name, const char* schema, const char* instance, bool valid)
  {
    return edge_case{name, schema, instance, valid, std::nullopt, std::nullopt};
  }

  /**
   * @brief A case that fails with the given error code and optional invalid-schema reason.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-054; J5-060; J5-065.
   */
  edge_case failure(const char* name, const char* schema, const char* instance, schema_error_code code,
                    std::optional<invalid_schema_reason> reason = std::nullopt)
  {
    return edge_case{name, schema, instance, std::nullopt, code, reason};
  }

  /**
   * @brief Check one outcome against an edge case.
   * @par Requirements
   * P047; P048; P049; P050; P051; J5-057.
   */
  void expect_case(const edge_case& item, const schema_outcome& outcome)
  {
    EXPECT_EQ(outcome.error, item.error) << item.schema << " / " << item.instance;
    if (item.reason.has_value())
    {
      EXPECT_EQ(outcome.reason, item.reason) << item.schema;
    }
    if (item.valid.has_value() && !outcome.error.has_value())
    {
      EXPECT_EQ(outcome.valid, *item.valid) << item.schema << " / " << item.instance;
    }
  }

  /* ================================== Test_P049_PatternEdges ================================== */
  class P049_PatternEdges : public testing::TestWithParam<edge_case>
  {
  };

  /*
   * Arrange: Patterns with surrogate pairs and unpaired units, hex escapes, class complements at the
   *          code point limits, Annex B braces, lazy counted repeats, word boundaries and line terminators.
   * Act: Compile and validate in collect-all mode.
   * Expect: ECMA-262 code point semantics, unsupported features as unsupported_pattern and oversized
   *         repeats as resource_limit (J5-060, J5-065).
   */
  TEST_P(P049_PatternEdges, FollowEcma262)
  {
    expect_case(GetParam(), run_schema(GetParam().schema, GetParam().instance));
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P049_PatternEdges,
    testing::Values(
      verdict("SurrogatePairLiteral", R"({"pattern": "^😀$"})", R"("😀")", true),
      verdict("SurrogatePairIsOneCharacter", R"({"pattern": "^.$"})", R"("😀")", true),
      failure("UnpairedUnitInPattern", R"({"pattern": "a\ud83db"})", "null", schema_error_code::invalid_schema),
      verdict("NegatedFromZero", R"({"pattern": "^[^\u0000-a]$"})", R"("b")", true),
      verdict("NegatedFromZeroExcludes", R"({"pattern": "^[^\u0000-a]$"})", R"("a")", false),
      verdict("NegatedToMaximum", R"({"pattern": "^[^b-􏿿]$"})", R"("a")", true),
      verdict("HexEscapes", R"({"pattern": "^\\x4a\\u004A\\x4F\\u00e9$"})", R"("JJOé")", true),
      verdict("TruncatedHexIsLetter", R"({"pattern": "^\\x4$"})", R"("x4")", true),
      verdict("TruncatedUnicodeIsLetter", R"({"pattern": "^\\u12G$"})", R"("u12G")", true),
      verdict("AnnexBOpenCount", R"({"pattern": "^a{2,$"})", R"("a{2,")", true),
      verdict("AnnexBNoLowCount", R"({"pattern": "^a{,3}$"})", R"("a{,3}")", true),
      verdict("AnnexBUnclosedCount", R"({"pattern": "^a{2,3$"})", R"("a{2,3")", true),
      verdict("LazyCount", R"({"pattern": "^a{2}?$"})", R"("aa")", true),
      verdict("LazyRange", R"({"pattern": "^(a{1,3}?)(a*)$"})", R"("aaaa")", true),
      verdict("WordBoundaryDigits", R"({"pattern": "\\bZ9_\\b"})", R"(" Z9_ ")", true),
      verdict("NotWordBoundary", R"({"pattern": "a\\Bb"})", R"("ab")", true),
      verdict("DotRejectsLineFeed", R"({"pattern": "^.$"})", R"("\n")", false),
      verdict("DotRejectsCarriageReturn", R"({"pattern": "^.$"})", R"("\r")", false),
      verdict("DotRejectsLineSeparator", R"({"pattern": "^.$"})", R"(" ")", false),
      verdict("DotRejectsParagraphSeparator", R"({"pattern": "^.$"})", R"(" ")", false),
      verdict("NonCapturingGroup", R"({"pattern": "^(?:ab)+$"})", R"("abab")", true),
      verdict("ClassShorthandsInside", R"({"pattern": "^[\\d\\s\\w]+$"})", R"("a 1_")", true),
      verdict("NegatedShorthands", R"({"pattern": "^\\D\\S\\W$"})", R"("a-!")", true),
      failure("UnclosedClass", R"({"pattern": "[^"})", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::invalid_pattern),
      verdict("AnnexBClassRangeToSet", R"({"pattern": "^[a-\\d]$"})", R"("-")", true),
      failure("Lookahead", R"x({"pattern": "(?=a)"})x", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::unsupported_pattern),
      failure("NegativeLookahead", R"x({"pattern": "(?!a)"})x", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::unsupported_pattern),
      failure("NamedReference", R"({"pattern": "\\k<a>"})", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::unsupported_pattern),
      failure("ControlLetter", R"({"pattern": "\\cA"})", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::unsupported_pattern),
      failure("PropertyEscape", R"({"pattern": "\\p{L}"})", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::unsupported_pattern),
      failure("NegatedPropertyEscape", R"({"pattern": "\\P{L}"})", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::unsupported_pattern),
      failure("HugeCount", R"({"pattern": "a{99999999999}"})", "null", schema_error_code::resource_limit),
      failure("HugeUpperCount", R"({"pattern": "a{1,200000}"})", "null", schema_error_code::resource_limit),
      verdict("HexEscapeBeyondF", R"({"pattern": "^\\xg1$"})", R"("xg1")", true),
      verdict("HexEscapeAtPatternEnd", R"({"pattern": "a\\x4"})", R"("ax4")", true),
      verdict("UnicodeEscapeAtPatternEnd", R"({"pattern": "a\\u00"})", R"("au00")", true),
      verdict("AdjacentComplement", R"({"pattern": "^[^ab]$"})", R"("c")", true),
      verdict("AdjacentComplementExcludes", R"({"pattern": "^[^ab]$"})", R"("b")", false),
      verdict("CountAtEnd", R"({"pattern": "a{2}"})", R"("xaa")", true),
      verdict("OpenBraceAtEnd", R"({"pattern": "a{"})", R"("a{")", true),
      verdict("LowCountAtEnd", R"({"pattern": "a{1"})", R"("a{1")", true),
      verdict("CommaAtEnd", R"({"pattern": "a{1,"})", R"("a{1,")", true),
      failure("ClassAtEnd", R"({"pattern": "a["})", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::invalid_pattern),
      failure("GroupAtEnd", R"({"pattern": "a("})", "null", schema_error_code::invalid_schema,
              invalid_schema_reason::invalid_pattern),
      failure("GroupQuestionAtEnd", R"({"pattern": "a(?"})", "null", schema_error_code::invalid_schema)),
    [](const testing::TestParamInfo<edge_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* ================================== Test_P048_NumberSpellings ================================== */
  class P048_NumberSpellings : public testing::TestWithParam<edge_case>
  {
  };

  /*
   * Arrange: JSON5 instance spellings (leading plus, signed exponents, hexadecimal, Infinity, NaN).
   * Act: Validate numeric keywords.
   * Expect: Exact results independent of the spelling; non-finite instances are unsupported_instance
   *         (J5-057, J5-059).
   */
  TEST_P(P048_NumberSpellings, AreExact)
  {
    expect_case(GetParam(), run_schema(GetParam().schema, GetParam().instance));
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P048_NumberSpellings,
    testing::Values(
      verdict("LeadingPlus", R"({"multipleOf": 5, "minimum": 5})", "+5", true),
      verdict("PlusExponent", R"({"multipleOf": 100})", "1e+2", true),
      verdict("MinusExponent", R"({"multipleOf": 0.01})", "3e-2", true),
      verdict("Hexadecimal", R"({"multipleOf": 16})", "0x20", true),
      verdict("NegativeHexadecimal", R"({"multipleOf": 3})", "-0x0F", true),
      verdict("FractionOfFraction", R"({"multipleOf": 0.75})", "2.2", false),
      failure("PositiveInfinityIsUnsupported", R"({"maximum": 1e308})", "Infinity",
              schema_error_code::unsupported_instance),
      failure("NegativeInfinityIsUnsupported", R"({"minimum": -1e308})", "-Infinity",
              schema_error_code::unsupported_instance),
      verdict("ZeroIsMultiple", R"({"multipleOf": 7})", "-0.0", true),
      failure("SaturatedExponent", R"({"multipleOf": 3})", "1e999999999999", schema_error_code::resource_limit),
      failure("ExponentAboveCap", R"({"multipleOf": 3})", "1e99999999999999", schema_error_code::resource_limit),
      failure("TinyExponentAboveCap", R"({"multipleOf": 3})", "1e-99999999999999", schema_error_code::resource_limit),
      verdict("HugeLengthKeyword", R"({"maxLength": 1e30, "minItems": 18446744073709551616})", R"("abc")", true),
      verdict("HugeLengthFails", R"({"minLength": 1e30})", R"("abc")", false),
      verdict("PrivateUseLength", R"({"maxLength": 2})", R"("￿")", true),
      verdict("EmptyEnumMatchesNothing", R"({"enum": []})", "1", false),
      failure("HugeNegativeLength", R"({"maxLength": -1e30})", R"("a")", schema_error_code::invalid_schema,
              invalid_schema_reason::keyword_shape),
      verdict("EmptyIdFragment", R"({"definitions": {"a": {"$id": "#"}}, "$id": "http://example.com/s.json#"})", "1",
              true),
      verdict("AstralPointerToken", R"({"definitions": {"😀": {"type": "integer"}}, "$ref": "#/definitions/😀"})", "1",
              true),
      failure("AstralPointerTokenMissing", R"({"definitions": {"a": true}, "$ref": "#/definitions/😀"})", "1",
              schema_error_code::unresolved_reference),
      verdict("PlainExponent", R"({"multipleOf": 100})", "1e2", true),
      verdict("ExponentDivisor", R"({"multipleOf": 1e-2})", "0.03", true),
      verdict("ExponentDivisorFails", R"({"multipleOf": 2E1})", "30", false),
      verdict("PlainNameFragment", R"({"definitions": {"a": {"$id": "#foo", "type": "integer"}}, "$ref": "#foo"})", "1",
              true),
      failure("EmptyTypeList", R"({"type": []})", "1", schema_error_code::invalid_schema),
      failure("RequiredObject", R"({"required": {}})", "{}", schema_error_code::invalid_schema),
      failure("NegativeLength", R"({"maxLength": -1})", "1", schema_error_code::invalid_schema),
      failure("FractionalCount", R"({"minItems": 1.5})", "1", schema_error_code::invalid_schema),
      verdict("IntegralFractionCount", R"({"minItems": 1.0})", "[1]", true),
      verdict("AdditionalItemsWithoutTuple", R"({"items": {}, "additionalItems": false})", "[1, 2]", true),
      verdict("ComplementOfZero", R"({"pattern": "^[^\u0000]$"})", R"("a")", true),
      verdict("ComplementToMaximum", R"({"pattern": "^[^\u0001-􏿿]$"})", R"("\u0000")", true),
      failure("NegativeDivisor", R"({"multipleOf": -2})", "1", schema_error_code::invalid_schema,
              invalid_schema_reason::keyword_shape),
      failure("HexDivisorZero", R"({"multipleOf": 0})", "1", schema_error_code::invalid_schema)),
    [](const testing::TestParamInfo<edge_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* =================================== Test_P051_FailFast =================================== */
  class P051_FailFast : public testing::TestWithParam<edge_case>
  {
  };

  /*
   * Arrange: Schemas whose late phases (items, contains, members, dependencies, propertyNames) fail.
   * Act: Validate in fail-fast mode.
   * Expect: The same verdict as collect-all mode (J5-057).
   */
  TEST_P(P051_FailFast, MatchesCollectAll)
  {
    const edge_case& item = GetParam();
    expect_case(item, run_schema(item.schema, item.instance, validation_budget{}, evaluation_mode::fail_fast));
    expect_case(item, run_schema(item.schema, item.instance));
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P051_FailFast,
    testing::Values(
      verdict("AllOfStopsEarly", R"({"allOf": [false, true, {"type": "string"}]})", "1", false),
      verdict("ItemsWithMissingTarget", R"({"items": [true], "contains": {"const": 1}})", "[1, 2]", true),
      verdict("ItemsThenContains", R"({"items": {"type": "integer"}, "contains": {"const": 9}})", "[1, 2.5, 3]", false),
      verdict("TupleAdditional", R"({"items": [true], "additionalItems": false, "contains": true})", "[1, 2]", false),
      verdict("ContainsFails", R"({"contains": {"const": 9}, "dependencies": {"a": ["b"]}})", "[1, 2]", false),
      verdict("MembersFail",
              R"({"properties": {"a": false}, "patternProperties": {"^b": false}, "additionalProperties": false})",
              R"({"a": 1, "b": 2, "c": 3})", false),
      verdict("DependencySchemasFail",
              R"({"dependencies": {"a": {"required": ["x"]}, "b": {"required": ["y"]}}, "propertyNames": true})",
              R"({"a": 1, "b": 2})", false),
      verdict("NamesFail", R"({"propertyNames": {"maxLength": 1}})", R"({"ab": 1, "cd": 2})", false),
      verdict("NamesAfterValidDependencies", R"({"dependencies": {"a": true}, "propertyNames": {"pattern": "^a"}})",
              R"({"a": 1, "b": 2})", false),
      verdict("SurrogateNameLength", R"({"propertyNames": {"maxLength": 1}})", R"({"😀": 1})", true),
      verdict("SurrogateNamePattern", R"({"propertyNames": {"pattern": "^.$"}})", R"({"😀": 1})", true),
      verdict("AllPass",
              R"({"items": {"type": "integer"}, "contains": {"const": 2}, "propertyNames": {"maxLength": 3}})",
              "[1, 2]", true)),
    [](const testing::TestParamInfo<edge_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* ================================== Test_P051_ListShapes ================================== */
  class P051_ListShapes : public testing::TestWithParam<edge_case>
  {
  };

  /*
   * Arrange: Malformed required and dependency name lists and valid empty lists.
   * Act: Compile.
   * Expect: Duplicate or non-string names and non-array lists are keyword_shape errors; empty lists are
   *         accepted (J5-054, J5-057).
   */
  TEST_P(P051_ListShapes, AreChecked)
  {
    expect_case(GetParam(), run_schema(GetParam().schema, GetParam().instance));
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P051_ListShapes,
    testing::Values(failure("RequiredNotArray", R"({"required": "a"})", "{}", schema_error_code::invalid_schema,
                            invalid_schema_reason::keyword_shape),
                    failure("RequiredDuplicate", R"({"required": ["a", "a"]})", "{}", schema_error_code::invalid_schema,
                            invalid_schema_reason::keyword_shape),
                    failure("RequiredNumber", R"({"required": [1]})", "{}", schema_error_code::invalid_schema,
                            invalid_schema_reason::keyword_shape),
                    failure("DependencyDuplicate", R"({"dependencies": {"a": ["b", "b"]}})", "{}",
                            schema_error_code::invalid_schema, invalid_schema_reason::keyword_shape),
                    failure("DependencyNumber", R"({"dependencies": {"a": [1]}})", "{}",
                            schema_error_code::invalid_schema, invalid_schema_reason::keyword_shape),
                    failure("DependencyString", R"({"dependencies": {"a": "b"}})", "{}",
                            schema_error_code::invalid_schema, invalid_schema_reason::keyword_shape),
                    verdict("RequiredEmpty", R"({"required": []})", "{}", true),
                    verdict("DependencyEmpty", R"({"dependencies": {"a": []}})", R"({"a": 1})", true),
                    verdict("DependencySurrogateNames", R"({"dependencies": {"😀": ["😁"]}})", R"({"😀": 1, "😁": 2})",
                            true)),
    [](const testing::TestParamInfo<edge_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /* ================================= Test_P049_UnpairedUnits ================================= */
  /*
   * Arrange: Instance strings with unpaired surrogate units.
   * Act: Validate minLength/maxLength and pattern.
   * Expect: The instance-domain policy applies before the string keywords: the outcome is the same
   *         error or verdict for every string keyword (J5-060).
   */
  TEST(P049_UnpairedUnits, FollowTheInstancePolicy)
  {
    const schema_outcome length = run_schema(R"({"maxLength": 1})", R"("\udc00")");
    const schema_outcome pattern = run_schema(R"({"pattern": "^.$"})", R"("\udc00")");
    EXPECT_EQ(length.error, pattern.error);
    if (!length.error.has_value())
    {
      EXPECT_TRUE(length.valid);
      EXPECT_TRUE(pattern.valid);
    }
  }
} // namespace
