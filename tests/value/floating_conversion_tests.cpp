#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
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

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Whether Clinger's exact fast path is compiled in for this host.
   * @par Requirements
   * J5-037.
   * @note It needs floating arithmetic without extra precision (FLT_EVAL_METHOD == 0).
   */
#if defined(FLT_EVAL_METHOD) && (FLT_EVAL_METHOD == 0)
  constexpr bool c_clinger_enabled = true;
#else
  constexpr bool c_clinger_enabled = false;
#endif

  /**
   * @brief Whether the library was built with the Eisel-Lemire step (ADAPTIVE_JSON5_EISEL_LEMIRE).
   * @par Requirements
   * J5-037.
   */
  constexpr bool c_eisel_lemire_enabled = (ADAPTIVE_JSON5_EXPECT_EISEL_LEMIRE != 0);

  /**
   * @brief Which scratch-free step, if any, can convert a spelling.
   * @par Requirements
   * J5-037.
   */
  enum class fast_step
  {
    clinger,      // Exact with one multiply or divide.
    eisel_lemire, // Needs ADAPTIVE_JSON5_EISEL_LEMIRE.
    none          // Always takes big-integer scratch.
  };

  /**
   * @brief One spelling, its correctly rounded value and the step expected to convert it.
   * @par Requirements
   * J5-037.
   * @note Float expectations are stored as double; every float value is exactly representable there.
   */
  struct fast_path_case
  {
      const char* name;
      const char* spelling; // Input JSON5 numeric spelling.
      double expected;      // Expected correctly rounded value.
      fast_step step;       // Expected scratch-free step, or none.
  };

  std::ostream& operator<<(std::ostream& stream, const fast_path_case& value)
  {
    return stream << value.name;
  }

  /**
   * @brief Report whether a case should convert without requesting scratch on this build.
   * @par Requirements
   * J5-037.
   * @param param The case; its step is the first scratch-free step that applies.
   * @note A Clinger case is also an Eisel-Lemire case when it is decimal, because every
   * Clinger case has at most 19 digits and a normal result. So without Clinger (for example
   * FLT_EVAL_METHOD != 0), a decimal Clinger case is still scratch-free when Eisel-Lemire is
   * built; a hexadecimal one is not, since Eisel-Lemire handles decimals only.
   */
  bool expect_scratch_free(const fast_path_case& param)
  {
    const std::string_view spelling{param.spelling};
    const bool decimal =
      (spelling.find('x') == std::string_view::npos) && (spelling.find('X') == std::string_view::npos);

    switch (param.step)
    {
    case fast_step::clinger:
      return c_clinger_enabled || (decimal && c_eisel_lemire_enabled);
    case fast_step::eisel_lemire:
      return c_eisel_lemire_enabled;
    case fast_step::none:
      break;
    }
    return false;
  }

  class DoubleFastPathUnits : public ::testing::TestWithParam<fast_path_case>
  {
  };

  class FloatFastPathUnits : public ::testing::TestWithParam<fast_path_case>
  {
  };

  /* Verify which doubles are read without scratch, and that every path gives the exact value.
   * Arrange: Own the case spelling; a guarded scratch provider and an empty storage_callbacks{}.
   * Act: Convert it to double with each provider.
   * Expect: The bit-exact value through the guarded provider. Scratch-free cases request no
   *         scratch and also succeed with storage_callbacks{}; the others request scratch,
   *         release it, and report allocation_failure with storage_callbacks{}.
   */
  TEST_P(DoubleFastPathUnits, ConvertsWithoutScratchOnlyInsideTheFastRange)
  {
    /* Arrange */
    const fast_path_case& param = GetParam();
    const bool scratch_free = expect_scratch_free(param);
    guard_provider number_storage;
    guard_provider scratch;
    const auto number = make_number(number_storage, param.spelling);
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto converted = to_double(*number.value_if(), scratch.callbacks());
    const auto without_scratch = to_double(*number.value_if(), storage_callbacks{});
    /* Expect */
    ASSERT_TRUE(converted.has_value());
    EXPECT_TRUE(same_value(*converted.value_if(), param.expected))
      << *converted.value_if() << " versus " << param.expected;
    EXPECT_EQ(scratch.m_attempts == 0U, scratch_free) << scratch.m_attempts << " scratch requests";
    EXPECT_EQ(scratch.m_live_bytes, 0U);
    ASSERT_EQ(without_scratch.has_value(), scratch_free);
    if (scratch_free)
    {
      EXPECT_TRUE(same_value(*without_scratch.value_if(), param.expected));
      return;
    }
    EXPECT_EQ(without_scratch.error_if()->code, error_code::allocation_failure);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_037_DoubleFastPath, DoubleFastPathUnits,
    ::testing::Values(
      fast_path_case{"Integer", "8080", 8080.0, fast_step::clinger},
      fast_path_case{"NegativeFraction", "-1.5", -1.5, fast_step::clinger},
      fast_path_case{"Tenth", "0.1", 0x1.999999999999ap-4, fast_step::clinger},
      fast_path_case{"LargestExactSignificand", "9007199254740992", 0x1p53, fast_step::clinger},
      fast_path_case{"LargestExactPowerOfTen", "1e22", 1e22, fast_step::clinger},
      fast_path_case{"DivideByLargestPower", "9007199254740991e-22", 9007199254740991e-22, fast_step::clinger},
      fast_path_case{"Hexadecimal", "0x1FFFFFFFFFFFFF", 9007199254740991.0, fast_step::clinger},
      fast_path_case{"SignificandAboveTwoTo53", "9007199254740993", 0x1p53, fast_step::eisel_lemire},
      fast_path_case{"TieRoundsToEvenUp", "9007199254740995", 9007199254740996.0, fast_step::eisel_lemire},
      fast_path_case{"PowerOfTenAbove22", "1e23", 1e23, fast_step::eisel_lemire},
      fast_path_case{"NineteenDigits", "1234567890123456789", 1234567890123456789.0, fast_step::eisel_lemire},
      // 602214076 * 10^15: inside Clinger's range despite the large written exponent.
      fast_path_case{"Avogadro", "6.02214076e23", 6.02214076e23, fast_step::clinger},
      fast_path_case{"ExactDigitsAboveTwoTo53", "6.0221407600000001e23", 6.0221407600000001e23,
                     fast_step::eisel_lemire},
      fast_path_case{"Maximum", "1.7976931348623157e308", 0x1.fffffffffffffp1023, fast_step::eisel_lemire},
      fast_path_case{"MinimumNormal", "2.2250738585072014e-308", 0x1p-1022, fast_step::eisel_lemire},
      fast_path_case{"TwentyDigits", "12345678901234567890", 12345678901234567890.0, fast_step::none},
      fast_path_case{"LeadingZerosCount", "0.00000000000000000001", 1e-20, fast_step::none},
      fast_path_case{"Subnormal", "4.9406564584124654e-324", 0x1p-1074, fast_step::none},
      fast_path_case{"SeventeenHexDigits", "0x10000000000000000", 0x1p64, fast_step::none}),
    [](const ::testing::TestParamInfo<fast_path_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify which floats are read without scratch, rounded once and never through double.
   * Arrange: Own the case spelling; a guarded scratch provider and an empty storage_callbacks{}.
   * Act: Convert it to float with each provider.
   * Expect: As for doubles, with float's Clinger range (significand up to 2^24, power of ten
   *         up to 10).
   */
  TEST_P(FloatFastPathUnits, ConvertsWithoutScratchOnlyInsideTheFastRange)
  {
    /* Arrange */
    const fast_path_case& param = GetParam();
    const bool scratch_free = expect_scratch_free(param);
    guard_provider number_storage;
    guard_provider scratch;
    const auto number = make_number(number_storage, param.spelling);
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto converted = to_float(*number.value_if(), scratch.callbacks());
    const auto without_scratch = to_float(*number.value_if(), storage_callbacks{});
    /* Expect */
    ASSERT_TRUE(converted.has_value());
    EXPECT_TRUE(same_value(*converted.value_if(), static_cast<float>(param.expected)))
      << *converted.value_if() << " versus " << param.expected;
    EXPECT_EQ(scratch.m_attempts == 0U, scratch_free) << scratch.m_attempts << " scratch requests";
    EXPECT_EQ(scratch.m_live_bytes, 0U);
    ASSERT_EQ(without_scratch.has_value(), scratch_free);
    if (scratch_free)
    {
      EXPECT_TRUE(same_value(*without_scratch.value_if(), static_cast<float>(param.expected)));
      return;
    }
    EXPECT_EQ(without_scratch.error_if()->code, error_code::allocation_failure);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_037_FloatFastPath, FloatFastPathUnits,
    ::testing::Values(fast_path_case{"Tenth", "0.1", 0x1.99999ap-4, fast_step::clinger},
                      fast_path_case{"LargestExactSignificand", "16777216", 16777216.0, fast_step::clinger},
                      fast_path_case{"LargestExactPowerOfTen", "1e10", 1e10, fast_step::clinger},
                      fast_path_case{"TieRoundsToEvenDown", "16777217", 16777216.0, fast_step::eisel_lemire},
                      fast_path_case{"PowerOfTenAbove10", "1e11", 99999997952.0, fast_step::eisel_lemire},
                      // Through double this would round to 1 + 2^-24, a tie, and then to 1.0.
                      fast_path_case{"NoDoubleRounding", "1.0000000596046448", 0x1.000002p0, fast_step::eisel_lemire},
                      fast_path_case{"Maximum", "3.4028235e38", 0x1.fffffep127, fast_step::eisel_lemire},
                      fast_path_case{"Subnormal", "1.40129846e-45", 0x1p-149, fast_step::none},
                      fast_path_case{"TwentyDigits", "16777217.000000000000", 16777216.0, fast_step::none}),
    [](const ::testing::TestParamInfo<fast_path_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Small deterministic generator so failures reproduce on every host.
   * @par Requirements
   * J5-037.
   * @note SplitMix64; any fixed sequence would do.
   */
  std::uint64_t next_random(std::uint64_t& state)
  {
    state += 0x9E3779B97F4A7C15U;
    std::uint64_t mixed = state;
    mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9U;
    mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBU;
    return mixed ^ (mixed >> 31U);
  }

  /**
   * @brief Convert a spelling and its zero-padded twin, and compare the outcomes.
   * @par Requirements
   * J5-037; J5-038.
   * @tparam T float or double.
   * @param significand Up to 19 decimal digits, first digit nonzero.
   * @param exponent Decimal exponent.
   * @return Empty on agreement, otherwise a description of the mismatch.
   * @note The padded twin has more than 19 digits, so it always takes the exact big-integer
   * path; the short spelling takes Clinger or Eisel-Lemire when they apply. The two must agree
   * bit for bit, including the error for out-of-range values.
   */
  template <typename T>
  std::string compare_with_exact_path(const std::string& significand, std::int64_t exponent)
  {
    const std::string exponent_text = "e" + std::to_string(exponent);
    const std::string short_text = significand + exponent_text;
    const std::string padded_text = significand + ".000000000000000000000" + exponent_text;
    guard_provider number_storage;
    guard_provider scratch;
    const auto short_number = make_number(number_storage, short_text.c_str());
    const auto padded_number = make_number(number_storage, padded_text.c_str());
    if (!short_number.has_value() || !padded_number.has_value())
    {
      return short_text + ": spelling rejected";
    }

    floating_conversion_options options{};
    options.max_work_units = c_ample_work;
    options.max_scratch_bytes = c_ample_scratch;
    const auto convert = [&](const owned_number& number)
    {
      if constexpr (std::is_same_v<T, float>)
      {
        return to_float(number, scratch.callbacks(), options);
      }
      else
      {
        return to_double(number, scratch.callbacks(), options);
      }
    };
    const auto fast = convert(*short_number.value_if());
    const std::size_t requests_before = scratch.m_attempts;
    const auto exact = convert(*padded_number.value_if());

    if (exact.has_value() && (scratch.m_attempts == requests_before))
    {
      return padded_text + ": did not take the big-integer path";
    }
    if (fast.has_value() != exact.has_value())
    {
      return short_text + ": success differs from the exact path";
    }
    if (!fast.has_value())
    {
      const bool same_error = (fast.error_if()->code == exact.error_if()->code) &&
                              (fast.error_if()->numeric_reason == exact.error_if()->numeric_reason);
      return same_error ? std::string{} : short_text + ": error differs from the exact path";
    }
    return same_value(*fast.value_if(), *exact.value_if()) ? std::string{} : short_text + ": value differs";
  }

  /* Verify the scratch-free steps agree bit for bit with the exact big-integer path.
   * Arrange: Hand-picked halfway, boundary and subnormal spellings, then 20000 deterministic
   *          pseudo-random 1..19-digit significands with exponents across the whole table range.
   * Act: Convert each spelling, and the same value padded beyond 19 digits, to double and float.
   * Expect: Identical values or identical errors; the padded spelling always takes scratch.
   */
  TEST(J5_037_FloatingConversion, ScratchFreeStepsMatchTheExactPath)
  {
    /* Arrange */
    struct fixed_case
    {
        const char* significand; // Input significand digits.
        std::int64_t exponent;   // Input decimal exponent.
    };
    const fixed_case fixed[] = {
      {"9007199254740993", 0},     // Double tie, rounds down to even.
      {"9007199254740995", 0},     // Double tie, rounds up to even.
      {"18014398509481986", 0},    // 2^54 + 2, double tie.
      {"18014398509481990", 0},    // 2^54 + 6, double tie.
      {"16777217", 0},             // Float tie.
      {"16777219", 0},             // Float tie.
      {"33554434", 0},             // 2^25 + 2, float tie.
      {"17976931348623157", 292},  // Largest double.
      {"17976931348623159", 292},  // Overflows double.
      {"22250738585072014", -324}, // Smallest normal double.
      {"22250738585072011", -324}, // Just below the smallest normal double.
      {"49406564584124654", -340}, // Smallest subnormal double.
      {"34028235", 31},            // Largest float.
      {"11754943508222875", -54},  // Smallest normal float.
      {"1", -342},                 // Smallest table power.
      {"1", 308},                  // Largest table power.
      {"9999999999999999999", 0},  // Largest 19-digit significand.
      {"1000000000000000000", 0},  // Smallest 19-digit significand.
    };
    std::size_t mismatches{};
    std::uint64_t state = 124U;
    for (const fixed_case& item : fixed)
    {
      /* Act */
      const std::string as_double = compare_with_exact_path<double>(item.significand, item.exponent);
      const std::string as_float = compare_with_exact_path<float>(item.significand, item.exponent);
      /* Expect */
      EXPECT_TRUE(as_double.empty()) << "double " << as_double;
      EXPECT_TRUE(as_float.empty()) << "float " << as_float;
    }
    for (std::size_t round{}; round < 20000U; ++round)
    {
      const std::size_t digit_count = 1U + static_cast<std::size_t>(next_random(state) % 19U);
      std::string significand(1U, static_cast<char>('1' + (next_random(state) % 9U)));
      while (significand.size() < digit_count)
      {
        significand.push_back(static_cast<char>('0' + (next_random(state) % 10U)));
      }
      // The scaled power (exponent + digits - 1) covers -350..319, the table range and a little beyond.
      const std::int64_t exponent =
        -350 + static_cast<std::int64_t>(next_random(state) % 670U) - static_cast<std::int64_t>(digit_count) + 1;
      /* Act */
      const std::string as_double = compare_with_exact_path<double>(significand, exponent);
      const std::string as_float = compare_with_exact_path<float>(significand, exponent);
      /* Expect */
      if (!as_double.empty() || !as_float.empty())
      {
        ++mismatches;
        ADD_FAILURE() << "double " << as_double << "; float " << as_float;
      }
      ASSERT_LT(mismatches, 10U) << "stopping after repeated mismatches";
    }
  }
  /* ================================= End Test_J5_037 ================================= */
  /* =================================== Test_J5_048 =================================== */
  /* Verify work and scratch ceilings return resource_limit and release scratch.
   * Arrange: Own a 27-digit spelling, too long for any scratch-free fast path; build options
   * with zero work and with zero scratch bytes.
   * Act: Convert to double under each option set.
   * Expect: resource_limit for both, with no outstanding scratch.
   */
  TEST(J5_048_FloatingConversion, EnforcesWorkAndScratchCeilings)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    const auto number = make_number(number_storage, "0.100000000000000000000000001");
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
