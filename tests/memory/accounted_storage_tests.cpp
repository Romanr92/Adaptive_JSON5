#include "../support/guard_provider.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/owned_block.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::guard_provider;
  /* =================================== Test_J5_046 =================================== */
  /**
   * @brief Owned-byte boundary cases with literal success expectations.
   * @par Requirements
   * P031; J5-046; J5-047.
   */
  struct allocation_case
  {
      std::size_t limit; // Input requested-byte ceiling.
      std::size_t bytes; // Input allocation bytes.
      bool accepted;     // Expected allocation result.
  };
  class J5_046_AccountedBoundary : public testing::TestWithParam<allocation_case>
  {
  };
  /*
   * Bound allocation before provider invocation.
   * Arrange: Create accounting with the parameter ceiling.
   * Act: Request the parameter allocation and release its owner.
   * Expect: Exact current/peak accounting and no provider call on rejection.
   */
  TEST_P(J5_046_AccountedBoundary, EnforcesInclusiveBytes)
  {
    /* Arrange */
    guard_provider provider;
    const auto input = GetParam();
    accounted_storage storage{provider.callbacks(), input.limit};
    /* Act */
    {
      const auto block = storage.allocate(input.bytes, alignof(char));
      /* Expect */
      EXPECT_EQ(block.has_value(), input.accepted);
      if (input.accepted)
      {
        ASSERT_TRUE(block.has_value());
        EXPECT_EQ(block.value_if()->size(), input.bytes);
        EXPECT_EQ(storage.current_bytes(), input.bytes);
        EXPECT_EQ(storage.peak_bytes(), input.bytes);
        EXPECT_EQ(provider.m_attempts, input.bytes == 0 ? 0u : 1u);
      }
      else
      {
        EXPECT_EQ(block.error_if()->code, error_code::resource_limit);
        EXPECT_EQ(provider.m_attempts, 0u);
        EXPECT_EQ(storage.current_bytes(), 0u);
        EXPECT_EQ(storage.peak_bytes(), 0u);
      }
    }
    EXPECT_EQ(storage.current_bytes(), 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  INSTANTIATE_TEST_SUITE_P(P031, J5_046_AccountedBoundary,
                           testing::Values(allocation_case{0, 0, true}, allocation_case{0, 1, false},
                                           allocation_case{8, 7, true}, allocation_case{8, 8, true},
                                           allocation_case{8, 9, false}));
  /*
   * TracksOverlapPeakAndMoveRelease.
   * Arrange: Keep one block live under a ten-byte ceiling.
   * Act: Reject excessive coexistence, reach the limit, then replace the first owner.
   * Expect: Owner moves do not charge twice and release restores the whole ceiling.
   */
  TEST(J5_046_AccountedStorage, TracksOverlapPeakAndMoveRelease)
  {
    /* Arrange */
    guard_provider provider;
    accounted_storage storage{provider.callbacks(), 10};
    auto first = storage.allocate(6, alignof(char));
    ASSERT_TRUE(first.has_value());
    /* Act */
    const auto denied = storage.allocate(5, alignof(char));
    EXPECT_EQ(provider.m_attempts, 1u);
    {
      auto second = storage.allocate(4, alignof(char));
      ASSERT_TRUE(second.has_value());
      EXPECT_EQ(storage.current_bytes(), 10u);
      EXPECT_EQ(storage.peak_bytes(), 10u);
      owned_block moved{std::move(*second.value_if())};
      EXPECT_EQ(second.value_if()->size(), 0u);
      *first.value_if() = std::move(moved);
      EXPECT_EQ(storage.current_bytes(), 4u);
      EXPECT_EQ(moved.size(), 0u);
    }
    *first.value_if() = owned_block{};
    const auto retry = storage.allocate(10, alignof(char));
    /* Expect */
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(storage.current_bytes(), 10u);
    EXPECT_EQ(storage.peak_bytes(), 10u);
    EXPECT_EQ(provider.m_attempts, 3u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * FailurePreservesUsageAndAllowsRetry.
   * Arrange: Hold three bytes and fail the next provider request.
   * Act: Fail a seven-byte allocation and then retry it.
   * Expect: Provider failure differs from resource exhaustion and consumes no bytes.
   */
  TEST(J5_046_AccountedStorage, FailurePreservesUsageAndAllowsRetry)
  {
    /* Arrange */
    guard_provider provider;
    accounted_storage storage{provider.callbacks(), 10};
    const auto held = storage.allocate(3, alignof(char));
    ASSERT_TRUE(held.has_value());
    provider.m_fail_on = provider.m_attempts + 1;
    /* Act */
    const auto failed = storage.allocate(7, alignof(char));
    EXPECT_EQ(storage.current_bytes(), 3u);
    EXPECT_EQ(storage.peak_bytes(), 3u);
    provider.m_fail_on = 0;
    const auto retry = storage.allocate(7, alignof(char));
    /* Expect */
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(storage.current_bytes(), 10u);
    EXPECT_EQ(storage.peak_bytes(), 10u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * ChecksArrayOverflowAlignmentAndProviders.
   * Arrange: Prepare valid accounting and invalid provider configurations.
   * Act: Request overflow, invalid alignments and missing providers.
   * Expect: Prechecks avoid provider calls and valid arrays charge the exact byte product.
   */
  TEST(J5_046_AccountedStorage, ChecksArrayOverflowAlignmentAndProviders)
  {
    /* Arrange */
    guard_provider provider;
    const auto callbacks = provider.callbacks();
    accounted_storage storage{callbacks, 64};
    accounted_storage missing{{}, 64};
    accounted_storage no_allocate{{callbacks.context, nullptr, callbacks.deallocate}, 64};
    accounted_storage no_release{{callbacks.context, callbacks.allocate, nullptr}, 64};
    /* Act */
    const auto overflow = storage.allocate_array(std::numeric_limits<std::size_t>::max(), 2, 1);
    const auto zero_alignment = storage.allocate(1, 0);
    const auto bad_alignment = storage.allocate(1, 3);
    const auto zero_bad_alignment = storage.allocate(0, 0);
    const auto absent = missing.allocate(0, 1);
    const auto absent_allocate = no_allocate.allocate(1, 1);
    const auto absent_release = no_release.allocate(1, 1);
    EXPECT_EQ(provider.m_attempts, 0u);
    const auto array = storage.allocate_array(3, sizeof(char16_t), alignof(char16_t));
    /* Expect */
    ASSERT_FALSE(overflow.has_value());
    EXPECT_EQ(overflow.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(zero_alignment.has_value());
    ASSERT_FALSE(bad_alignment.has_value());
    EXPECT_EQ(zero_alignment.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(bad_alignment.error_if()->code, error_code::resource_limit);
    EXPECT_FALSE(zero_bad_alignment.has_value());
    ASSERT_FALSE(absent.has_value());
    ASSERT_FALSE(absent_allocate.has_value());
    ASSERT_FALSE(absent_release.has_value());
    EXPECT_EQ(absent.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(absent_allocate.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(absent_release.error_if()->code, error_code::allocation_failure);
    ASSERT_TRUE(array.has_value());
    EXPECT_EQ(storage.current_bytes(), 3 * sizeof(char16_t));
    EXPECT_EQ(array.value_if()->alignment(), alignof(char16_t));
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_046 ================================= */
} // namespace
