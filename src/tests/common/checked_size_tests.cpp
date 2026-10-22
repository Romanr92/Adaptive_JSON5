#include "adaptive_json5/detail/common/checked_size.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <ostream>
namespace
{
  using namespace adaptive_json5;
  constexpr std::size_t c_max = std::numeric_limits<std::size_t>::max();
  /* =================================== Test_J5_046 ==================================== */
  static_assert(checked_add(1, 2).has_value());
  static_assert(!checked_multiply(c_max, 2).has_value());
  /**
   * @brief Describe one checked size operation.
   * @par Requirements
   * P003; J5-046; J5-051.
   */
  struct size_case
  {
      const char* name;
      std::size_t left;     // First operand in bytes or elements.
      std::size_t right;    // Second operand in the same unit.
      bool succeeds;        // Whether the exact result is representable.
      std::size_t expected; // Exact result when succeeds; ignored otherwise.
  };

  std::ostream& operator<<(std::ostream& stream, const size_case& value)
  {
    return stream << value.name;
  }

  class CheckedAddUnits : public ::testing::TestWithParam<size_case>
  {
  };

  class CheckedMultiplyUnits : public ::testing::TestWithParam<size_case>
  {
  };

  /* Verify checked addition returns exact sums or an allocation-free resource limit.
   * Arrange: Select operands around zero and the representable maximum.
   * Act: Add the operands.
   * Expect: Representable sums are exact; overflow is resource_limit without a location.
   */
  TEST_P(CheckedAddUnits, ReportsExactSumOrResourceLimit)
  {
    /* Arrange */
    const size_case& param = GetParam();
    /* Act */
    const auto actual = checked_add(param.left, param.right);
    /* Expect */
    ASSERT_EQ(actual.has_value(), param.succeeds);
    if (param.succeeds)
    {
      EXPECT_EQ(actual.error_if(), nullptr);
      EXPECT_EQ(*actual.value_if(), param.expected);
    }
    else
    {
      ASSERT_NE(actual.error_if(), nullptr);
      EXPECT_EQ(actual.value_if(), nullptr);
      EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
      EXPECT_FALSE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 0U);
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_046_CheckedSize, CheckedAddUnits,
    ::testing::Values(size_case{"ZeroPlusZero", 0, 0, true, 0}, size_case{"SmallValues", 3, 4, true, 7},
                      size_case{"MaxPlusZero", c_max, 0, true, c_max}, size_case{"ZeroPlusMax", 0, c_max, true, c_max},
                      size_case{"MaxMinusOnePlusOne", c_max - 1, 1, true, c_max},
                      size_case{"MaxPlusOne", c_max, 1, false, 0}, size_case{"OnePlusMax", 1, c_max, false, 0},
                      size_case{"MaxPlusMax", c_max, c_max, false, 0}),
    [](const ::testing::TestParamInfo<size_case>& info)
    {
      return info.param.name;
    });

  /* Verify checked multiplication returns exact products or an allocation-free resource limit.
   * Arrange: Select operands around zero, one and the representable maximum.
   * Act: Multiply the operands.
   * Expect: Representable products are exact; overflow is resource_limit without a location.
   */
  TEST_P(CheckedMultiplyUnits, ReportsExactProductOrResourceLimit)
  {
    /* Arrange */
    const size_case& param = GetParam();
    /* Act */
    const auto actual = checked_multiply(param.left, param.right);
    /* Expect */
    ASSERT_EQ(actual.has_value(), param.succeeds);
    if (param.succeeds)
    {
      EXPECT_EQ(actual.error_if(), nullptr);
      EXPECT_EQ(*actual.value_if(), param.expected);
    }
    else
    {
      ASSERT_NE(actual.error_if(), nullptr);
      EXPECT_EQ(actual.value_if(), nullptr);
      EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
      EXPECT_FALSE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, 0U);
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_046_CheckedSize, CheckedMultiplyUnits,
    ::testing::Values(size_case{"ZeroTimesZero", 0, 0, true, 0}, size_case{"ZeroTimesMax", 0, c_max, true, 0},
                      size_case{"MaxTimesZero", c_max, 0, true, 0}, size_case{"SmallValues", 6, 7, true, 42},
                      size_case{"MaxTimesOne", c_max, 1, true, c_max}, size_case{"OneTimesMax", 1, c_max, true, c_max},
                      size_case{"MaxTimesTwo", c_max, 2, false, 0}, size_case{"TwoTimesMax", 2, c_max, false, 0},
                      size_case{"HalfPlusOneTimesTwo", c_max / 2 + 1, 2, false, 0},
                      size_case{"MaxTimesMax", c_max, c_max, false, 0}),
    [](const ::testing::TestParamInfo<size_case>& info)
    {
      return info.param.name;
    });

  /* ================================= End Test_J5_046 ================================== */
} // namespace
