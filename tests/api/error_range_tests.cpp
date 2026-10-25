#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"

#include <gtest/gtest.h>

namespace
{
  using namespace adaptive_json5;
  /* =================================== Test_J5_043 =================================== */
  // The range factory stays usable in constant expressions, like the other factories.
  constexpr error c_ranged = error::at_range(error_code::syntax, 6U, source_span{2U, 4U});
  static_assert(c_ranged.has_byte_offset && (c_ranged.byte_offset == 6U));
  static_assert(c_ranged.source_range.has_value() && (c_ranged.source_range->byte_offset == 2U) &&
                (c_ranged.source_range->byte_length == 4U));
  static_assert(!error::at_offset(error_code::syntax, 1U).source_range.has_value());
  static_assert(!error::without_location(error_code::allocation_failure).source_range.has_value());
  static_assert(!error::numeric_failure(numeric_conversion_reason::out_of_range).source_range.has_value());

  /* Verify a ranged error keeps its primary location separate from the token range and carries no other metadata.
   * Arrange: A range covering bytes 2-5 and a primary offset at byte 6.
   * Act: Build the error and a duplicate-member error for comparison.
   * Expect: Exact offset and range; no duplicate or numeric metadata; duplicates carry no range.
   */
  TEST(J5_043_ErrorRange, SeparatesPrimaryOffsetFromTokenRange)
  {
    /* Arrange */
    constexpr source_span c_range{2U, 4U};
    /* Act */
    const error ranged = error::at_range(error_code::syntax, 6U, c_range);
    const error duplicate = error::duplicate_member(source_span{1U, 1U}, source_span{7U, 1U});
    /* Expect */
    EXPECT_EQ(ranged.code, error_code::syntax);
    EXPECT_TRUE(ranged.has_byte_offset);
    EXPECT_EQ(ranged.byte_offset, 6U);
    ASSERT_TRUE(ranged.source_range.has_value());
    EXPECT_EQ(ranged.source_range->byte_offset, 2U);
    EXPECT_EQ(ranged.source_range->byte_length, 4U);
    EXPECT_FALSE(ranged.duplicate_names.has_value());
    EXPECT_FALSE(ranged.numeric_reason.has_value());
    EXPECT_FALSE(duplicate.source_range.has_value());
  }
  /* ================================= End Test_J5_043 ================================= */
} // namespace
