#include "../../../tests/shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "memory/work_budget.hpp"
#include "value/integer_magnitude.hpp"
#include "value/number_analysis.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <ostream>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  constexpr std::size_t c_ample_work = 1000000;
  constexpr std::size_t c_max_spelling_bytes = 4096;
  constexpr std::uintmax_t c_uintmax_max = std::numeric_limits<std::uintmax_t>::max();

  /**
   * @brief Validate and own one JSON5 numeric spelling for a unit.
   * @par Requirements
   * P038; J5-034.
   * @param provider Guarded storage that must outlive the returned number.
   * @param text NUL-terminated numeric spelling.
   * @return Owned number or the validation/allocation error.
   */
  result<owned_number> make_number(guard_provider& provider, const char* text)
  {
    return owned_number::copy_spelling(input_view{text, std::strlen(text)}, syntax_mode::json5, provider.callbacks(),
                                       c_max_spelling_bytes);
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_036 =================================== */
  /**
   * @brief Describe the exact properties expected from one numeric spelling.
   * @par Requirements
   * P038; J5-036; J5-038.
   */
  struct analysis_case
  {
      const char* name;
      const char* spelling; // Input JSON5 numeric spelling.
      number_kind kind;     // Expected spelling family.
      bool negative;        // Expected sign of the spelling.
      bool is_zero;         // Expected exact-zero property.
      bool is_integral;     // Expected mathematical integrality.
  };

  std::ostream& operator<<(std::ostream& stream, const analysis_case& value)
  {
    return stream << value.name;
  }

  class NumberAnalysisUnits : public ::testing::TestWithParam<analysis_case>
  {
  };

  /* Verify analysis distinguishes mathematical integrality and zero from lexical form.
   * Arrange: Own the case spelling.
   * Act: Analyse it with an ample budget.
   * Expect: The expected kind, sign, zero and integrality properties.
   */
  TEST_P(NumberAnalysisUnits, ReportsExactProperties)
  {
    /* Arrange */
    const analysis_case& param = GetParam();
    guard_provider provider;
    const auto number = make_number(provider, param.spelling);
    ASSERT_TRUE(number.has_value());
    work_budget work{c_ample_work};
    /* Act */
    const auto analysis = detail::analyse_number(*number.value_if(), work);
    /* Expect */
    ASSERT_TRUE(analysis.has_value());
    EXPECT_EQ(analysis.value_if()->kind, param.kind);
    EXPECT_EQ(analysis.value_if()->negative, param.negative);
    EXPECT_EQ(analysis.value_if()->is_zero, param.is_zero);
    EXPECT_EQ(analysis.value_if()->is_integral, param.is_integral);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_036_NumberAnalysis, NumberAnalysisUnits,
    ::testing::Values(
      analysis_case{"Zero", "0", number_kind::decimal, false, true, true},
      analysis_case{"NegativeZeroFraction", "-0.0", number_kind::decimal, true, true, true},
      analysis_case{"ZeroHugeNegativeExponent", "0.0e-99999999999999999999", number_kind::decimal, false, true, true},
      analysis_case{"PositiveHexZero", "+0x0", number_kind::hexadecimal, false, true, true},
      analysis_case{"NegativeHexZero", "-0x00", number_kind::hexadecimal, true, true, true},
      analysis_case{"One", "1", number_kind::decimal, false, false, true},
      analysis_case{"PlusOne", "+1", number_kind::decimal, false, false, true},
      analysis_case{"OnePointZero", "1.0", number_kind::decimal, false, false, true},
      analysis_case{"OneExponentZero", "1e0", number_kind::decimal, false, false, true},
      analysis_case{"TrailingPoint", "5.", number_kind::decimal, false, false, true},
      analysis_case{"LeadingPoint", ".5", number_kind::decimal, false, false, false},
      analysis_case{"Fraction", "1.5", number_kind::decimal, false, false, false},
      analysis_case{"FractionScaledIntegral", "1.5e1", number_kind::decimal, false, false, true},
      analysis_case{"FractionScaledStillFractional", "1.25e1", number_kind::decimal, false, false, false},
      analysis_case{"TrailingZerosCancelNegativeExponent", "100e-2", number_kind::decimal, false, false, true},
      analysis_case{"NegativeExponentPastTrailingZeros", "100e-3", number_kind::decimal, false, false, false},
      analysis_case{"FractionalTrailingZeros", "123.4500e2", number_kind::decimal, false, false, true},
      analysis_case{"UppercaseSignedExponent", "1E+2", number_kind::decimal, false, false, true},
      analysis_case{"NegativeTenth", "-1e-1", number_kind::decimal, true, false, false},
      analysis_case{"CloseFraction", "1.0000000000000000000001", number_kind::decimal, false, false, false},
      analysis_case{"HugePositiveExponent", "1e99999999999999999999999", number_kind::decimal, false, false, true},
      analysis_case{"HugeNegativeExponent", "1e-99999999999999999999999", number_kind::decimal, false, false, false},
      analysis_case{"Hexadecimal", "0x1F", number_kind::hexadecimal, false, false, true},
      analysis_case{"NegativeHexadecimal", "-0Xab", number_kind::hexadecimal, true, false, true},
      analysis_case{"Infinity", "Infinity", number_kind::infinity, false, false, false},
      analysis_case{"NegativeInfinity", "-Infinity", number_kind::infinity, true, false, false},
      analysis_case{"NegativeNaN", "-NaN", number_kind::nan, true, false, false}),
    [](const ::testing::TestParamInfo<analysis_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify analysis charges one unit per spelling byte before scanning.
   * Arrange: Own "123.45" and budgets one unit short and exactly sufficient.
   * Act: Analyse with each budget.
   * Expect: resource_limit with no consumption, then success consuming the spelling size.
   */
  TEST(J5_036_NumberAnalysis, ChargesSpellingBytesBeforeScanning)
  {
    /* Arrange */
    guard_provider provider;
    const auto number = make_number(provider, "123.45");
    ASSERT_TRUE(number.has_value());
    work_budget short_budget{5U};
    work_budget exact_budget{6U};
    /* Act */
    const auto exhausted = detail::analyse_number(*number.value_if(), short_budget);
    const auto analysed = detail::analyse_number(*number.value_if(), exact_budget);
    /* Expect */
    ASSERT_FALSE(exhausted.has_value());
    EXPECT_EQ(exhausted.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(short_budget.consumed(), 0U);
    ASSERT_TRUE(analysed.has_value());
    EXPECT_EQ(exact_budget.consumed(), 6U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a moved-from number has no numeric value.
   * Arrange: Own "1" and move it into another number.
   * Act: Analyse the moved-from number.
   * Expect: Unlocated wrong_type_or_access and no work charge.
   */
  TEST(J5_036_NumberAnalysis, RejectsEmptySpelling)
  {
    /* Arrange */
    guard_provider provider;
    auto number = make_number(provider, "1");
    ASSERT_TRUE(number.has_value());
    const owned_number moved{std::move(*number.value_if())};
    work_budget work{c_ample_work};
    /* Act */
    const auto analysis = detail::analyse_number(*number.value_if(), work);
    /* Expect */
    ASSERT_FALSE(analysis.has_value());
    EXPECT_EQ(analysis.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_FALSE(analysis.error_if()->has_byte_offset);
    EXPECT_EQ(work.consumed(), 0U);
  }
  /* ================================= End Test_J5_036 ================================= */
  /* =================================== Test_J5_035 =================================== */
  /**
   * @brief Describe one bounded digit append and its outcome.
   * @par Requirements
   * P038; J5-035.
   */
  struct digit_case
  {
      const char* name;
      std::uintmax_t current;  // Input magnitude before the append.
      unsigned int digit;      // Input digit value.
      unsigned int radix;      // Input spelling base.
      std::uintmax_t maximum;  // Input inclusive magnitude bound.
      bool succeeds;           // Whether the append succeeds.
      std::uintmax_t expected; // Expected magnitude on success.
      error_code code;         // Expected error code on failure.
  };

  std::ostream& operator<<(std::ostream& stream, const digit_case& value)
  {
    return stream << value.name;
  }

  class IntegerDigitUnits : public ::testing::TestWithParam<digit_case>
  {
  };

  /**
   * @brief Describe one bounded integer-magnitude read and its outcome.
   * @par Requirements
   * P038; J5-035; J5-036; J5-038.
   */
  struct magnitude_case
  {
      const char* name;
      const char* spelling;             // Input JSON5 numeric spelling.
      std::uintmax_t maximum;           // Input inclusive magnitude bound.
      bool succeeds;                    // Whether reading succeeds.
      std::uintmax_t magnitude;         // Expected magnitude on success.
      bool negative;                    // Expected retained sign on success.
      numeric_conversion_reason reason; // Expected conversion reason on failure.
  };

  std::ostream& operator<<(std::ostream& stream, const magnitude_case& value)
  {
    return stream << value.name;
  }

  class IntegerMagnitudeUnits : public ::testing::TestWithParam<magnitude_case>
  {
  };

  constexpr auto c_out_of_range = numeric_conversion_reason::out_of_range;
  constexpr auto c_non_integral = numeric_conversion_reason::non_integral;
  constexpr auto c_non_finite = numeric_conversion_reason::non_finite;

  /* Verify digit appends are exact up to an inclusive bound and reject invalid input.
   * Arrange: Select the case magnitude, digit, radix and bound.
   * Act: Append the digit.
   * Expect: The exact product-plus-digit, out_of_range or wrong_type_or_access.
   */
  TEST_P(IntegerDigitUnits, AppendsWithinInclusiveBound)
  {
    /* Arrange */
    const digit_case& param = GetParam();
    /* Act */
    const auto appended = detail::append_integer_digit(param.current, param.digit, param.radix, param.maximum);
    /* Expect */
    ASSERT_EQ(appended.has_value(), param.succeeds);
    if (param.succeeds)
    {
      EXPECT_EQ(*appended.value_if(), param.expected);
      return;
    }
    EXPECT_EQ(appended.error_if()->code, param.code);
    if (param.code == error_code::numeric_conversion)
    {
      ASSERT_TRUE(appended.error_if()->numeric_reason.has_value());
      EXPECT_EQ(*appended.error_if()->numeric_reason, c_out_of_range);
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_035_IntegerDigit, IntegerDigitUnits,
    ::testing::Values(
      digit_case{"FirstDecimal", 0U, 9U, 10U, c_uintmax_max, true, 9U, error_code::numeric_conversion},
      digit_case{"Decimal", 12U, 3U, 10U, c_uintmax_max, true, 123U, error_code::numeric_conversion},
      digit_case{"Hexadecimal", 15U, 15U, 16U, c_uintmax_max, true, 255U, error_code::numeric_conversion},
      digit_case{"ZeroWithinZeroBound", 0U, 0U, 10U, 0U, true, 0U, error_code::numeric_conversion},
      digit_case{"ReachesMaximum", c_uintmax_max / 10U, static_cast<unsigned int>(c_uintmax_max % 10U), 10U,
                 c_uintmax_max, true, c_uintmax_max, error_code::numeric_conversion},
      digit_case{"ExceedsMaximumByOne", c_uintmax_max / 10U, static_cast<unsigned int>(c_uintmax_max % 10U) + 1U, 10U,
                 c_uintmax_max, false, 0U, error_code::numeric_conversion},
      digit_case{"ExceedsSmallBound", 25U, 6U, 10U, 255U, false, 0U, error_code::numeric_conversion},
      digit_case{"DigitAboveBound", 0U, 5U, 10U, 4U, false, 0U, error_code::numeric_conversion},
      digit_case{"DigitEqualsRadix", 0U, 10U, 10U, c_uintmax_max, false, 0U, error_code::wrong_type_or_access},
      digit_case{"UnsupportedRadix", 0U, 1U, 8U, c_uintmax_max, false, 0U, error_code::wrong_type_or_access}),
    [](const ::testing::TestParamInfo<digit_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify integral spellings read into an exact bounded magnitude with their sign.
   * Arrange: Own the case spelling.
   * Act: Read its magnitude under the case bound.
   * Expect: The exact magnitude and sign, or the case conversion reason.
   */
  TEST_P(IntegerMagnitudeUnits, ReadsExactBoundedMagnitude)
  {
    /* Arrange */
    const magnitude_case& param = GetParam();
    guard_provider provider;
    const auto number = make_number(provider, param.spelling);
    ASSERT_TRUE(number.has_value());
    work_budget work{c_ample_work};
    /* Act */
    const auto read = detail::read_integer_magnitude(*number.value_if(), param.maximum, work);
    /* Expect */
    ASSERT_EQ(read.has_value(), param.succeeds);
    if (param.succeeds)
    {
      EXPECT_EQ(read.value_if()->magnitude, param.magnitude);
      EXPECT_EQ(read.value_if()->negative, param.negative);
      return;
    }
    EXPECT_EQ(read.error_if()->code, error_code::numeric_conversion);
    ASSERT_TRUE(read.error_if()->numeric_reason.has_value());
    EXPECT_EQ(*read.error_if()->numeric_reason, param.reason);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_035_IntegerMagnitude, IntegerMagnitudeUnits,
    ::testing::Values(magnitude_case{"Zero", "0", 0U, true, 0U, false, c_out_of_range},
                      magnitude_case{"NegativeZeroKeepsSign", "-0", 0U, true, 0U, true, c_out_of_range},
                      magnitude_case{"NegativeHexZeroKeepsSign", "-0x0", 0U, true, 0U, true, c_out_of_range},
                      magnitude_case{"Decimal", "42", 255U, true, 42U, false, c_out_of_range},
                      magnitude_case{"PlusSign", "+7", 255U, true, 7U, false, c_out_of_range},
                      magnitude_case{"NegativeDecimal", "-42", 255U, true, 42U, true, c_out_of_range},
                      magnitude_case{"LowercaseHex", "0xff", 255U, true, 255U, false, c_out_of_range},
                      magnitude_case{"UppercaseHex", "0XFF", 255U, true, 255U, false, c_out_of_range},
                      magnitude_case{"PointZero", "1.0", 255U, true, 1U, false, c_out_of_range},
                      magnitude_case{"PositiveExponent", "1e2", 255U, true, 100U, false, c_out_of_range},
                      magnitude_case{"FractionScaled", "1.5e1", 255U, true, 15U, false, c_out_of_range},
                      magnitude_case{"NegativeExponentTrailingZeros", "100e-2", 255U, true, 1U, false, c_out_of_range},
                      magnitude_case{"FractionalTrailingZeros", "123.4500e2", c_uintmax_max, true, 12345U, false,
                                     c_out_of_range},
                      magnitude_case{"DecimalAtBound", "255", 255U, true, 255U, false, c_out_of_range},
                      magnitude_case{"DecimalAboveBound", "256", 255U, false, 0U, false, c_out_of_range},
                      magnitude_case{"HexAboveBound", "0x100", 255U, false, 0U, false, c_out_of_range},
                      magnitude_case{"ExponentAboveBound", "2.56e2", 255U, false, 0U, false, c_out_of_range},
                      magnitude_case{"ImpliedZerosOverflow", "1e400", c_uintmax_max, false, 0U, false, c_out_of_range},
                      magnitude_case{"ExponentDigitsOverflow", "1e99999999999999999999999", c_uintmax_max, false, 0U,
                                     false, c_out_of_range},
                      magnitude_case{"Fraction", "1.5", 255U, false, 0U, false, c_non_integral},
                      magnitude_case{"NegativeFraction", "-1.5", 255U, false, 0U, false, c_non_integral},
                      magnitude_case{"NegativeExponentFraction", "1e-1", 255U, false, 0U, false, c_non_integral},
                      magnitude_case{"Infinity", "Infinity", 255U, false, 0U, false, c_non_finite},
                      magnitude_case{"NaN", "NaN", 255U, false, 0U, false, c_non_finite}),
    [](const ::testing::TestParamInfo<magnitude_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify exponent-implied zeros charge work and exhaustion is reported.
   * Arrange: Own "1e5"; analysis and scan charge 3 units each and five zeros charge 5.
   * Act: Read with budgets of 10 and 11 units.
   * Expect: resource_limit, then 100000 consuming exactly 11 units.
   */
  TEST(J5_035_IntegerMagnitude, ChargesImpliedZeros)
  {
    /* Arrange */
    guard_provider provider;
    const auto number = make_number(provider, "1e5");
    ASSERT_TRUE(number.has_value());
    work_budget short_budget{10U};
    work_budget exact_budget{11U};
    /* Act */
    const auto exhausted = detail::read_integer_magnitude(*number.value_if(), c_uintmax_max, short_budget);
    const auto read = detail::read_integer_magnitude(*number.value_if(), c_uintmax_max, exact_budget);
    /* Expect */
    ASSERT_FALSE(exhausted.has_value());
    EXPECT_EQ(exhausted.error_if()->code, error_code::resource_limit);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read.value_if()->magnitude, 100000U);
    EXPECT_EQ(exact_budget.consumed(), 11U);
  }
  /* ================================= End Test_J5_035 ================================= */
} // namespace
