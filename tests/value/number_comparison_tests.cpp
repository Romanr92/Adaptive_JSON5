#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/number_comparison.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  constexpr std::size_t c_max_spelling_bytes = 4096;
  constexpr std::size_t c_ample_work = 10000000;
  constexpr std::size_t c_ample_scratch = 1048576;
  constexpr std::size_t c_max_injected_failures = 1024;

  /**
   * @brief Validate and own one JSON5 numeric spelling for a unit.
   * @par Requirements
   * P038; J5-034.
   * @param provider Guarded storage that must outlive the returned number.
   * @param text Numeric spelling.
   * @return Owned number or the validation/allocation error.
   */
  result<owned_number> make_number(guard_provider& provider, std::string_view text)
  {
    return owned_number::copy_spelling(input_view{text.data(), text.size()}, syntax_mode::json5, provider.callbacks(),
                                       c_max_spelling_bytes);
  }

  /**
   * @brief Build options with ample work and scratch ceilings.
   * @par Requirements
   * P038; J5-048.
   */
  number_comparison_options ample_options()
  {
    number_comparison_options options{};
    options.max_work_units = c_ample_work;
    options.max_scratch_bytes = c_ample_scratch;
    return options;
  }

  /**
   * @brief Mirror an expected ordering for swapped operands.
   * @par Requirements
   * P038; J5-036.
   */
  number_order mirrored(number_order order)
  {
    if (order == number_order::less)
    {
      return number_order::greater;
    }
    if (order == number_order::greater)
    {
      return number_order::less;
    }
    return order;
  }

  /**
   * @brief Print an ordering by name in failure messages.
   * @par Requirements
   * P038; J5-036.
   */
  std::ostream& operator<<(std::ostream& stream, number_order value)
  {
    switch (value)
    {
    case number_order::less:
      return stream << "less";
    case number_order::equal:
      return stream << "equal";
    case number_order::greater:
      return stream << "greater";
    case number_order::unordered:
      return stream << "unordered";
    }
    return stream << "unknown";
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_036 =================================== */
  static_assert(noexcept(compare_numbers(std::declval<const owned_number&>(), std::declval<const owned_number&>(),
                                         std::declval<const storage_callbacks&>())));

  /**
   * @brief Describe one exact comparison and its expected ordering.
   * @par Requirements
   * P038; J5-036; J5-038.
   */
  struct comparison_case
  {
      const char* name;
      const char* left;      // Input left JSON5 numeric spelling.
      const char* right;     // Input right JSON5 numeric spelling.
      number_order expected; // Expected ordering of left relative to right.
  };

  std::ostream& operator<<(std::ostream& stream, const comparison_case& value)
  {
    return stream << value.name;
  }

  class NumberComparisonUnits : public ::testing::TestWithParam<comparison_case>
  {
  };

  /* Verify comparison orders exact mathematical values independently of spelling, in both directions.
   * Arrange: Own the case's left and right spellings.
   * Act: Compare left with right and right with left.
   * Expect: The case ordering, its mirror for swapped operands and no outstanding scratch.
   */
  TEST_P(NumberComparisonUnits, OrdersExactValuesSymmetrically)
  {
    /* Arrange */
    const comparison_case& param = GetParam();
    guard_provider number_storage;
    guard_provider scratch;
    const auto left = make_number(number_storage, param.left);
    const auto right = make_number(number_storage, param.right);
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    /* Act */
    const auto forward = compare_numbers(*left.value_if(), *right.value_if(), scratch.callbacks(), ample_options());
    const auto backward = compare_numbers(*right.value_if(), *left.value_if(), scratch.callbacks(), ample_options());
    /* Expect */
    ASSERT_TRUE(forward.has_value());
    ASSERT_TRUE(backward.has_value());
    EXPECT_EQ(*forward.value_if(), param.expected);
    EXPECT_EQ(*backward.value_if(), mirrored(param.expected));
    EXPECT_EQ(scratch.m_live_bytes, 0U);
  }

  constexpr auto c_less = number_order::less;
  constexpr auto c_equal = number_order::equal;
  constexpr auto c_greater = number_order::greater;
  constexpr auto c_unordered = number_order::unordered;

  INSTANTIATE_TEST_SUITE_P(
    J5_036_NumberComparison, NumberComparisonUnits,
    ::testing::Values(
      comparison_case{"PointZero", "1", "1.0", c_equal}, comparison_case{"ExponentZero", "1", "1e0", c_equal},
      comparison_case{"PlusSign", "+1", "1", c_equal}, comparison_case{"Hexadecimal", "1", "0x1", c_equal},
      comparison_case{"ScaledHex", "1.0e2", "0x64", c_equal},
      comparison_case{"HexAgainstExponent", "0xff", "2.55e2", c_equal},
      comparison_case{"WideHexAgainstDecimal", "0x10000000000000000", "18446744073709551616", c_equal},
      comparison_case{"TrailingZerosCancelExponent", "100e-2", "1", c_equal},
      comparison_case{"FractionAgainstExponent", "123.4", "1234e-1", c_equal},
      comparison_case{"LeadingZerosInFraction", "0.0001", "1e-4", c_equal},
      comparison_case{"NegativeHexAgainstDecimal", "-0x10", "-16", c_equal},
      comparison_case{"NegativeZero", "-0", "0", c_equal}, comparison_case{"HexZeroes", "-0x0", "0.0", c_equal},
      comparison_case{"ZeroWithHugeExponent", "0e999999999999999999999", "-0.0", c_equal},
      comparison_case{"CloseFraction", "0.1", "0.10000000000000000001", c_less},
      comparison_case{"LongIntegers", "123456789012345678901234567890", "123456789012345678901234567891", c_less},
      comparison_case{"DigitCountBoundary", "9", "10", c_less},
      comparison_case{"DecimalBelowHex", "99", "0x64", c_less},
      comparison_case{"HexBelowDecimal", "0xFF", "256", c_less},
      comparison_case{"LargeExponentGap", "9e999999", "1e1000000", c_less},
      comparison_case{"TinyExponentGap", "1e-1000", "1e-999", c_less},
      comparison_case{"NegativeMagnitudesReverse", "-2.5", "-2.4", c_less},
      comparison_case{"NegativeLargeExponents", "-1e1000000", "-9e999999", c_less},
      comparison_case{"NegativeBelowZero", "-1", "0", c_less},
      comparison_case{"NegativeZeroBelowPositive", "-0", "1e-999999", c_less},
      comparison_case{"InfinityAboveFinite", "1e999999", "Infinity", c_less},
      comparison_case{"NegativeInfinityBelowFinite", "-Infinity", "-1e999999", c_less},
      comparison_case{"InfinitiesOfOppositeSign", "-Infinity", "Infinity", c_less},
      comparison_case{"EqualInfinities", "Infinity", "+Infinity", c_equal},
      comparison_case{"EqualNegativeInfinities", "-Infinity", "-Infinity", c_equal},
      comparison_case{"NaNWithNaN", "NaN", "NaN", c_unordered},
      comparison_case{"NaNWithFinite", "1", "NaN", c_unordered},
      comparison_case{"NaNWithInfinity", "-NaN", "Infinity", c_unordered}),
    [](const ::testing::TestParamInfo<comparison_case>& info)
    {
      return info.param.name;
    });

  // Chunk boundaries of the 10^4 steps (ADR 0003, decisions 1b and 10): digit runs of 3, 4, 5, 8 and 9
  // digits, scale differences of 3, 4, 5 and 8, and hexadecimal values whose decimal digit count is
  // found in whole chunks plus single steps.
  INSTANTIATE_TEST_SUITE_P(J5_037_ComparisonChunks, NumberComparisonUnits,
                           ::testing::Values(comparison_case{"ThreeDigitRun", "123", "1.23e2", c_equal},
                                             comparison_case{"FourDigitRun", "1234", "12340e-1", c_equal},
                                             comparison_case{"FiveDigitRun", "12345", "1.2345e4", c_equal},
                                             comparison_case{"EightDigitRun", "12345678", "123456780e-1", c_equal},
                                             comparison_case{"NineDigitRun", "123456789", "123456789.1", c_less},
                                             comparison_case{"ScaleDifferenceThree", "1000", "1e3", c_equal},
                                             comparison_case{"ScaleDifferenceFour", "10000", "1e4", c_equal},
                                             comparison_case{"ScaleDifferenceFive", "100000", "1e5", c_equal},
                                             comparison_case{"ScaleDifferenceEight", "100000001", "1e8", c_greater},
                                             comparison_case{"HexFourDigits", "0x270F", "9.999e3", c_equal},
                                             comparison_case{"HexFiveDigits", "0x2710", "1e4", c_equal},
                                             comparison_case{"HexEightDigits", "0x5F5E0FF", "99999999", c_equal},
                                             comparison_case{"HexNineDigits", "0x5F5E100", "1e8", c_equal},
                                             comparison_case{"HexNineteenDigits", "0xDE0B6B3A7640000", "1e18", c_equal},
                                             comparison_case{"HexJustBelowPower", "0xDE0B6B3A763FFFF", "1e18", c_less}),
                           [](const ::testing::TestParamInfo<comparison_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify zeros, infinities and NaN compare without scratch allocation.
   * Arrange: Own zero, negative zero, infinity, NaN and one; use a scratch limit of zero bytes.
   * Act: Compare pairs decided before any magnitude arithmetic.
   * Expect: Correct orderings and no scratch provider request.
   */
  TEST(J5_036_NumberComparison, SpecialAndSignDecidedValuesDoNotAllocate)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    const auto zero = make_number(number_storage, "0");
    const auto negative_zero = make_number(number_storage, "-0.0");
    const auto infinity = make_number(number_storage, "Infinity");
    const auto nan = make_number(number_storage, "NaN");
    const auto one = make_number(number_storage, "1");
    const auto minus_one = make_number(number_storage, "-1");
    for (const auto* number : {&zero, &negative_zero, &infinity, &nan, &one, &minus_one})
    {
      ASSERT_TRUE(number->has_value());
    }
    number_comparison_options options{};
    options.max_scratch_bytes = 0U;
    /* Act */
    const auto zeros = compare_numbers(*zero.value_if(), *negative_zero.value_if(), scratch.callbacks(), options);
    const auto infinite = compare_numbers(*infinity.value_if(), *one.value_if(), scratch.callbacks(), options);
    const auto unordered = compare_numbers(*nan.value_if(), *one.value_if(), scratch.callbacks(), options);
    const auto signs = compare_numbers(*minus_one.value_if(), *one.value_if(), scratch.callbacks(), options);
    /* Expect */
    ASSERT_TRUE(zeros.has_value());
    EXPECT_EQ(*zeros.value_if(), c_equal);
    ASSERT_TRUE(infinite.has_value());
    EXPECT_EQ(*infinite.value_if(), c_greater);
    ASSERT_TRUE(unordered.has_value());
    EXPECT_EQ(*unordered.value_if(), c_unordered);
    ASSERT_TRUE(signs.has_value());
    EXPECT_EQ(*signs.value_if(), c_less);
    EXPECT_EQ(scratch.m_attempts, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify comparison leaves both spellings unchanged and repeats deterministically.
   * Arrange: Own "1.50e1" and "0xF" with default options.
   * Act: Compare them twice.
   * Expect: equal both times and both spellings unchanged.
   */
  TEST(J5_036_NumberComparison, PreservesSpellingsAndRepeats)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    const auto left = make_number(number_storage, "1.50e1");
    const auto right = make_number(number_storage, "0xF");
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    /* Act */
    const auto first = compare_numbers(*left.value_if(), *right.value_if(), scratch.callbacks());
    const auto second = compare_numbers(*left.value_if(), *right.value_if(), scratch.callbacks());
    /* Expect */
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first.value_if(), c_equal);
    EXPECT_EQ(*second.value_if(), c_equal);
    const auto left_spelling = left.value_if()->spelling();
    const auto right_spelling = right.value_if()->spelling();
    EXPECT_EQ(std::string_view(left_spelling.data, left_spelling.size), "1.50e1");
    EXPECT_EQ(std::string_view(right_spelling.data, right_spelling.size), "0xF");
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a moved-from operand on either side is rejected.
   * Arrange: Own "1" twice and move one of them away.
   * Act: Compare the empty number on the left and on the right.
   * Expect: wrong_type_or_access for both orders.
   */
  TEST(J5_036_NumberComparison, RejectsEmptySpelling)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    auto moved_from = make_number(number_storage, "1");
    const auto other = make_number(number_storage, "1");
    ASSERT_TRUE(moved_from.has_value());
    ASSERT_TRUE(other.has_value());
    const owned_number moved{std::move(*moved_from.value_if())};
    /* Act */
    const auto empty_left = compare_numbers(*moved_from.value_if(), *other.value_if(), scratch.callbacks());
    const auto empty_right = compare_numbers(*other.value_if(), *moved_from.value_if(), scratch.callbacks());
    /* Expect */
    ASSERT_FALSE(empty_left.has_value());
    EXPECT_EQ(empty_left.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(empty_right.has_value());
    EXPECT_EQ(empty_right.error_if()->code, error_code::wrong_type_or_access);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the documented exponent limit is inclusive and exact.
   * Arrange: Build exponents equal to PTRDIFF_MAX / 4, one below it and one above it.
   * Act: Compare the limit against one below, and the over-limit value against one.
   * Expect: greater at the limit, resource_limit above it.
   */
  TEST(J5_036_NumberComparison, EnforcesExponentLimit)
  {
    /* Arrange */
    constexpr auto c_limit = static_cast<unsigned long long>(std::numeric_limits<std::ptrdiff_t>::max() / 4);
    const std::string at_limit = "1e" + std::to_string(c_limit);
    const std::string below_limit = "1e" + std::to_string(c_limit - 1U);
    const std::string above_limit = "1e" + std::to_string(c_limit + 1U);
    guard_provider number_storage;
    guard_provider scratch;
    const auto at = make_number(number_storage, at_limit);
    const auto below = make_number(number_storage, below_limit);
    const auto above = make_number(number_storage, above_limit);
    const auto one = make_number(number_storage, "1");
    ASSERT_TRUE(at.has_value());
    ASSERT_TRUE(below.has_value());
    ASSERT_TRUE(above.has_value());
    ASSERT_TRUE(one.has_value());
    /* Act */
    const auto within = compare_numbers(*at.value_if(), *below.value_if(), scratch.callbacks(), ample_options());
    const auto beyond = compare_numbers(*above.value_if(), *one.value_if(), scratch.callbacks(), ample_options());
    /* Expect */
    ASSERT_TRUE(within.has_value());
    EXPECT_EQ(*within.value_if(), c_greater);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(scratch.m_live_bytes, 0U);
  }
  /* ================================= End Test_J5_036 ================================= */
  /* =================================== Test_J5_048 =================================== */
  /* Verify work and scratch ceilings return resource_limit and release scratch.
   * Arrange: Own "1.5" and "2"; build options with zero work and with zero scratch bytes.
   * Act: Compare under each option set.
   * Expect: resource_limit for both, with no outstanding scratch.
   */
  TEST(J5_048_NumberComparison, EnforcesWorkAndScratchCeilings)
  {
    /* Arrange */
    guard_provider number_storage;
    guard_provider scratch;
    const auto left = make_number(number_storage, "1.5");
    const auto right = make_number(number_storage, "2");
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    number_comparison_options no_work{};
    no_work.max_work_units = 0U;
    number_comparison_options no_scratch{};
    no_scratch.max_scratch_bytes = 0U;
    /* Act */
    const auto work_limited = compare_numbers(*left.value_if(), *right.value_if(), scratch.callbacks(), no_work);
    const auto scratch_limited = compare_numbers(*left.value_if(), *right.value_if(), scratch.callbacks(), no_scratch);
    /* Expect */
    ASSERT_FALSE(work_limited.has_value());
    EXPECT_EQ(work_limited.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(scratch_limited.has_value());
    EXPECT_EQ(scratch_limited.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(scratch.m_live_bytes, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify every scratch allocation failure is reported and fully released.
   * Arrange: Own an 80-bit hexadecimal value and its decimal spelling, which need digit counting.
   * Act: Fail the first, second, ... scratch request in fresh providers until comparison succeeds.
   * Expect: allocation_failure with no outstanding scratch for each failure, then equal.
   */
  TEST(J5_048_NumberComparison, FailsEveryScratchAllocationAndAllowsRetry)
  {
    /* Arrange */
    guard_provider number_storage;
    const auto hexadecimal = make_number(number_storage, "0xFFFFFFFFFFFFFFFFFFFF");
    const auto decimal = make_number(number_storage, "1208925819614629174706175");
    ASSERT_TRUE(hexadecimal.has_value());
    ASSERT_TRUE(decimal.has_value());
    std::size_t failures{};
    bool succeeded{};
    /* Act */
    for (std::size_t fail_on = 1U; (fail_on <= c_max_injected_failures) && !succeeded; ++fail_on)
    {
      guard_provider scratch;
      scratch.m_fail_on = fail_on;
      const auto compared =
        compare_numbers(*hexadecimal.value_if(), *decimal.value_if(), scratch.callbacks(), ample_options());
      EXPECT_EQ(scratch.m_live_bytes, 0U) << "fail_on " << fail_on;
      if (compared.has_value())
      {
        succeeded = true;
        EXPECT_EQ(*compared.value_if(), c_equal);
        continue;
      }
      EXPECT_EQ(compared.error_if()->code, error_code::allocation_failure) << "fail_on " << fail_on;
      ++failures;
    }
    /* Expect */
    EXPECT_TRUE(succeeded);
    EXPECT_GT(failures, 1U);
  }
  /* ================================= End Test_J5_048 ================================= */
} // namespace
