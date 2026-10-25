#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <ostream>
#include <string_view>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  constexpr std::size_t c_max_spelling_bytes = 4096;
  // Extreme values legitimately need more than the default work ceiling.
  constexpr std::size_t c_ample_work = 10000000;
  constexpr std::size_t c_ample_scratch = 1048576;
  constexpr std::size_t c_max_injected_failures = 1024;

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

  /**
   * @brief Compare two floating values bit for bit, treating any NaN as equal to NaN.
   * @par Requirements
   * P038; J5-037; J5-038.
   * @tparam T float or double.
   * @return True when both are NaN or have identical object representations.
   * @note Bitwise comparison distinguishes positive and negative zero.
   */
  template <typename T>
  bool same_value(T actual, T expected)
  {
    if (std::isnan(expected))
    {
      return std::isnan(actual);
    }
    return std::memcmp(&actual, &expected, sizeof(T)) == 0;
  }

  /**
   * @brief Describe one floating conversion, its policies and its outcome.
   * @par Requirements
   * P038; J5-037; J5-038.
   * @note Float expectations are stored as double; every float value is exactly representable there.
   */
  struct floating_case
  {
      const char* name;
      const char* spelling;             // Input JSON5 numeric spelling.
      bool allow_zero;                  // Input underflow policy: allow_zero when true, otherwise reject.
      bool allow_non_finite;            // Input non-finite policy: allow when true, otherwise reject.
      bool succeeds;                    // Whether conversion succeeds.
      double expected;                  // Expected value on success; NaN means any NaN.
      numeric_conversion_reason reason; // Expected conversion reason on failure.
  };

  std::ostream& operator<<(std::ostream& stream, const floating_case& value)
  {
    return stream << value.name;
  }

  /**
   * @brief Build options with ample limits and the case policies.
   * @par Requirements
   * P038; J5-037; J5-038.
   */
  floating_conversion_options options_for(const floating_case& param)
  {
    floating_conversion_options options{};
    options.max_work_units = c_ample_work;
    options.max_scratch_bytes = c_ample_scratch;
    options.underflow = param.allow_zero ? floating_underflow_policy::allow_zero : floating_underflow_policy::reject;
    options.non_finite =
      param.allow_non_finite ? floating_non_finite_policy::allow : floating_non_finite_policy::reject;
    return options;
  }

  constexpr auto c_out_of_range = numeric_conversion_reason::out_of_range;
  constexpr auto c_underflow = numeric_conversion_reason::underflow;
  constexpr auto c_non_finite = numeric_conversion_reason::non_finite;
  constexpr double c_infinity = std::numeric_limits<double>::infinity();
  constexpr double c_nan = std::numeric_limits<double>::quiet_NaN();

  /**
   * @brief Build a successful case under default policies.
   * @par Requirements
   * P038; J5-037.
   */
  constexpr floating_case converts(const char* name, const char* spelling, double expected)
  {
    return floating_case{name, spelling, false, false, true, expected, c_out_of_range};
  }

  /**
   * @brief Build a failing case under default policies.
   * @par Requirements
   * P038; J5-037; J5-038.
   */
  constexpr floating_case rejects(const char* name, const char* spelling, numeric_conversion_reason reason)
  {
    return floating_case{name, spelling, false, false, false, 0.0, reason};
  }

  /**
   * @brief Build a successful case with permissive underflow and non-finite policies.
   * @par Requirements
   * P038; J5-037; J5-038.
   */
  constexpr floating_case converts_permissive(const char* name, const char* spelling, double expected)
  {
    return floating_case{name, spelling, true, true, true, expected, c_out_of_range};
  }

  /**
   * @brief Build a failing case with permissive underflow and non-finite policies.
   * @par Requirements
   * P038; J5-037; J5-038.
   */
  constexpr floating_case rejects_permissive(const char* name, const char* spelling, numeric_conversion_reason reason)
  {
    return floating_case{name, spelling, true, true, false, 0.0, reason};
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_037 =================================== */
  static_assert(noexcept(to_double(std::declval<const owned_number&>(), std::declval<const storage_callbacks&>())));
  static_assert(noexcept(to_float(std::declval<const owned_number&>(), std::declval<const storage_callbacks&>())));

  class DoubleConversionUnits : public ::testing::TestWithParam<floating_case>
  {
  };

  class FloatConversionUnits : public ::testing::TestWithParam<floating_case>
  {
  };

  /* Verify correctly rounded double conversion and its documented failure reasons.
   * Arrange: Own the case spelling and build options from the case policies.
   * Act: Convert it to double.
   * Expect: The bit-exact value, or numeric_conversion with the case reason; scratch is released.
   */
  TEST_P(DoubleConversionUnits, RoundsOnceOrReportsReason)
  {
    /* Arrange */
    const floating_case& param = GetParam();
    guard_provider number_storage;
    guard_provider scratch;
    const auto number = make_number(number_storage, param.spelling);
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto converted = to_double(*number.value_if(), scratch.callbacks(), options_for(param));
    /* Expect */
    EXPECT_EQ(scratch.m_live_bytes, 0U);
    ASSERT_EQ(converted.has_value(), param.succeeds);
    if (param.succeeds)
    {
      EXPECT_TRUE(same_value(*converted.value_if(), param.expected))
        << *converted.value_if() << " versus " << param.expected;
      return;
    }
    EXPECT_EQ(converted.error_if()->code, error_code::numeric_conversion);
    ASSERT_TRUE(converted.error_if()->numeric_reason.has_value());
    EXPECT_EQ(*converted.error_if()->numeric_reason, param.reason);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_037_DoubleConversion, DoubleConversionUnits,
    ::testing::Values(
      converts("Tenth", "0.1", 0x1.999999999999ap-4), converts("NegativeFraction", "-2.5", -2.5),
      converts("LeadingPoint", ".5", 0.5), converts("TrailingPoint", "5.", 5.0),
      converts("UppercaseExponent", "1E5", 1e5), converts("SignedExponent", "1e+5", 1e5),
      converts("NegativeScaled", "-0.1e-2", -0.001), converts("ClassicHardCase", "1e23", 1e23),
      converts("ManyDigits", "123456789012345678901234567890", 123456789012345678901234567890.0),
      converts("Hexadecimal", "0x1F", 31.0), converts("WideHexRoundsUp", "0xFFFFFFFFFFFFFFFFFF", 0x1p72),
      converts("WideHexBeyondFloat", "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", 0x1p136),
      converts("TieRoundsToEvenDown", "9007199254740993", 0x1p53),
      converts("TieRoundsToEvenUp", "9007199254740995", 9007199254740996.0),
      converts("StickyBreaksTie", "9007199254740993.0000000000000001", 9007199254740994.0),
      converts("Maximum", "1.7976931348623157e308", 0x1.fffffffffffffp1023),
      converts("MinimumNormal", "2.2250738585072014e-308", 0x1p-1022),
      converts("LargestSubnormal", "2.2250738585072009e-308", 0x0.fffffffffffffp-1022),
      converts("MinimumSubnormal", "4.9406564584124654e-324", 0x1p-1074),
      converts("AboveHalfMinimumSubnormal", "2.4703282292062328e-324", 0x1p-1074), converts("PositiveZero", "0", 0.0),
      converts("NegativeZero", "-0", -0.0), converts("NegativeZeroHugeExponent", "-0.0e-999999", -0.0),
      converts("NegativeHexZero", "-0x0", -0.0), rejects("AboveMaximum", "1.7976931348623159e308", c_out_of_range),
      rejects("ExponentOverflow", "1e309", c_out_of_range), rejects("NegativeOverflow", "-1e400", c_out_of_range),
      rejects("SaturatedExponent", "1e99999999999999999999999", c_out_of_range),
      rejects("BelowHalfMinimumSubnormal", "2.4703282292062327e-324", c_underflow),
      rejects("FarUnderflow", "1e-400", c_underflow),
      rejects("SaturatedNegativeExponent", "-1e-99999999999999999999999", c_underflow),
      rejects("Infinity", "Infinity", c_non_finite), rejects("NegativeInfinity", "-Infinity", c_non_finite),
      rejects("NaN", "NaN", c_non_finite), converts_permissive("UnderflowToZero", "2.4703282292062327e-324", 0.0),
      converts_permissive("NegativeUnderflowToZero", "-1e-400", -0.0),
      converts_permissive("AllowedInfinity", "Infinity", c_infinity),
      converts_permissive("AllowedNegativeInfinity", "-Infinity", -c_infinity),
      converts_permissive("AllowedNaN", "+NaN", c_nan),
      rejects_permissive("FiniteOverflowIgnoresNonFinitePolicy", "1e309", c_out_of_range)),
    [](const ::testing::TestParamInfo<floating_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify correctly rounded float conversion directly, not through double.
   * Arrange: Own the case spelling and build options from the case policies.
   * Act: Convert it to float.
   * Expect: The bit-exact value, or numeric_conversion with the case reason; scratch is released.
   */
  TEST_P(FloatConversionUnits, RoundsOnceOrReportsReason)
  {
    /* Arrange */
    const floating_case& param = GetParam();
    guard_provider number_storage;
    guard_provider scratch;
    const auto number = make_number(number_storage, param.spelling);
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto converted = to_float(*number.value_if(), scratch.callbacks(), options_for(param));
    /* Expect */
    EXPECT_EQ(scratch.m_live_bytes, 0U);
    ASSERT_EQ(converted.has_value(), param.succeeds);
    if (param.succeeds)
    {
      EXPECT_TRUE(same_value(*converted.value_if(), static_cast<float>(param.expected)))
        << *converted.value_if() << " versus " << param.expected;
      return;
    }
    EXPECT_EQ(converted.error_if()->code, error_code::numeric_conversion);
    ASSERT_TRUE(converted.error_if()->numeric_reason.has_value());
    EXPECT_EQ(*converted.error_if()->numeric_reason, param.reason);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_037_FloatConversion, FloatConversionUnits,
    ::testing::Values(
      converts("Tenth", "0.1", 0x1.99999ap-4), converts("Hexadecimal", "0x1F", 31.0),
      converts("TieRoundsToEvenDown", "16777217", 16777216.0), converts("TieRoundsToEvenUp", "16777219", 16777220.0),
      converts("HexTieRoundsToEven", "0x1000001", 16777216.0),
      converts("NoDoubleRounding", "1.00000005960464477550", 0x1.000002p0),
      converts("Maximum", "3.4028235e38", 0x1.fffffep127),
      converts("MinimumNormal", "1.17549435082228750797e-38", 0x1p-126),
      converts("MinimumSubnormal", "1.40129846e-45", 0x1p-149),
      converts("AboveHalfMinimumSubnormal", "7.1e-46", 0x1p-149), converts("NegativeZero", "-0.0", -0.0),
      rejects("AboveMaximum", "3.4028236e38", c_out_of_range), rejects("ExponentOverflow", "1e39", c_out_of_range),
      rejects("WideHexOverflow", "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", c_out_of_range),
      rejects("BelowHalfMinimumSubnormal", "7e-46", c_underflow),
      rejects(
        "ExactHalfMinimumSubnormal",
        "7.00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094181060791015625e-"
        "46",
        c_underflow),
      rejects("NegativeInfinity", "-Infinity", c_non_finite),
      converts_permissive(
        "ExactHalfToEvenZero",
        "7.00649232162408535461864791644958065640130970938257885878534141944895541342930300743319094181060791015625e-"
        "46",
        0.0),
      converts_permissive("NegativeUnderflowToZero", "-7e-46", -0.0),
      converts_permissive("AllowedInfinity", "Infinity", c_infinity), converts_permissive("AllowedNaN", "NaN", c_nan)),
    [](const ::testing::TestParamInfo<floating_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify default options convert ordinary values and reject special and underflowing values.
   * Arrange: Own "0.1", "NaN" and "1e-400".
   * Act: Convert each to double with default options.
   * Expect: 0.1, non_finite and underflow.
   */
  TEST(J5_037_FloatingConversion, DefaultOptionsAreConservative)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    const auto tenth = make_number(number_storage, "0.1");
    const auto nan = make_number(number_storage, "NaN");
    const auto tiny = make_number(number_storage, "1e-400");
    ASSERT_TRUE(tenth.has_value());
    ASSERT_TRUE(nan.has_value());
    ASSERT_TRUE(tiny.has_value());
    /* Act */
    const auto converted = to_double(*tenth.value_if(), scratch.callbacks());
    const auto special = to_double(*nan.value_if(), scratch.callbacks());
    const auto underflow = to_double(*tiny.value_if(), scratch.callbacks());
    /* Expect */
    ASSERT_TRUE(converted.has_value());
    EXPECT_EQ(*converted.value_if(), 0.1);
    ASSERT_FALSE(special.has_value());
    ASSERT_TRUE(special.error_if()->numeric_reason.has_value());
    EXPECT_EQ(*special.error_if()->numeric_reason, c_non_finite);
    ASSERT_FALSE(underflow.has_value());
    ASSERT_TRUE(underflow.error_if()->numeric_reason.has_value());
    EXPECT_EQ(*underflow.error_if()->numeric_reason, c_underflow);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify conversion leaves the original spelling unchanged and repeats deterministically.
   * Arrange: Own "1.50e1".
   * Act: Convert it to double twice and to float once.
   * Expect: 15 each time and the spelling still reads "1.50e1".
   */
  TEST(J5_037_FloatingConversion, PreservesSpellingAndRepeats)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    const auto number = make_number(number_storage, "1.50e1");
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto first = to_double(*number.value_if(), scratch.callbacks());
    const auto second = to_double(*number.value_if(), scratch.callbacks());
    const auto single = to_float(*number.value_if(), scratch.callbacks());
    /* Expect */
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(single.has_value());
    EXPECT_EQ(*first.value_if(), 15.0);
    EXPECT_EQ(*second.value_if(), 15.0);
    EXPECT_EQ(*single.value_if(), 15.0F);
    const auto spelling = number.value_if()->spelling();
    EXPECT_EQ(std::string_view(spelling.data, spelling.size), "1.50e1");
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a moved-from number is rejected by both conversions.
   * Arrange: Own "1" and move it into another number.
   * Act: Convert the moved-from number to double and float.
   * Expect: wrong_type_or_access without scratch allocation.
   */
  TEST(J5_037_FloatingConversion, RejectsEmptySpelling)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    auto number = make_number(number_storage, "1");
    ASSERT_TRUE(number.has_value());
    const owned_number moved{std::move(*number.value_if())};
    /* Act */
    const auto as_double = to_double(*number.value_if(), scratch.callbacks());
    const auto as_float = to_float(*number.value_if(), scratch.callbacks());
    /* Expect */
    ASSERT_FALSE(as_double.has_value());
    EXPECT_EQ(as_double.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(as_float.has_value());
    EXPECT_EQ(as_float.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(scratch.m_attempts, 0U);
  }
  /* ================================= End Test_J5_037 ================================= */
  /* =================================== Test_J5_048 =================================== */
  /* Verify work and scratch ceilings return resource_limit and release scratch.
   * Arrange: Own "0.1"; build options with zero work and with zero scratch bytes.
   * Act: Convert to double under each option set.
   * Expect: resource_limit for both, with no outstanding scratch.
   */
  TEST(J5_048_FloatingConversion, EnforcesWorkAndScratchCeilings)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    const auto number = make_number(number_storage, "0.1");
    ASSERT_TRUE(number.has_value());
    floating_conversion_options no_work{};
    no_work.max_work_units = 0U;
    floating_conversion_options no_scratch{};
    no_scratch.max_scratch_bytes = 0U;
    /* Act */
    const auto work_limited = to_double(*number.value_if(), scratch.callbacks(), no_work);
    const auto scratch_limited = to_double(*number.value_if(), scratch.callbacks(), no_scratch);
    /* Expect */
    ASSERT_FALSE(work_limited.has_value());
    EXPECT_EQ(work_limited.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(scratch_limited.has_value());
    EXPECT_EQ(scratch_limited.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(scratch.m_live_bytes, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify every scratch allocation failure is reported and fully released.
   * Arrange: Own a subnormal spelling whose conversion needs several scratch allocations.
   * Act: Fail the first, second, ... scratch request in fresh providers until conversion succeeds.
   * Expect: allocation_failure with no outstanding scratch for each failure, then the exact value.
   */
  TEST(J5_048_FloatingConversion, FailsEveryScratchAllocationAndAllowsRetry)
  {
    /* Arrange */
    guard_provider number_storage;
    const auto number = make_number(number_storage, "4.9406564584124654e-324");
    ASSERT_TRUE(number.has_value());
    floating_conversion_options options{};
    options.max_work_units = c_ample_work;
    options.max_scratch_bytes = c_ample_scratch;
    std::size_t failures{};
    bool succeeded{};
    /* Act */
    for (std::size_t fail_on = 1U; (fail_on <= c_max_injected_failures) && !succeeded; ++fail_on)
    {
      guard_provider scratch;
      scratch.m_fail_on = fail_on;
      const auto converted = to_double(*number.value_if(), scratch.callbacks(), options);
      EXPECT_EQ(scratch.m_live_bytes, 0U) << "fail_on " << fail_on;
      if (converted.has_value())
      {
        succeeded = true;
        EXPECT_TRUE(same_value(*converted.value_if(), 0x1p-1074));
        continue;
      }
      EXPECT_EQ(converted.error_if()->code, error_code::allocation_failure) << "fail_on " << fail_on;
      ++failures;
    }
    /* Expect */
    EXPECT_TRUE(succeeded);
    EXPECT_GT(failures, 1U);
  }
  /* ================================= End Test_J5_048 ================================= */
} // namespace
