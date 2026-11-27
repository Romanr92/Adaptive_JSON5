#include "schema/idna.hpp"
#include "schema/normalization.hpp"
#include "schema/unicode_properties.hpp"
#include "schema/unicode_property.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  using namespace adaptive_json5::detail;

  /**
   * @brief Decode text with punycode_decode; nullopt on failure.
   * @par Requirements
   * J5-069; RFC 3492.
   */
  std::optional<std::vector<std::uint32_t>> decode(std::string_view text)
  {
    std::array<std::uint32_t, c_max_label_code_points> output{};
    const auto count = punycode_decode(text, output);
    if (!count.has_value())
    {
      return std::nullopt;
    }
    return std::vector<std::uint32_t>(output.begin(), output.begin() + static_cast<std::ptrdiff_t>(*count));
  }

  /* ================================ Test_P187_UnicodeProperties ================================ */
  /*
   * Arrange: Code points of known category, script, joining type, combining class and IDNA status.
   * Act: Look them up in the generated Unicode 17.0.0 tables.
   * Expect: The UCD values; gaps are Cn, Unknown, non-joining and DISALLOWED (ADR 0006 #187).
   */
  TEST(P187_UnicodeProperties, LookUpCodePoints)
  {
    EXPECT_EQ(general_category_bit(0x41U), *general_category_mask("Lu"));
    EXPECT_EQ(general_category_bit(0x10FFFFU), c_unassigned_category_mask);
    EXPECT_EQ(general_category_bit(0x378U), *general_category_mask("Cn"));
    EXPECT_EQ(*general_category_mask("L"), *general_category_mask("Letter"));
    EXPECT_EQ(*general_category_mask("digit"), *general_category_mask("Nd"));
    EXPECT_FALSE(general_category_mask("Bogus").has_value());
    EXPECT_FALSE(general_category_mask("").has_value());
    EXPECT_EQ(script_of(0x3B1U), *script_number("Greek"));
    EXPECT_EQ(*script_number("Grek"), *script_number("Greek"));
    EXPECT_EQ(script_of(0x378U), c_unknown_script);
    EXPECT_FALSE(script_number("Klingon").has_value());
    EXPECT_EQ(joining_type_of(0x628U), std::optional<joining_type>{joining_type::d_type});
    EXPECT_EQ(joining_type_of(0x627U), std::optional<joining_type>{joining_type::r_type});
    EXPECT_FALSE(joining_type_of(0x41U).has_value());
    EXPECT_TRUE(is_virama(0x94DU));
    EXPECT_FALSE(is_virama(0x41U));
    EXPECT_EQ(idna_property_of(0x61U), std::optional<idna_property>{idna_property::pvalid});
    EXPECT_EQ(idna_property_of(0x200CU), std::optional<idna_property>{idna_property::contextj});
    EXPECT_EQ(idna_property_of(0xB7U), std::optional<idna_property>{idna_property::contexto});
    EXPECT_FALSE(idna_property_of(0x41U).has_value());
    EXPECT_FALSE(idna_property_of(0x378U).has_value());
  }

  /* =================================== Test_P187_Punycode =================================== */
  /*
   * Arrange: RFC 3492 encodings (basic-only prefix, all-encoded, 63 code points) and malformed ones:
   *          a truncated integer, an invalid digit, arithmetic overflow, a surrogate, a code point
   *          beyond U+10FFFF and more than 63 code points.
   * Act: Decode the text after "xn--".
   * Expect: The original code points; nullopt for every malformed input (J5-069, RFC 3492).
   */
  TEST(P187_Punycode, DecodesAndRejects)
  {
    EXPECT_EQ(decode("mnchen-3ya"), (std::vector<std::uint32_t>{'m', 0xFCU, 'n', 'c', 'h', 'e', 'n'}));
    EXPECT_EQ(decode("bcher-kva"), (std::vector<std::uint32_t>{'b', 0xFCU, 'c', 'h', 'e', 'r'}));
    EXPECT_EQ(decode("r8jz45g"), (std::vector<std::uint32_t>{0x4F8BU, 0x3048U}));
    EXPECT_EQ(decode("MNCHEN-3YA"), (std::vector<std::uint32_t>{'M', 0xFCU, 'N', 'C', 'H', 'E', 'N'}));
    EXPECT_EQ(decode("dn32g"), (std::vector<std::uint32_t>{0x10FFFFU}));
    EXPECT_EQ(decode(""), std::vector<std::uint32_t>{});
    EXPECT_EQ(decode("abc-"), (std::vector<std::uint32_t>{'a', 'b', 'c'}));
    const std::string longest = "td" + std::string(63U, 'a');
    ASSERT_TRUE(decode(longest).has_value());
    EXPECT_EQ(decode(longest)->size(), 63U);
    EXPECT_FALSE(decode(longest + "a").has_value());
    EXPECT_FALSE(decode(std::string(64U, 'a') + "-").has_value());
    EXPECT_FALSE(decode("b").has_value());
    EXPECT_FALSE(decode("a!").has_value());
    EXPECT_FALSE(decode("a{").has_value());
    EXPECT_FALSE(decode("a_").has_value());
    EXPECT_FALSE(decode("a:").has_value());
    EXPECT_TRUE(has_ace_prefix("Xn--abc"));
    EXPECT_FALSE(has_ace_prefix("xn-"));
    EXPECT_FALSE(has_ace_prefix("xm--a"));
    EXPECT_FALSE(has_ace_prefix("xna-a"));
    EXPECT_FALSE(decode("99999999999a").has_value());
    EXPECT_FALSE(decode("ib9b").has_value());
    EXPECT_FALSE(decode("en32g").has_value());
  }

  /**
   * @brief Run is_nfc on a list of code points (at most c_max_label_code_points).
   * @par Requirements
   * J5-069; Unicode UAX #15.
   */
  bool nfc(const std::vector<std::uint32_t>& text)
  {
    std::array<std::uint32_t, c_max_label_code_points> label{};
    std::size_t index = 0U;
    for (const std::uint32_t code_point : text)
    {
      label[index] = code_point;
      ++index;
    }
    return is_nfc(label, text.size());
  }

  /* ===================================== Test_P187_Nfc ====================================== */
  /*
   * Arrange: Composed and decomposed forms: a precomposed letter, a base with a combining accent,
   *          a singleton (ANGSTROM SIGN), a composition exclusion (DEVANAGARI QA), non-starters
   *          out of canonical order, a blocked composition and Hangul LV, LVT and conjoining jamo.
   * Act: Check each for NFC.
   * Expect: The Unicode 17.0.0 NFC verdict (J5-069; RFC 5891 section 5.4; UAX #15).
   */
  TEST(P187_Nfc, MatchesUnicodeNormalization)
  {
    EXPECT_TRUE(nfc({0xE9U}));
    EXPECT_FALSE(nfc({0x65U, 0x301U}));
    EXPECT_FALSE(nfc({0x212BU}));
    EXPECT_FALSE(nfc({0x958U}));
    EXPECT_TRUE(nfc({0x915U, 0x93CU}));
    EXPECT_FALSE(nfc({0x61U, 0x301U, 0x316U}));
    EXPECT_FALSE(nfc({0x61U, 0x316U, 0x301U}));
    EXPECT_TRUE(nfc({0xE1U, 0x316U}));
    EXPECT_FALSE(nfc({0x61U, 0x315U, 0x300U, 0x5AEU, 0x300U, 0x62U}));
    EXPECT_TRUE(nfc({0xE0U, 0x5AEU, 0x300U, 0x315U, 0x62U}));
    EXPECT_TRUE(nfc({0x301U, 0x61U}));
    EXPECT_TRUE(nfc({0xAC00U}));
    EXPECT_TRUE(nfc({0xAC01U}));
    EXPECT_FALSE(nfc({0x1100U, 0x1161U}));
    EXPECT_FALSE(nfc({0xAC00U, 0x11A8U}));
    EXPECT_TRUE(nfc({}));
    EXPECT_TRUE(nfc(std::vector<std::uint32_t>(63U, 0x1E08U)));
  }

  /* =================================== Test_P187_BidiRule =================================== */
  struct bidi_case
  {
      const char* name;
      const char* host;
      bool valid;
  };

  class P187_BidiRule : public testing::TestWithParam<bidi_case>
  {
  };

  /*
   * Arrange: Host names without right-to-left labels, and Bidi domains whose labels meet or break
   *          each condition of RFC 5893 section 2.
   * Act: Apply satisfies_bidi_rule.
   * Expect: The rule applies only when a label holds R, AL or AN, and then to every label
   *         (J5-069; RFC 5893).
   */
  TEST_P(P187_BidiRule, FollowsRfc5893)
  {
    EXPECT_EQ(satisfies_bidi_rule(GetParam().host), GetParam().valid) << GetParam().host;
  }

  INSTANTIATE_TEST_SUITE_P(Cases, P187_BidiRule,
                           testing::Values(bidi_case{"NoRightToLeft", "1abc.example", true},
                                           bidi_case{"Hebrew", "xn--4dbc5h", true},
                                           bidi_case{"WithLtrLabel", "abc.xn--4dbc5h", true},
                                           bidi_case{"LtrLabelStartsWithDigit", "1abc.xn--4dbc5h", false},
                                           bidi_case{"LtrLabelEndsWithHyphen", "abc-.xn--4dbc5h", false},
                                           bidi_case{"LtrLabelEndsWithDigit", "abc1.xn--4dbc5h", true},
                                           bidi_case{"ArabicIndicDigit", "xn--ngba1o", true},
                                           bidi_case{"EuropeanAndArabicDigits", "xn--1-0mcb6t", false},
                                           bidi_case{"LtrWithHebrew", "xn--a-0hc", false},
                                           bidi_case{"StartsWithArabicDigit", "xn--ngb5i", false},
                                           bidi_case{"EndsWithMark", "xn--ngb0f", true},
                                           bidi_case{"RtlEndsWithNeutral", "xn--uba99y", false},
                                           bidi_case{"LtrEndsWithNeutral", "xn--a-gda.xn--4dbc5h", false},
                                           bidi_case{"UndecodableLabel", "xn--b.xn--4dbc5h", false}),
                           [](const testing::TestParamInfo<bidi_case>& param_info)
                           {
                             return std::string{param_info.param.name};
                           });

  /* =================================== Test_P187_ALabels ==================================== */
  struct label_case
  {
      const char* name;
      const char* label;
      bool valid;
  };

  class P187_ALabels : public testing::TestWithParam<label_case>
  {
  };

  /*
   * Arrange: A-labels for each U-label rule: PVALID text, all-ASCII and empty decodings, upper
   *          case, leading and doubled hyphens, a leading combining mark and each CONTEXTJ and
   *          CONTEXTO rule in an allowed and a forbidden form.
   * Act: Validate them.
   * Expect: RFC 5891 section 5.4 and RFC 5892 (derived property and Appendix A) (J5-069).
   */
  TEST_P(P187_ALabels, FollowIdna2008)
  {
    EXPECT_EQ(is_a_label(GetParam().label), GetParam().valid) << GetParam().label;
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P187_ALabels,
    testing::Values(
      label_case{"Pvalid", "xn--mnchen-3ya", true}, label_case{"UpperPrefix", "XN--mnchen-3ya", true},
      label_case{"UpperLetters", "xn--MNCHEN-3YA", false}, label_case{"AllAscii", "xn--abc-", false},
      label_case{"Empty", "xn--", false}, label_case{"Malformed", "xn--b", false},
      label_case{"Cjk", "xn--r8jz45g", true}, label_case{"HyphensThreeFour", "xn--aa---o47jg78q", false},
      label_case{"LeadingHyphen", "xn---abc-epa", false}, label_case{"LeadingMark", "xn--hello-zed", false},
      label_case{"ZeroWidthNonJoinerJoining", "xn--ngba799q", true},
      label_case{"ZeroWidthNonJoinerAlone", "xn--0ug", false},
      label_case{"ZeroWidthJoinerAfterVirama", "xn--11b6iy14e", true},
      label_case{"ZeroWidthJoinerAlone", "xn--1ug", false}, label_case{"MiddleDotBetweenL", "xn--ll-0ea", true},
      label_case{"MiddleDotAlone", "xn--l-fda", false}, label_case{"KeraiaGreek", "xn--wva3je", true},
      label_case{"KeraiaAlone", "xn--wva3j", false}, label_case{"GereshHebrew", "xn--4dbc5h", true},
      label_case{"GereshAlone", "xn--5db1e", false}, label_case{"KatakanaDotHan", "xn--vek868f", true},
      label_case{"KatakanaDotAlone", "xn--vek", false}, label_case{"ArabicIndicDigits", "xn--8hbc", true},
      label_case{"MixedArabicIndicDigits", "xn--8hb20a", false},
      label_case{"ExtendedArabicIndicDigits", "xn--0-gyc", true}, label_case{"Disallowed", "xn--07jt112bpxg", false},
      label_case{"KatakanaDotKatakana", "xn--cckzj", true}, label_case{"KatakanaDotHiragana", "xn--l8j4u", true},
      label_case{"ZeroWidthJoinerNoVirama", "xn--a-bga030v", false},
      label_case{"ZeroWidthNonJoinerTransparent", "xn--ngba7ia3604a", true},
      label_case{"ZeroWidthNonJoinerAtEnd", "xn--ngb073k", false},
      label_case{"ZeroWidthNonJoinerNoLeftJoin", "xn--mgbc799q", false},
      label_case{"ZeroWidthNonJoinerVirama", "xn--11b6iv14e", true},
      label_case{"MiddleDotNoPrecedingL", "xn--al-0ea", false},
      label_case{"MiddleDotNoFollowingL", "xn--la-0ea", false}, label_case{"MiddleDotAtEnd", "xn--l-gda", false},
      label_case{"GershayimHebrew", "xn--4dbc9h", true}, label_case{"GershayimNotHebrew", "xn--a-2jc", false},
      label_case{"GereshAtStart", "xn--4eb", false}, label_case{"ExtendedThenArabicIndicDigits", "xn--8hb10a", false},
      label_case{"TrailingHyphen", "xn----9fa", false}, label_case{"ShortLabel", "xn--9ca", true},
      label_case{"KeraiaNotGreek", "xn--a-jib", false}),
    [](const testing::TestParamInfo<label_case>& param_info)
    {
      return std::string{param_info.param.name};
    });
} // namespace
