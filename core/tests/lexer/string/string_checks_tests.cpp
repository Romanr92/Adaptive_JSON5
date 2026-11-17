#include "../../../../tests/shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/security_reason.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "input/cursor.hpp"
#include "lexer/string/string_checks.hpp"
#include "lexer/string/string_lexer.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_032 =================================== */

  // The default checks apply nothing, so every existing caller keeps standard behavior.
  static_assert(!string_checks{}.reject_zero_bytes);
  static_assert(!string_checks{}.reject_control_characters);
  static_assert(!string_checks{}.reject_unpaired_surrogate);
  static_assert(noexcept(value_string_checks(parse_options{})));
  static_assert(noexcept(name_string_checks(parse_options{})));

  /* Verify the value and name checks are selected from the parse options.
   * Arrange: Default options, secure_parse_options(), and secure options with each string
   *          option switched off.
   * Act: Select value and name checks from each option set.
   * Expect: Values never check control characters; names add that check; each flag follows
   *         its own option only.
   */
  TEST(J5_032_StringChecks, FollowsParseOptions)
  {
    /* Arrange */
    const parse_options standard{};
    const parse_options secure = secure_parse_options();
    parse_options without_zero = secure;
    without_zero.zero_bytes = zero_byte_policy::preserve;
    parse_options without_controls = secure;
    without_controls.name_controls = name_control_policy::preserve;
    parse_options without_surrogates = secure;
    without_surrogates.unpaired_surrogates = unpaired_surrogate_policy::warn;
    /* Act */
    const string_checks standard_value = value_string_checks(standard);
    const string_checks standard_name = name_string_checks(standard);
    const string_checks secure_value = value_string_checks(secure);
    const string_checks secure_name = name_string_checks(secure);
    const string_checks no_zero_name = name_string_checks(without_zero);
    const string_checks no_controls_name = name_string_checks(without_controls);
    const string_checks no_surrogates_value = value_string_checks(without_surrogates);
    /* Expect */
    EXPECT_FALSE(standard_value.reject_zero_bytes || standard_value.reject_control_characters ||
                 standard_value.reject_unpaired_surrogate);
    EXPECT_FALSE(standard_name.reject_zero_bytes || standard_name.reject_control_characters ||
                 standard_name.reject_unpaired_surrogate);
    EXPECT_TRUE(secure_value.reject_zero_bytes);
    EXPECT_FALSE(secure_value.reject_control_characters);
    EXPECT_TRUE(secure_value.reject_unpaired_surrogate);
    EXPECT_TRUE(secure_name.reject_zero_bytes);
    EXPECT_TRUE(secure_name.reject_control_characters);
    EXPECT_TRUE(secure_name.reject_unpaired_surrogate);
    EXPECT_FALSE(no_zero_name.reject_zero_bytes);
    EXPECT_TRUE(no_zero_name.reject_control_characters);
    EXPECT_TRUE(no_controls_name.reject_zero_bytes);
    EXPECT_FALSE(no_controls_name.reject_control_characters);
    EXPECT_FALSE(no_surrogates_value.reject_unpaired_surrogate);
    EXPECT_TRUE(no_surrogates_value.reject_zero_bytes);
  }

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief One string token checked by the string lexer.
   * @par Requirements
   * CSR-07; J5-032; issue #145.
   */
  struct checked_string_case
  {
      const char* name;        // Case identifier.
      std::string_view input;  // Input string token, from its opening quote.
      bool name_checks;        // Input uses name checks instead of value checks.
      security_reason reason;  // Expected reason.
      std::size_t byte_offset; // Expected start of the offending character or escape, in bytes.
      std::size_t byte_length; // Expected length of the offending character or escape, in bytes.
  };

  class J5_032_StringChecksLexer : public testing::TestWithParam<checked_string_case>
  {
  };

  /* Verify measure_string() and consume_string() report the same violation without allocating,
   * and leave the cursor unchanged.
   * Arrange: One JSON5 string token per case, the matching secure checks and a counting
   *          storage provider.
   * Act: Measure the token, then consume it with the provider.
   * Expect: Both fail with the same security_policy reason and range; the provider made no
   *         allocation; both cursors stay at offset 0.
   */
  TEST_P(J5_032_StringChecksLexer, MeasureAndConsumeAgreeWithoutAllocation)
  {
    /* Arrange */
    const checked_string_case& fixture = GetParam();
    const parse_options secure = secure_parse_options();
    const string_checks checks = fixture.name_checks ? name_string_checks(secure) : value_string_checks(secure);
    document_provider provider;
    cursor measuring({fixture.input.data(), fixture.input.size()});
    cursor consuming({fixture.input.data(), fixture.input.size()});
    /* Act */
    const auto measured = measure_string(measuring, syntax_mode::json5, 64U, 64U, checks);
    const auto consumed = consume_string(consuming, syntax_mode::json5, provider.callbacks(), 64U, {}, nullptr, checks);
    /* Expect */
    ASSERT_FALSE(measured.has_value());
    ASSERT_FALSE(consumed.has_value());
    for (const error* failure : {measured.error_if(), consumed.error_if()})
    {
      EXPECT_EQ(failure->code(), error_code::security_policy);
      EXPECT_EQ(failure->security_reason(), std::optional<security_reason>{fixture.reason});
      EXPECT_EQ(failure->byte_offset(), fixture.byte_offset);
      ASSERT_TRUE(failure->source_range().has_value());
      EXPECT_EQ(failure->source_range()->byte_offset, fixture.byte_offset);
      EXPECT_EQ(failure->source_range()->byte_length, fixture.byte_length);
    }
    EXPECT_EQ(provider.m_allocations, 0U);
    EXPECT_EQ(measuring.offset(), 0U);
    EXPECT_EQ(consuming.offset(), 0U);
  }

  INSTANTIATE_TEST_SUITE_P(
    CSR_07, J5_032_StringChecksLexer,
    testing::Values(
      checked_string_case{"ValueZeroUnicode", R"("ab\u0000")", false, security_reason::zero_byte, 3, 6},
      checked_string_case{"ValueZeroNul", R"('\0')", false, security_reason::zero_byte, 1, 2},
      checked_string_case{"ValueZeroHex", R"("\x00")", false, security_reason::zero_byte, 1, 4},
      checked_string_case{"ValueZeroRaw", std::string_view{"'a\0'", 4}, false, security_reason::zero_byte, 2, 1},
      checked_string_case{"ValueFirstOfTwo", R"("\0\0")", false, security_reason::zero_byte, 1, 2},
      checked_string_case{"NameZeroBeatsControl", R"("\0")", true, security_reason::zero_byte, 1, 2},
      checked_string_case{"NameControlEscape", R"("a\b")", true, security_reason::name_control_character, 2, 2},
      checked_string_case{"NameControlUnicode", R"("\u0019")", true, security_reason::name_control_character, 1, 6},
      checked_string_case{"NameControlHex", R"('\x1F')", true, security_reason::name_control_character, 1, 4},
      checked_string_case{"NameControlRaw", "'\x1f'", true, security_reason::name_control_character, 1, 1},
      checked_string_case{"UnpairedHigh", R"("\uD800")", false, security_reason::unpaired_surrogate, 1, 6},
      checked_string_case{"UnpairedHighAtContinuationEnd", "'\\uD800\\\n'", false, security_reason::unpaired_surrogate,
                          1, 6},
      checked_string_case{"SurrogateBeforeLaterZero", R"("\uDC00\0")", false, security_reason::unpaired_surrogate, 1,
                          6}),
    [](const testing::TestParamInfo<checked_string_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify an unpaired surrogate is a warning by default and neither warned nor counted when
   * it is rejected, while control characters in values stay valid.
   * Arrange: "\uD800" and "\n", measured with no checks and with secure value checks.
   * Act: Measure each string with both check sets.
   * Expect: The default measurement counts one warning for the surrogate; the secure one fails;
   *         "\n" succeeds without warnings under both, since values do not check controls.
   */
  TEST(J5_032_StringChecksLexer, RejectionReplacesSurrogateWarning)
  {
    /* Arrange */
    constexpr std::string_view surrogate{R"("\uD800")"};
    constexpr std::string_view control{R"("\n")"};
    const string_checks secure = value_string_checks(secure_parse_options());
    cursor default_surrogate({surrogate.data(), surrogate.size()});
    cursor secure_surrogate({surrogate.data(), surrogate.size()});
    cursor secure_control({control.data(), control.size()});
    /* Act */
    const auto warned = measure_string(default_surrogate, syntax_mode::strict_json, 8U);
    const auto rejected = measure_string(secure_surrogate, syntax_mode::strict_json, 8U, 8U, secure);
    const auto accepted = measure_string(secure_control, syntax_mode::strict_json, 8U, 8U, secure);
    /* Expect */
    ASSERT_TRUE(warned.has_value());
    ASSERT_TRUE(warned.value_if()->has_value());
    EXPECT_EQ((*warned.value_if())->warning_count, 1U);
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->security_reason(),
              std::optional<security_reason>{security_reason::unpaired_surrogate});
    ASSERT_TRUE(accepted.has_value());
    ASSERT_TRUE(accepted.value_if()->has_value());
    EXPECT_EQ((*accepted.value_if())->warning_count, 0U);
    EXPECT_EQ((*accepted.value_if())->units, 1U);
  }
  /* ================================= End Test_J5_032 ================================= */
} // namespace
