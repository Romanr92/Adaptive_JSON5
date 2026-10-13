// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
  using adaptive_json5::error;
  using adaptive_json5::error_code;
  using int_result = adaptive_json5::result<int>;
  using owned_result = adaptive_json5::result<std::unique_ptr<int>>;

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Count all constructed and destroyed test payload objects.
   * @par Requirements
   * P002; J5-003.
   */
  struct lifetime_counts
  {
      int constructed = 0;
      int destroyed = 0;
      int live = 0;
  };

  /**
   * @brief Instrument active-alternative lifetimes without dynamic storage.
   * @par Requirements
   * P002; J5-003.
   * @note The counter must outlive every payload referring to it.
   */
  class counted_value
  {
    public:
      counted_value(lifetime_counts& counts, int value) noexcept : m_counts(&counts), m_value(value)
      {
        ++m_counts->constructed;
        ++m_counts->live;
      }

      counted_value(const counted_value& other) noexcept : counted_value(*other.m_counts, other.m_value) {}

      // Count moved-from objects too: they remain alive until their own destruction.
      counted_value(counted_value&& other) noexcept : counted_value(*other.m_counts, other.m_value) {}

      ~counted_value() noexcept
      {
        ++m_counts->destroyed;
        --m_counts->live;
      }

      int value() const noexcept
      {
        return m_value;
      }

    private:
      lifetime_counts* m_counts;
      int m_value;
  };

  /**
   * @brief Pair a minimal error category with its stable public number and case name.
   * @par Requirements
   * P002; J5-043.
   */
  struct code_case
  {
      error_code code;     // Input error category.
      int expected_number; // Expected stable numeric code.
      const char* name;    // Human-readable parameter name.
  };

  const code_case code_cases[] = {
    {error_code::syntax, 1, "Syntax"},
    {error_code::encoding, 2, "Encoding"},
    {error_code::duplicate_policy, 3, "DuplicatePolicy"},
    {error_code::wrong_type_or_access, 4, "WrongTypeOrAccess"},
    {error_code::numeric_conversion, 5, "NumericConversion"},
    {error_code::resource_limit, 6, "ResourceLimit"},
    {error_code::allocation_failure, 7, "AllocationFailure"},
  };

  /**
   * @brief Supply every error category and byte-offset boundary.
   * @par Requirements
   * P002; J5-043.
   */
  class J5_043_LocatedResult : public testing::TestWithParam<std::tuple<code_case, std::size_t>>
  {
  };

  /**
   * @brief Supply every error category without an input location.
   * @par Requirements
   * P002; J5-043.
   */
  class J5_043_UnlocatedResult : public testing::TestWithParam<code_case>
  {
  };

  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_003 =================================== */

  // A move-only payload must make result copying unavailable, while moving remains safe.
  static_assert(!std::is_copy_constructible_v<owned_result>);
  static_assert(std::is_nothrow_move_constructible_v<owned_result>);

  // Value-bearing assignment is deliberately outside the current P002 API.
  static_assert(!std::is_default_constructible_v<int_result>);
  static_assert(!std::is_default_constructible_v<adaptive_json5::status_result>);
  static_assert(!std::is_copy_assignable_v<int_result>);
  static_assert(!std::is_move_assignable_v<int_result>);

  static_assert(std::is_same_v<decltype(std::declval<int_result&>().value_if()), int*>);
  static_assert(std::is_same_v<decltype(std::declval<const int_result&>().value_if()), const int*>);
  static_assert(std::is_same_v<decltype(std::declval<const int_result&>().error_if()), const error*>);

  static_assert(std::is_nothrow_copy_constructible_v<int_result>);
  static_assert(std::is_nothrow_move_constructible_v<int_result>);
  static_assert(std::is_nothrow_destructible_v<int_result>);
  static_assert(std::is_nothrow_copy_constructible_v<adaptive_json5::status_result>);
  static_assert(std::is_nothrow_move_constructible_v<adaptive_json5::status_result>);
  static_assert(std::is_nothrow_copy_assignable_v<adaptive_json5::status_result>);
  static_assert(std::is_nothrow_move_assignable_v<adaptive_json5::status_result>);
  static_assert(std::is_nothrow_destructible_v<adaptive_json5::status_result>);

  static_assert(noexcept(error::at_offset(error_code::syntax, 0)));
  static_assert(noexcept(error::without_location(error_code::syntax)));
  static_assert(noexcept(int_result::success(42)));
  static_assert(noexcept(int_result::failure(error::without_location(error_code::syntax))));
  static_assert(noexcept(std::declval<const int_result&>().has_value()));
  static_assert(noexcept(std::declval<int_result&>().value_if()));
  static_assert(noexcept(std::declval<const int_result&>().value_if()));
  static_assert(noexcept(std::declval<const int_result&>().error_if()));
  static_assert(noexcept(adaptive_json5::status_result::success()));
  static_assert(noexcept(adaptive_json5::status_result::failure(error::without_location(error_code::syntax))));
  static_assert(noexcept(std::declval<const adaptive_json5::status_result&>().has_value()));
  static_assert(noexcept(std::declval<const adaptive_json5::status_result&>().error_if()));

  /* Verify the status-only success alternative.
   * Arrange: Select a status result.
   * Act: Construct success and inspect it.
   * Expect: Success has no error.
   */
  TEST(J5_003_Result, StatusSuccessHasNoError)
  {
    /* Arrange */
    using status = adaptive_json5::status_result;
    /* Act */
    const auto actual = status::success();
    /* Expect */
    EXPECT_TRUE(actual.has_value());
    EXPECT_EQ(actual.error_if(), nullptr);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify owned success-value construction.
   * Arrange: Choose the integer value 42.
   * Act: Construct and inspect a const result.
   * Expect: The value is 42 and error inspection is unavailable.
   */
  TEST(J5_003_Result, ValueSuccessHasNoError)
  {
    /* Arrange */
    constexpr int expected = 42;
    /* Act */
    const auto actual = int_result::success(expected);
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    ASSERT_NE(actual.value_if(), nullptr);
    EXPECT_EQ(*actual.value_if(), expected);
    EXPECT_EQ(actual.error_if(), nullptr);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify mutable and const access address the same value.
   * Arrange: Construct an integer success result.
   * Act: Modify the value through checked mutable access.
   * Expect: Const inspection sees the updated owned value.
   */
  TEST(J5_003_Result, MutableAccessUpdatesStoredValue)
  {
    /* Arrange */
    auto actual = int_result::success(42);
    /* Act */
    auto* value = actual.value_if();
    ASSERT_NE(value, nullptr);
    *value = 17;
    const auto& view = actual;
    /* Expect */
    EXPECT_EQ(view.value_if(), value);
    EXPECT_EQ(*view.value_if(), 17);
    EXPECT_TRUE(view.has_value());
    EXPECT_EQ(view.error_if(), nullptr);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify copying success and failure without changing their sources.
   * Arrange: Construct one value result and one failure.
   * Act: Copy both results.
   * Expect: Copies preserve the active alternative and exact contents.
   */
  TEST(J5_003_Result, CopyConstructionPreservesAlternatives)
  {
    /* Arrange */
    const auto success = int_result::success(42);
    const auto failure = int_result::failure(error::at_offset(error_code::syntax, 17));
    /* Act */
    const auto success_copy = success;
    const auto failure_copy = failure;
    /* Expect */
    ASSERT_NE(success_copy.value_if(), nullptr);
    EXPECT_EQ(*success_copy.value_if(), 42);
    EXPECT_EQ(success_copy.error_if(), nullptr);
    EXPECT_NE(success_copy.value_if(), success.value_if());
    EXPECT_FALSE(failure_copy.has_value());
    EXPECT_EQ(failure_copy.value_if(), nullptr);
    ASSERT_NE(failure_copy.error_if(), nullptr);
    EXPECT_EQ(failure_copy.error_if()->code, error_code::syntax);
    EXPECT_EQ(failure_copy.error_if()->byte_offset, 17U);
    EXPECT_TRUE(failure_copy.error_if()->has_byte_offset);
    EXPECT_TRUE(success.has_value());
    EXPECT_FALSE(failure.has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify both active alternatives survive move construction.
   * Arrange: Construct success and failure results.
   * Act: Move them into new results.
   * Expect: Destinations preserve the value or exact error.
   */
  TEST(J5_003_Result, MoveConstructionPreservesAlternatives)
  {
    /* Arrange */
    auto success = int_result::success(42);
    auto failure = int_result::failure(error::at_offset(error_code::encoding, 17));
    /* Act */
    const auto moved_success = std::move(success);
    const auto moved_failure = std::move(failure);
    /* Expect */
    ASSERT_NE(moved_success.value_if(), nullptr);
    EXPECT_EQ(*moved_success.value_if(), 42);
    EXPECT_EQ(moved_success.error_if(), nullptr);
    EXPECT_FALSE(moved_failure.has_value());
    EXPECT_EQ(moved_failure.value_if(), nullptr);
    ASSERT_NE(moved_failure.error_if(), nullptr);
    EXPECT_EQ(moved_failure.error_if()->code, error_code::encoding);
    EXPECT_EQ(moved_failure.error_if()->byte_offset, 17U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify construction, moving and extraction of a move-only value.
   * Arrange: Create host-test-owned unique ownership of integer 42.
   * Act: Transfer it into a result, move the result and extract the value.
   * Expect: The same resource survives each transfer without copying.
   */
  TEST(J5_003_Result, SupportsMoveOnlyValueOwnership)
  {
    /* Arrange */
    auto owned = std::make_unique<int>(42);
    const auto* expected_address = owned.get();
    /* Act */
    auto original = owned_result::success(std::move(owned));
    auto moved = std::move(original);
    ASSERT_NE(moved.value_if(), nullptr);
    auto extracted = std::move(*moved.value_if());
    /* Expect */
    EXPECT_EQ(owned, nullptr);
    ASSERT_NE(extracted, nullptr);
    EXPECT_EQ(extracted.get(), expected_address);
    EXPECT_EQ(*extracted, 42);
    EXPECT_TRUE(moved.has_value());
    EXPECT_EQ(moved.error_if(), nullptr);
    EXPECT_EQ(*moved.value_if(), nullptr);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify alternative selection when value and error types are identical.
   * Arrange: Choose result<int, int> and two different integers.
   * Act: Construct success and failure using the named factories.
   * Expect: Only the selected alternative is accessible.
   */
  TEST(J5_003_Result, SameTypeAlternativesRemainDistinct)
  {
    /* Arrange */
    using same_type_result = adaptive_json5::result<int, int>;
    /* Act */
    const auto success = same_type_result::success(11);
    const auto failure = same_type_result::failure(12);
    /* Expect */
    EXPECT_TRUE(success.has_value());
    ASSERT_NE(success.value_if(), nullptr);
    EXPECT_EQ(*success.value_if(), 11);
    EXPECT_EQ(success.error_if(), nullptr);
    EXPECT_FALSE(failure.has_value());
    EXPECT_EQ(failure.value_if(), nullptr);
    ASSERT_NE(failure.error_if(), nullptr);
    EXPECT_EQ(*failure.error_if(), 12);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the void specialization retains errors across copy and move.
   * Arrange: Construct a status failure without input location.
   * Act: Copy it and move the copy.
   * Expect: Destinations retain the exact unlocated error.
   */
  TEST(J5_003_Result, StatusCopyAndMovePreserveFailure)
  {
    /* Arrange */
    const auto original =
      adaptive_json5::status_result::failure(error::without_location(error_code::wrong_type_or_access));
    /* Act */
    auto copied = original;
    const auto moved = std::move(copied);
    /* Expect */
    EXPECT_FALSE(moved.has_value());
    ASSERT_NE(moved.error_if(), nullptr);
    EXPECT_EQ(moved.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_FALSE(moved.error_if()->has_byte_offset);
    EXPECT_EQ(moved.error_if()->byte_offset, 0U);
    EXPECT_FALSE(original.has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify existing status assignment changes the active state correctly.
   * Arrange: Prepare status success and failure.
   * Act: Copy failure onto success, then move success onto failure.
   * Expect: Error appears after failure assignment and disappears after success assignment.
   */
  TEST(J5_003_Result, StatusAssignmentTransitionsBothWays)
  {
    /* Arrange */
    auto actual = adaptive_json5::status_result::success();
    const auto failed = adaptive_json5::status_result::failure(error::at_offset(error_code::resource_limit, 17));
    auto succeeded = adaptive_json5::status_result::success();
    /* Act */
    actual = failed;
    const bool failure_state = !actual.has_value();
    const auto* stored_error = actual.error_if();
    const bool failure_contents = stored_error != nullptr && stored_error->code == error_code::resource_limit &&
                                  stored_error->byte_offset == 17 && stored_error->has_byte_offset;
    actual = std::move(succeeded);
    /* Expect */
    EXPECT_TRUE(failure_state);
    EXPECT_TRUE(failure_contents);
    EXPECT_TRUE(actual.has_value());
    EXPECT_EQ(actual.error_if(), nullptr);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify cleanup of nontrivial value alternatives.
   * Arrange: Start zeroed lifetime counters.
   * Act: Construct, copy and move successful tracked values in a scope.
   * Expect: Only active values are constructed and all constructed objects are destroyed.
   */
  TEST(J5_003_Result, BalancesSuccessAlternativeLifetimes)
  {
    /* Arrange */
    lifetime_counts counts;
    /* Act */
    {
      const auto failed = adaptive_json5::result<counted_value>::failure(error::without_location(error_code::syntax));
      EXPECT_EQ(counts.constructed, 0);
      EXPECT_FALSE(failed.has_value());
      auto first = adaptive_json5::result<counted_value>::success(counted_value{counts, 42});
      const auto copied = first;
      const auto moved = std::move(first);
      EXPECT_EQ(counts.live, 3);
      ASSERT_NE(copied.value_if(), nullptr);
      ASSERT_NE(moved.value_if(), nullptr);
      EXPECT_EQ(copied.value_if()->value(), 42);
      EXPECT_EQ(moved.value_if()->value(), 42);
    }
    /* Expect */
    EXPECT_EQ(counts.live, 0);
    EXPECT_EQ(counts.constructed, counts.destroyed);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify cleanup of nontrivial custom error alternatives.
   * Arrange: Start zeroed error lifetime counters.
   * Act: Construct, copy and move failures in a scope.
   * Expect: Success does not construct an error and every constructed error is destroyed.
   */
  TEST(J5_003_Result, BalancesErrorAlternativeLifetimes)
  {
    /* Arrange */
    lifetime_counts counts;
    /* Act */
    {
      const auto succeeded = adaptive_json5::result<int, counted_value>::success(42);
      EXPECT_TRUE(succeeded.has_value());
      EXPECT_EQ(counts.constructed, 0);
      auto first = adaptive_json5::result<int, counted_value>::failure(counted_value{counts, 7});
      const auto copied = first;
      const auto moved = std::move(first);
      EXPECT_EQ(counts.live, 3);
      ASSERT_NE(copied.error_if(), nullptr);
      ASSERT_NE(moved.error_if(), nullptr);
      EXPECT_EQ(copied.error_if()->value(), 7);
      EXPECT_EQ(moved.error_if()->value(), 7);
    }
    /* Expect */
    EXPECT_EQ(counts.live, 0);
    EXPECT_EQ(counts.constructed, counts.destroyed);
  }

  /* ================================= End Test_J5_003 ================================= */

  /* =================================== Test_J5_043 =================================== */

  /* Verify every stable error category and byte-offset boundary in both result forms.
   * Arrange: Select an error category and zero, ordinary or maximum byte offset.
   * Act: Construct a minimal error and both failed result forms.
   * Expect: Exact codes and locations survive; success-value access is unavailable.
   */
  TEST_P(J5_043_LocatedResult, PreservesCategoryAndOffset)
  {
    /* Arrange */
    const auto category = std::get<0>(GetParam());
    const auto offset = std::get<1>(GetParam());
    /* Act */
    const auto reason = error::at_offset(category.code, offset);
    const auto value = int_result::failure(reason);
    const auto status = adaptive_json5::status_result::failure(reason);
    /* Expect */
    EXPECT_EQ(static_cast<int>(category.code), category.expected_number);
    EXPECT_EQ(reason.code, category.code);
    EXPECT_EQ(reason.byte_offset, offset);
    EXPECT_TRUE(reason.has_byte_offset);
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.value_if(), nullptr);
    ASSERT_NE(value.error_if(), nullptr);
    EXPECT_EQ(value.error_if()->code, category.code);
    EXPECT_EQ(value.error_if()->byte_offset, offset);
    EXPECT_TRUE(value.error_if()->has_byte_offset);
    EXPECT_FALSE(status.has_value());
    ASSERT_NE(status.error_if(), nullptr);
    EXPECT_EQ(status.error_if()->code, category.code);
    EXPECT_EQ(status.error_if()->byte_offset, offset);
    EXPECT_TRUE(status.error_if()->has_byte_offset);
  }

  INSTANTIATE_TEST_SUITE_P(CategoriesAndBoundaries, J5_043_LocatedResult,
                           testing::Combine(testing::ValuesIn(code_cases),
                                            testing::Values(std::size_t{0}, std::size_t{17},
                                                            std::numeric_limits<std::size_t>::max())),
                           [](const testing::TestParamInfo<J5_043_LocatedResult::ParamType>& info)
                           {
                             const auto offset = std::get<1>(info.param);
                             const auto suffix =
                               offset == 0 ? "ByteZero" : (offset == 17 ? "ByteSeventeen" : "MaximumOffset");
                             return std::string(std::get<0>(info.param).name) + suffix;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify that missing locations remain distinct from the first input byte.
   * Arrange: Select each minimal error category.
   * Act: Construct located-at-zero and unlocated failures in both result forms.
   * Expect: The location flag distinguishes absence from byte zero without changing the code.
   */
  TEST_P(J5_043_UnlocatedResult, DistinguishesMissingLocationFromByteZero)
  {
    /* Arrange */
    const auto category = GetParam();
    /* Act */
    const auto located = error::at_offset(category.code, 0);
    const auto absent = error::without_location(category.code);
    const auto value = int_result::failure(absent);
    const auto status = adaptive_json5::status_result::failure(absent);
    /* Expect */
    EXPECT_TRUE(located.has_byte_offset);
    EXPECT_EQ(located.byte_offset, 0U);
    EXPECT_FALSE(absent.has_byte_offset);
    EXPECT_EQ(absent.byte_offset, 0U);
    EXPECT_EQ(absent.code, category.code);
    EXPECT_FALSE(value.has_value());
    EXPECT_EQ(value.value_if(), nullptr);
    ASSERT_NE(value.error_if(), nullptr);
    EXPECT_EQ(value.error_if()->code, category.code);
    EXPECT_FALSE(value.error_if()->has_byte_offset);
    EXPECT_EQ(value.error_if()->byte_offset, 0U);
    EXPECT_FALSE(status.has_value());
    ASSERT_NE(status.error_if(), nullptr);
    EXPECT_EQ(status.error_if()->code, category.code);
    EXPECT_FALSE(status.error_if()->has_byte_offset);
    EXPECT_EQ(status.error_if()->byte_offset, 0U);
  }

  INSTANTIATE_TEST_SUITE_P(Categories, J5_043_UnlocatedResult, testing::ValuesIn(code_cases),
                           [](const testing::TestParamInfo<code_case>& info)
                           {
                             return std::string(info.param.name);
                           });

  /* ================================= End Test_J5_043 ================================= */

  /* =================================== Test_J5_045 =================================== */

  // C++17 constant evaluation cannot use dynamic allocation for this reporting path.
  constexpr auto allocation_reason =
    error::at_offset(error_code::allocation_failure, std::numeric_limits<std::size_t>::max());
  constexpr auto allocation_value_result = int_result::failure(allocation_reason);
  constexpr auto allocation_status_result = adaptive_json5::status_result::failure(allocation_reason);
  static_assert(!allocation_value_result.has_value());
  static_assert(allocation_value_result.value_if() == nullptr);
  static_assert(allocation_value_result.error_if()->code == error_code::allocation_failure);
  static_assert(allocation_value_result.error_if()->byte_offset == std::numeric_limits<std::size_t>::max());
  static_assert(!allocation_status_result.has_value());
  static_assert(allocation_status_result.error_if()->code == error_code::allocation_failure);

  /* Verify the minimal allocation-failure outcome is available in constant evaluation.
   * Arrange: Use compile-time failure results with the largest representable location.
   * Act: Inspect the same results at runtime.
   * Expect: Both result forms retain allocation failure and exact location without rich diagnostics.
   */
  TEST(J5_045_Result, ReportsAllocationFailureWithoutRichDiagnostics)
  {
    /* Arrange */
    constexpr auto expected_offset = std::numeric_limits<std::size_t>::max();
    /* Act */
    const auto* value_error = allocation_value_result.error_if();
    const auto* status_error = allocation_status_result.error_if();
    /* Expect */
    ASSERT_NE(value_error, nullptr);
    ASSERT_NE(status_error, nullptr);
    EXPECT_EQ(value_error->code, error_code::allocation_failure);
    EXPECT_EQ(value_error->byte_offset, expected_offset);
    EXPECT_TRUE(value_error->has_byte_offset);
    EXPECT_EQ(status_error->code, error_code::allocation_failure);
    EXPECT_EQ(status_error->byte_offset, expected_offset);
    EXPECT_TRUE(status_error->has_byte_offset);
  }

  /* ================================= End Test_J5_045 ================================= */
} // namespace
