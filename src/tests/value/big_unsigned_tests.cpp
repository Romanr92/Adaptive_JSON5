#include "../../../tests/support/guard_provider.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/value/big_unsigned.hpp"
#include "adaptive_json5/detail/value/floating_format.hpp"
#include "adaptive_json5/detail/value/floating_special.hpp"
#include "adaptive_json5/detail/value/number_analysis.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/floating_conversion_policy.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <ostream>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5::detail::big_unsigned;
  using adaptive_json5::detail::magnitude_order;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  constexpr std::size_t c_ample_work = 1000000;

  /**
   * @brief Append every digit of a test literal to an exact integer.
   * @par Requirements
   * P038; J5-037.
   * @param target Integer receiving the digits.
   * @param digits Decimal or uppercase hexadecimal digits without prefix or sign.
   * @param radix Spelling base passed unchanged to append_digit.
   * @param work Shared budget charged by each append.
   * @return True when every append succeeded.
   * @note Only maps characters to digit values; arithmetic remains in the unit under test.
   */
  bool load_digits(big_unsigned& target, const char* digits, unsigned int radix, work_budget& work)
  {
    for (const char* current = digits; *current != '\0'; ++current)
    {
      const char character = *current;
      const unsigned int value = ((character >= '0') && (character <= '9'))
                                   ? static_cast<unsigned int>(character - '0')
                                   : static_cast<unsigned int>(character - 'A') + 10U;

      if (!target.append_digit(value, radix, work).has_value())
      {
        return false;
      }
    }

    return true;
  }

  /**
   * @brief Compare two exact integers with an ample, isolated work budget.
   * @par Requirements
   * P038; J5-037.
   * @param left Left magnitude.
   * @param right Right magnitude.
   * @return Exact ordering; a failed comparison is reported as a test failure.
   */
  magnitude_order order_of(const big_unsigned& left, const big_unsigned& right)
  {
    work_budget work{c_ample_work};
    const auto order = left.compare(right, work);

    EXPECT_TRUE(order.has_value());

    return order.has_value() ? *order.value_if() : magnitude_order::less;
  }

  /**
   * @brief Print an ordering by name in parameterized failure messages.
   * @par Requirements
   * P038; J5-037.
   */
  std::ostream& operator<<(std::ostream& stream, magnitude_order value)
  {
    switch (value)
    {
    case magnitude_order::less:
      return stream << "less";
    case magnitude_order::equal:
      return stream << "equal";
    case magnitude_order::greater:
      return stream << "greater";
    }

    return stream << "unknown";
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_037 =================================== */
  static_assert(!std::is_copy_constructible_v<big_unsigned>);
  static_assert(!std::is_move_constructible_v<big_unsigned>);
  static_assert(noexcept(std::declval<const big_unsigned&>().is_zero()));
  static_assert(noexcept(std::declval<const big_unsigned&>().bit_length()));

  /**
   * @brief Describe one exact bit-length expectation.
   * @par Requirements
   * P038; J5-037.
   */
  struct bit_length_case
  {
      const char* name;
      const char* digits; // Input digits without prefix.
      unsigned int radix; // Input spelling base: 10 or 16.
      std::size_t bits;   // Expected significant bit count.
  };

  std::ostream& operator<<(std::ostream& stream, const bit_length_case& value)
  {
    return stream << value.name;
  }

  class BigUnsignedBitLengthUnits : public ::testing::TestWithParam<bit_length_case>
  {
  };

  /**
   * @brief Describe one rejected append request.
   * @par Requirements
   * P038; J5-003; J5-037.
   */
  struct invalid_digit_case
  {
      const char* name;
      unsigned int digit; // Requested digit value.
      unsigned int radix; // Requested spelling base.
  };

  std::ostream& operator<<(std::ostream& stream, const invalid_digit_case& value)
  {
    return stream << value.name;
  }

  class BigUnsignedInvalidDigitUnits : public ::testing::TestWithParam<invalid_digit_case>
  {
  };

  /**
   * @brief Describe one ordering expectation between two decimal magnitudes.
   * @par Requirements
   * P038; J5-037.
   */
  struct compare_case
  {
      const char* name;
      const char* left;         // Left decimal digits.
      const char* right;        // Right decimal digits.
      magnitude_order expected; // Expected ordering of left relative to right.
  };

  std::ostream& operator<<(std::ostream& stream, const compare_case& value)
  {
    return stream << value.name;
  }

  class BigUnsignedCompareUnits : public ::testing::TestWithParam<compare_case>
  {
  };

  /**
   * @brief Describe one exact power-of-two scaling.
   * @par Requirements
   * P038; J5-037.
   */
  struct shift_case
  {
      const char* name;
      const char* value;    // Input decimal digits.
      std::size_t bits;     // Shift count in bits.
      const char* expected; // Expected decimal digits after shifting.
      std::size_t length;   // Expected significant bit count after shifting.
  };

  std::ostream& operator<<(std::ostream& stream, const shift_case& value)
  {
    return stream << value.name;
  }

  class BigUnsignedShiftUnits : public ::testing::TestWithParam<shift_case>
  {
  };

  /* Verify a new integer is zero without allocating or charging work.
   * Arrange: Construct an integer over a guarded provider.
   * Act: Query zero state and bit length.
   * Expect: Zero, a bit length of zero and no provider request.
   */
  TEST(J5_037_BigUnsigned, StartsAtZeroWithoutAllocation)
  {
    /* Arrange */
    guard_provider provider;
    const big_unsigned value{provider.callbacks()};
    /* Act */
    const bool zero = value.is_zero();
    const auto length = value.bit_length();
    /* Expect */
    EXPECT_TRUE(zero);
    ASSERT_TRUE(length.has_value());
    EXPECT_EQ(*length.value_if(), 0U);
    EXPECT_EQ(provider.m_attempts, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify bit length is exact across limb boundaries for decimal and hexadecimal input.
   * Arrange: Append the case digits in the case radix.
   * Act: Query the bit length.
   * Expect: The exact significant bit count and matching zero state.
   */
  TEST_P(BigUnsignedBitLengthUnits, ReportsExactSignificantBits)
  {
    /* Arrange */
    const bit_length_case& param = GetParam();
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned value{provider.callbacks()};
    ASSERT_TRUE(load_digits(value, param.digits, param.radix, work));
    /* Act */
    const auto length = value.bit_length();
    /* Expect */
    ASSERT_TRUE(length.has_value());
    EXPECT_EQ(*length.value_if(), param.bits);
    EXPECT_EQ(value.is_zero(), param.bits == 0U);
  }

  INSTANTIATE_TEST_SUITE_P(J5_037_BigUnsigned, BigUnsignedBitLengthUnits,
                           ::testing::Values(bit_length_case{"LeadingZerosStayZero", "000", 10U, 0U},
                                             bit_length_case{"One", "1", 10U, 1U},
                                             bit_length_case{"FullFirstLimb", "65535", 10U, 16U},
                                             bit_length_case{"SecondLimb", "65536", 10U, 17U},
                                             bit_length_case{"TwoToSixtyFour", "18446744073709551616", 10U, 65U},
                                             bit_length_case{"HexTwoToSixtyFour", "10000000000000000", 16U, 65U},
                                             bit_length_case{"HexFiveFullLimbs", "FFFFFFFFFFFFFFFFFFFF", 16U, 80U},
                                             bit_length_case{"HexLeadingZeros", "0001", 16U, 1U}),
                           [](const ::testing::TestParamInfo<bit_length_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify invalid digits and radices are rejected without changing the value.
   * Arrange: Load 12 in decimal and an equal reference.
   * Act: Append the case digit in the case radix.
   * Expect: wrong_type_or_access and an unchanged value.
   */
  TEST_P(BigUnsignedInvalidDigitUnits, RejectsWithoutMutation)
  {
    /* Arrange */
    const invalid_digit_case& param = GetParam();
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned value{provider.callbacks()};
    big_unsigned reference{provider.callbacks()};
    ASSERT_TRUE(load_digits(value, "12", 10U, work));
    ASSERT_TRUE(load_digits(reference, "12", 10U, work));
    /* Act */
    const auto appended = value.append_digit(param.digit, param.radix, work);
    /* Expect */
    ASSERT_FALSE(appended.has_value());
    EXPECT_EQ(appended.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(order_of(value, reference), magnitude_order::equal);
  }

  INSTANTIATE_TEST_SUITE_P(J5_037_BigUnsigned, BigUnsignedInvalidDigitUnits,
                           ::testing::Values(invalid_digit_case{"DecimalDigitEqualsRadix", 10U, 10U},
                                             invalid_digit_case{"HexDigitInDecimal", 15U, 10U},
                                             invalid_digit_case{"HexDigitEqualsRadix", 16U, 16U},
                                             invalid_digit_case{"OctalRadix", 1U, 8U},
                                             invalid_digit_case{"ZeroRadix", 0U, 0U}),
                           [](const ::testing::TestParamInfo<invalid_digit_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify comparison orders values by limb count and by limb contents.
   * Arrange: Load the case's left and right decimal digits.
   * Act: Compare left with right.
   * Expect: The case ordering.
   */
  TEST_P(BigUnsignedCompareUnits, ReportsExactOrdering)
  {
    /* Arrange */
    const compare_case& param = GetParam();
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned left{provider.callbacks()};
    big_unsigned right{provider.callbacks()};
    ASSERT_TRUE(load_digits(left, param.left, 10U, work));
    ASSERT_TRUE(load_digits(right, param.right, 10U, work));
    /* Act */
    const magnitude_order order = order_of(left, right);
    /* Expect */
    EXPECT_EQ(order, param.expected);
  }

  INSTANTIATE_TEST_SUITE_P(J5_037_BigUnsigned, BigUnsignedCompareUnits,
                           ::testing::Values(compare_case{"ZeroEqualsZero", "0", "0", magnitude_order::equal},
                                             compare_case{"ZeroBelowOne", "0", "1", magnitude_order::less},
                                             compare_case{"FewerLimbsBelow", "65535", "65536", magnitude_order::less},
                                             compare_case{"MoreLimbsAbove", "65536", "65535", magnitude_order::greater},
                                             compare_case{"SameLimbsLowerTop", "65536", "65537", magnitude_order::less},
                                             compare_case{"SameLimbsHigherTop", "131072", "65537",
                                                          magnitude_order::greater},
                                             compare_case{"LargeEqual", "18446744073709551616", "18446744073709551616",
                                                          magnitude_order::equal}),
                           [](const ::testing::TestParamInfo<compare_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify left shifts multiply by exact powers of two across whole and partial limbs.
   * Arrange: Load the case value and the expected shifted value.
   * Act: Shift the value by the case bit count.
   * Expect: The expected value and bit length.
   */
  TEST_P(BigUnsignedShiftUnits, MultipliesByPowerOfTwo)
  {
    /* Arrange */
    const shift_case& param = GetParam();
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned value{provider.callbacks()};
    big_unsigned expected{provider.callbacks()};
    ASSERT_TRUE(load_digits(value, param.value, 10U, work));
    ASSERT_TRUE(load_digits(expected, param.expected, 10U, work));
    /* Act */
    const auto shifted = value.shift_left(param.bits, work);
    /* Expect */
    ASSERT_TRUE(shifted.has_value());
    EXPECT_EQ(order_of(value, expected), magnitude_order::equal);
    const auto length = value.bit_length();
    ASSERT_TRUE(length.has_value());
    EXPECT_EQ(*length.value_if(), param.length);
  }

  INSTANTIATE_TEST_SUITE_P(J5_037_BigUnsigned, BigUnsignedShiftUnits,
                           ::testing::Values(shift_case{"ZeroBits", "5", 0U, "5", 3U},
                                             shift_case{"ZeroValue", "0", 100U, "0", 0U},
                                             shift_case{"PartialWithinLimb", "3", 3U, "24", 5U},
                                             shift_case{"PartialAcrossLimb", "3", 17U, "393216", 19U},
                                             shift_case{"PartialWithoutCarry", "1", 15U, "32768", 16U},
                                             shift_case{"WholeLimbs", "1", 64U, "18446744073709551616", 65U},
                                             shift_case{"MultiLimbPartial", "65537", 20U, "68720525312", 37U}),
                           [](const ::testing::TestParamInfo<shift_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify trivial shifts neither allocate nor charge work.
   * Arrange: Load 5 and keep a zero integer; record provider and work usage.
   * Act: Shift 5 by zero bits and zero by 100 bits.
   * Expect: Success with unchanged provider requests and consumed work.
   */
  TEST(J5_037_BigUnsigned, TrivialShiftsAreFree)
  {
    /* Arrange */
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned five{provider.callbacks()};
    big_unsigned zero{provider.callbacks()};
    ASSERT_TRUE(load_digits(five, "5", 10U, work));
    const std::size_t attempts = provider.m_attempts;
    const std::size_t consumed = work.consumed();
    /* Act */
    const auto no_bits = five.shift_left(0U, work);
    const auto zero_value = zero.shift_left(100U, work);
    /* Expect */
    EXPECT_TRUE(no_bits.has_value());
    EXPECT_TRUE(zero_value.has_value());
    EXPECT_EQ(provider.m_attempts, attempts);
    EXPECT_EQ(work.consumed(), consumed);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify subtraction borrows across limbs and removes leading zero limbs.
   * Arrange: Load 2^64, one and the expected 2^64 - 1.
   * Act: Subtract one from 2^64.
   * Expect: 2^64 - 1 with 64 significant bits.
   */
  TEST(J5_037_BigUnsigned, SubtractsWithBorrowAndCanonicalLength)
  {
    /* Arrange */
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned value{provider.callbacks()};
    big_unsigned one{provider.callbacks()};
    big_unsigned expected{provider.callbacks()};
    ASSERT_TRUE(load_digits(value, "18446744073709551616", 10U, work));
    ASSERT_TRUE(load_digits(one, "1", 10U, work));
    ASSERT_TRUE(load_digits(expected, "18446744073709551615", 10U, work));
    /* Act */
    const auto difference = value.subtract(one, work);
    /* Expect */
    ASSERT_TRUE(difference.has_value());
    EXPECT_EQ(order_of(value, expected), magnitude_order::equal);
    const auto length = value.bit_length();
    ASSERT_TRUE(length.has_value());
    EXPECT_EQ(*length.value_if(), 64U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify self-subtraction yields canonical zero and negative results are rejected.
   * Arrange: Load 7, 8 and a reference 7.
   * Act: Subtract 8 from 7, then subtract 7 from itself.
   * Expect: wrong_type_or_access leaving 7 unchanged, then zero with no significant bits.
   */
  TEST(J5_037_BigUnsigned, RejectsNegativeDifferenceAndSelfSubtractsToZero)
  {
    /* Arrange */
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned seven{provider.callbacks()};
    big_unsigned eight{provider.callbacks()};
    big_unsigned reference{provider.callbacks()};
    ASSERT_TRUE(load_digits(seven, "7", 10U, work));
    ASSERT_TRUE(load_digits(eight, "8", 10U, work));
    ASSERT_TRUE(load_digits(reference, "7", 10U, work));
    /* Act */
    const auto negative = seven.subtract(eight, work);
    const magnitude_order after_rejection = order_of(seven, reference);
    const auto self = seven.subtract(seven, work);
    /* Expect */
    ASSERT_FALSE(negative.has_value());
    EXPECT_EQ(negative.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(after_rejection, magnitude_order::equal);
    ASSERT_TRUE(self.has_value());
    EXPECT_TRUE(seven.is_zero());
    const auto length = seven.bit_length();
    ASSERT_TRUE(length.has_value());
    EXPECT_EQ(*length.value_if(), 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify assignment copies independently and self-assignment is free.
   * Arrange: Load a two-limb source, an empty destination and a reference value.
   * Act: Assign, modify the source, then self-assign the destination.
   * Expect: Destination keeps the copied value; self-assignment charges no work.
   */
  TEST(J5_037_BigUnsigned, AssignsIndependentCopy)
  {
    /* Arrange */
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned source{provider.callbacks()};
    big_unsigned destination{provider.callbacks()};
    big_unsigned reference{provider.callbacks()};
    ASSERT_TRUE(load_digits(source, "65537", 10U, work));
    ASSERT_TRUE(load_digits(reference, "65537", 10U, work));
    /* Act */
    const auto copied = destination.assign(source, work);
    const auto modified = source.append_digit(9U, 10U, work);
    const std::size_t consumed = work.consumed();
    const auto self = destination.assign(destination, work);
    /* Expect */
    ASSERT_TRUE(copied.has_value());
    ASSERT_TRUE(modified.has_value());
    ASSERT_TRUE(self.has_value());
    EXPECT_EQ(work.consumed(), consumed);
    EXPECT_EQ(order_of(destination, reference), magnitude_order::equal);
    EXPECT_EQ(order_of(source, reference), magnitude_order::greater);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify documented work charges for append, compare and shift.
   * Arrange: Load two equal two-limb integers (65536) and fresh budgets.
   * Act: Append a digit, compare equal-length values and shift by one bit.
   * Expect: 2n+1, 1+n and 1+n+(n+1) units, where n = 2 limbs before each call.
   */
  TEST(J5_037_BigUnsigned, ChargesDocumentedWorkUnits)
  {
    /* Arrange */
    guard_provider provider;
    work_budget setup{c_ample_work};
    big_unsigned value{provider.callbacks()};
    big_unsigned other{provider.callbacks()};
    ASSERT_TRUE(load_digits(value, "65536", 10U, setup));
    ASSERT_TRUE(load_digits(other, "65536", 10U, setup));
    work_budget append_work{c_ample_work};
    work_budget compare_work{c_ample_work};
    work_budget shift_work{c_ample_work};
    /* Act */
    const auto appended = value.append_digit(0U, 10U, append_work);
    const auto compared = other.compare(other, compare_work);
    const auto shifted = other.shift_left(1U, shift_work);
    /* Expect */
    ASSERT_TRUE(appended.has_value());
    ASSERT_TRUE(compared.has_value());
    ASSERT_TRUE(shifted.has_value());
    EXPECT_EQ(append_work.consumed(), 5U);
    EXPECT_EQ(compare_work.consumed(), 3U);
    EXPECT_EQ(shift_work.consumed(), 6U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify work exhaustion is reported before mutation and permits retry.
   * Arrange: Load 65536 (two limbs) and a 4-unit budget, below every needed charge.
   * Act: Append and shift with the small budget, then append with an ample one.
   * Expect: resource_limit preserving the value, then a successful append.
   */
  TEST(J5_037_BigUnsigned, WorkExhaustionPreservesValueAndAllowsRetry)
  {
    /* Arrange */
    guard_provider provider;
    work_budget setup{c_ample_work};
    big_unsigned value{provider.callbacks()};
    big_unsigned reference{provider.callbacks()};
    ASSERT_TRUE(load_digits(value, "65536", 10U, setup));
    ASSERT_TRUE(load_digits(reference, "65536", 10U, setup));
    work_budget small{4U};
    /* Act */
    const auto appended = value.append_digit(0U, 10U, small);
    const auto shifted = value.shift_left(1U, small);
    const magnitude_order after_failures = order_of(value, reference);
    const auto retried = value.append_digit(0U, 10U, setup);
    /* Expect */
    ASSERT_FALSE(appended.has_value());
    EXPECT_EQ(appended.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(shifted.has_value());
    EXPECT_EQ(shifted.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(small.consumed(), 0U);
    EXPECT_EQ(after_failures, magnitude_order::equal);
    EXPECT_TRUE(retried.has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify growth, shift and copy allocation failures preserve values and release storage.
   * Arrange: Load 65535 (one full limb) and an empty destination.
   * Act: Inject failure on the next request before a growing append, a shift and an assignment; retry.
   * Expect: allocation_failure without mutation; retry succeeds; the guard sees balanced releases.
   */
  TEST(J5_037_BigUnsigned, AllocationFailurePreservesValueAndAllowsRetry)
  {
    /* Arrange */
    guard_provider provider;
    work_budget work{c_ample_work};
    big_unsigned value{provider.callbacks()};
    big_unsigned reference{provider.callbacks()};
    big_unsigned destination{provider.callbacks()};
    ASSERT_TRUE(load_digits(value, "65535", 10U, work));
    ASSERT_TRUE(load_digits(reference, "65535", 10U, work));
    /* Act */
    provider.m_fail_on = provider.m_attempts + 1U;
    const auto grown = value.append_digit(0U, 10U, work);
    provider.m_fail_on = provider.m_attempts + 1U;
    const auto shifted = value.shift_left(3U, work);
    provider.m_fail_on = provider.m_attempts + 1U;
    const auto copied = destination.assign(value, work);
    const magnitude_order after_failures = order_of(value, reference);
    provider.m_fail_on = 0U;
    const auto retried = value.append_digit(0U, 10U, work);
    /* Expect */
    for (const auto* failure : {&grown, &shifted, &copied})
    {
      ASSERT_FALSE(failure->has_value());
      EXPECT_EQ(failure->error_if()->code, error_code::allocation_failure);
    }
    EXPECT_EQ(after_failures, magnitude_order::equal);
    EXPECT_TRUE(destination.is_zero());
    ASSERT_TRUE(retried.has_value());
    EXPECT_EQ(order_of(value, reference), magnitude_order::greater);
  }
  /* ================================= End Test_J5_037 ================================= */
  /* =================================== Test_J5_038 =================================== */
  static_assert(detail::supports_floating_conversion<float>() == std::numeric_limits<float>::is_iec559);
  static_assert(detail::supports_floating_conversion<double>() == std::numeric_limits<double>::is_iec559);
  static_assert(!detail::supports_floating_conversion<int>());

  /**
   * @brief Describe one special-value classification and its expected outcome.
   * @par Requirements
   * P038; J5-037; J5-038.
   */
  struct special_case
  {
      const char* name;
      detail::number_analysis analysis;  // Input analysis of a validated number.
      floating_non_finite_policy policy; // Input non-finite policy.
      bool fails;                        // Whether the helper returns non_finite.
      bool produces_value;               // Whether success holds a value rather than nullopt.
      double expected;                   // Expected value when produced; NaN means any NaN.
  };

  std::ostream& operator<<(std::ostream& stream, const special_case& value)
  {
    return stream << value.name;
  }

  class FloatingSpecialUnits : public ::testing::TestWithParam<special_case>
  {
  };

  constexpr double c_infinity = std::numeric_limits<double>::infinity();
  constexpr double c_nan = std::numeric_limits<double>::quiet_NaN();
  constexpr auto c_reject = floating_non_finite_policy::reject;
  constexpr auto c_allow = floating_non_finite_policy::allow;

  /* Verify exact zero keeps its sign, finite nonzero values defer and non-finite values follow policy.
   * Arrange: Select the case analysis and non-finite policy.
   * Act: Convert through the double and float special-value helpers.
   * Expect: The same outcome for both destinations, including the sign of zero and infinity.
   */
  TEST_P(FloatingSpecialUnits, AppliesSignAndNonFinitePolicy)
  {
    /* Arrange */
    const special_case& param = GetParam();
    floating_conversion_options options{};
    options.non_finite = param.policy;
    /* Act */
    const auto as_double = detail::convert_special_floating<double>(param.analysis, options);
    const auto as_float = detail::convert_special_floating<float>(param.analysis, options);
    /* Expect */
    ASSERT_EQ(as_double.has_value(), !param.fails);
    ASSERT_EQ(as_float.has_value(), !param.fails);
    if (param.fails)
    {
      EXPECT_EQ(as_double.error_if()->code, error_code::numeric_conversion);
      ASSERT_TRUE(as_double.error_if()->numeric_reason.has_value());
      EXPECT_EQ(*as_double.error_if()->numeric_reason, numeric_conversion_reason::non_finite);
      ASSERT_TRUE(as_float.error_if()->numeric_reason.has_value());
      EXPECT_EQ(*as_float.error_if()->numeric_reason, numeric_conversion_reason::non_finite);
      return;
    }
    ASSERT_EQ(as_double.value_if()->has_value(), param.produces_value);
    ASSERT_EQ(as_float.value_if()->has_value(), param.produces_value);
    if (!param.produces_value)
    {
      return;
    }
    const double double_value = **as_double.value_if();
    const double float_value = static_cast<double>(**as_float.value_if());
    if (std::isnan(param.expected))
    {
      EXPECT_TRUE(std::isnan(double_value));
      EXPECT_TRUE(std::isnan(float_value));
      return;
    }
    EXPECT_EQ(double_value, param.expected);
    EXPECT_EQ(float_value, param.expected);
    EXPECT_EQ(std::signbit(double_value), std::signbit(param.expected));
    EXPECT_EQ(std::signbit(float_value), std::signbit(param.expected));
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_038_FloatingSpecial, FloatingSpecialUnits,
    ::testing::Values(
      special_case{"PositiveDecimalZero", {number_kind::decimal, false, true, true}, c_reject, false, true, 0.0},
      special_case{"NegativeDecimalZero", {number_kind::decimal, true, true, true}, c_reject, false, true, -0.0},
      special_case{"NegativeHexZero", {number_kind::hexadecimal, true, true, true}, c_reject, false, true, -0.0},
      special_case{"FiniteNonzeroDefers", {number_kind::decimal, true, false, false}, c_reject, false, false, 0.0},
      special_case{"HexNonzeroDefers", {number_kind::hexadecimal, false, false, true}, c_allow, false, false, 0.0},
      special_case{"RejectInfinity", {number_kind::infinity, false, false, false}, c_reject, true, false, 0.0},
      special_case{"RejectNaN", {number_kind::nan, false, false, false}, c_reject, true, false, 0.0},
      special_case{"AllowInfinity", {number_kind::infinity, false, false, false}, c_allow, false, true, c_infinity},
      special_case{
        "AllowNegativeInfinity", {number_kind::infinity, true, false, false}, c_allow, false, true, -c_infinity},
      special_case{"AllowNaN", {number_kind::nan, true, false, false}, c_allow, false, true, c_nan}),
    [](const ::testing::TestParamInfo<special_case>& info)
    {
      return info.param.name;
    });
  /* ================================= End Test_J5_038 ================================= */
} // namespace
