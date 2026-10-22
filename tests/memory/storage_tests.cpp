#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>

namespace
{
  using adaptive_json5::allocate_array;
  using adaptive_json5::allocate_block;
  using adaptive_json5::error_code;
  using adaptive_json5::owned_block;
  using adaptive_json5::storage_callbacks;

  constexpr std::size_t c_max = std::numeric_limits<std::size_t>::max();

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Caller-owned test provider with injectable allocation failure.
   * @par Requirements
   * P003; J5-040; J5-046; J5-101.
   * @note Fixed slots avoid dynamic storage. The provider must outlive its callbacks.
   */
  class counting_provider
  {
    public:
      static constexpr std::size_t c_slot_bytes = 64;
      static constexpr std::size_t c_slot_count = 4;

      // Allow only this many successful allocations before simulating exhaustion.
      explicit counting_provider(std::size_t successful_allocations) noexcept : m_remaining(successful_allocations) {}

      storage_callbacks callbacks() noexcept
      {
        return storage_callbacks{this, &allocate_thunk, &deallocate_thunk};
      }

      std::size_t allocations() const noexcept
      {
        return m_allocations;
      }

      std::size_t releases() const noexcept
      {
        return m_releases;
      }

      std::size_t failures() const noexcept
      {
        return m_failures;
      }

      std::size_t outstanding_bytes() const noexcept
      {
        return m_outstanding_bytes;
      }

      void* last_released() const noexcept
      {
        return m_last_released;
      }

      std::size_t last_alignment() const noexcept
      {
        return m_last_alignment;
      }

    private:
      static void* allocate_thunk(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        return static_cast<counting_provider*>(context)->allocate(bytes, alignment);
      }

      static void deallocate_thunk(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        static_cast<counting_provider*>(context)->deallocate(address, bytes, alignment);
      }

      void* allocate(std::size_t bytes, std::size_t alignment) noexcept
      {
        m_last_alignment = alignment;
        if (m_remaining == 0 || bytes > c_slot_bytes || m_next_slot >= c_slot_count)
        {
          ++m_failures;
          return nullptr;
        }

        --m_remaining;
        ++m_allocations;
        m_outstanding_bytes += bytes;
        return m_slots[m_next_slot++].data();
      }

      void deallocate(void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        m_last_released = address;
        ++m_releases;
        m_outstanding_bytes -= bytes;
        m_last_alignment = alignment;
      }

      alignas(std::max_align_t) std::array<std::array<unsigned char, c_slot_bytes>, c_slot_count> m_slots{};
      std::size_t m_next_slot = 0;
      std::size_t m_remaining;
      std::size_t m_allocations = 0;
      std::size_t m_releases = 0;
      std::size_t m_failures = 0;
      std::size_t m_outstanding_bytes = 0;
      std::size_t m_last_alignment = 0;
      void* m_last_released = nullptr;
  };

  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_046 =================================== */

  static_assert(
    std::is_nothrow_invocable_r_v<void*, adaptive_json5::allocate_function, void*, std::size_t, std::size_t>);
  static_assert(
    std::is_nothrow_invocable_r_v<void, adaptive_json5::deallocate_function, void*, void*, std::size_t, std::size_t>);
  static_assert(std::is_nothrow_default_constructible_v<storage_callbacks>);

  /* Verify a default storage description supplies no usable provider.
   * Arrange: Choose default storage callbacks.
   * Act: Construct them.
   * Expect: Context and both callbacks are null.
   */
  TEST(J5_046_Storage, DefaultHasNoProvider)
  {
    /* Arrange */
    /* Act */
    const storage_callbacks actual{};
    /* Expect */
    EXPECT_EQ(actual.context, nullptr);
    EXPECT_EQ(actual.allocate, nullptr);
    EXPECT_EQ(actual.deallocate, nullptr);
  }

  /* ------------------------------------------------------------------------------- */

  /* Verify allocation failure is reported as nullptr and leaves nothing to release.
   * Arrange: Create a provider that is already exhausted.
   * Act: Request aligned storage through its callbacks.
   * Expect: The request returns nullptr and no allocation or release is recorded.
   */
  TEST(J5_046_Storage, ReportsFailureAsNullWithoutOutstandingStorage)
  {
    /* Arrange */
    counting_provider provider{0};
    const storage_callbacks callbacks = provider.callbacks();
    /* Act */
    void* const actual = callbacks.allocate(callbacks.context, 16, alignof(std::max_align_t));
    /* Expect */
    EXPECT_EQ(actual, nullptr);
    EXPECT_EQ(provider.failures(), 1U);
    EXPECT_EQ(provider.allocations(), 0U);
    EXPECT_EQ(provider.releases(), 0U);
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  /* ------------------------------------------------------------------------------- */

  /* Verify each allocation can fail independently and successful ones release in balance.
   * Arrange: Allow exactly two allocations, then request three blocks.
   * Act: Allocate, observe the third failure and release the successes.
   * Expect: Two aligned blocks succeed, the third is nullptr and outstanding bytes return to zero.
   */
  TEST(J5_046_Storage, BalancesReleaseAfterInjectedFailure)
  {
    /* Arrange */
    counting_provider provider{2};
    const storage_callbacks callbacks = provider.callbacks();
    /* Act */
    void* const first = callbacks.allocate(callbacks.context, 8, alignof(std::max_align_t));
    void* const second = callbacks.allocate(callbacks.context, 24, alignof(std::max_align_t));
    void* const third = callbacks.allocate(callbacks.context, 8, alignof(std::max_align_t));
    const std::size_t outstanding_before_release = provider.outstanding_bytes();
    callbacks.deallocate(callbacks.context, first, 8, alignof(std::max_align_t));
    callbacks.deallocate(callbacks.context, second, 24, alignof(std::max_align_t));
    /* Expect */
    EXPECT_NE(first, nullptr);
    EXPECT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(first) % alignof(std::max_align_t), 0U);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(second) % alignof(std::max_align_t), 0U);
    EXPECT_EQ(third, nullptr);
    EXPECT_EQ(outstanding_before_release, 32U);
    EXPECT_EQ(provider.allocations(), 2U);
    EXPECT_EQ(provider.failures(), 1U);
    EXPECT_EQ(provider.releases(), provider.allocations());
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  /* ------------------------------------------------------------------------------- */

  /* Verify release receives the original size and alignment of the allocation.
   * Arrange: Create a provider and allocate with a non-default alignment.
   * Act: Release the block with the same description.
   * Expect: The provider observes the same alignment during allocation and release.
   */
  TEST(J5_040_Storage, ReleasePreservesAllocationDescription)
  {
    /* Arrange */
    counting_provider provider{1};
    const storage_callbacks callbacks = provider.callbacks();
    void* const block = callbacks.allocate(callbacks.context, 12, 4);
    const std::size_t allocation_alignment = provider.last_alignment();
    /* Act */
    callbacks.deallocate(callbacks.context, block, 12, 4);
    /* Expect */
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(provider.last_released(), block);
    EXPECT_EQ(allocation_alignment, 4U);
    EXPECT_EQ(provider.last_alignment(), 4U);
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  /**
   * @brief Describe a provider budget for allocation-point injection.
   * @par Requirements
   * P003; J5-046; J5-051.
   */
  struct budget_case
  {
      const char* name;
      std::size_t budget;             // Successful provider allocations permitted.
      std::size_t expected_successes; // Blocks of three requests that succeed.
  };

  std::ostream& operator<<(std::ostream& stream, const budget_case& value)
  {
    return stream << value.name;
  }

  class OwnedBlockAllocationPointUnits : public ::testing::TestWithParam<budget_case>
  {
  };

  /* Verify a failure at each allocation point is explicit and cleanup is balanced.
   * Arrange: Create a provider that permits a chosen number of allocations.
   * Act: Request three blocks and keep the successful owners until scope exit.
   * Expect: Requests beyond the budget return allocation_failure; every success is released once.
   */
  TEST_P(OwnedBlockAllocationPointUnits, ReportsFailureAndBalancesRelease)
  {
    /* Arrange */
    const budget_case& param = GetParam();
    counting_provider provider{param.budget};
    std::size_t successes = 0;
    std::size_t failures = 0;
    /* Act */
    {
      std::array<std::optional<owned_block>, 3> owners{};
      for (auto& owner : owners)
      {
        auto allocated = allocate_block(provider.callbacks(), 8, 8);
        if (allocated.has_value())
        {
          owner.emplace(std::move(*allocated.value_if()));
          ++successes;
        }
        else
        {
          ASSERT_NE(allocated.error_if(), nullptr);
          EXPECT_EQ(allocated.error_if()->code, error_code::allocation_failure);
          EXPECT_FALSE(allocated.error_if()->has_byte_offset);
          ++failures;
        }
      }
      EXPECT_EQ(provider.releases(), 0U);
    }
    /* Expect */
    EXPECT_EQ(successes, param.expected_successes);
    EXPECT_EQ(failures, 3U - param.expected_successes);
    EXPECT_EQ(provider.allocations(), param.expected_successes);
    EXPECT_EQ(provider.failures(), failures);
    EXPECT_EQ(provider.releases(), provider.allocations());
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  INSTANTIATE_TEST_SUITE_P(J5_046_OwnedBlock, OwnedBlockAllocationPointUnits,
                           ::testing::Values(budget_case{"FailsFirst", 0, 0}, budget_case{"FailsSecond", 1, 1},
                                             budget_case{"FailsThird", 2, 2}, budget_case{"NeverFails", 3, 3}),
                           [](const ::testing::TestParamInfo<budget_case>& info)
                           {
                             return info.param.name;
                           });

  /* ------------------------------------------------------------------------------- */

  /* Verify an unusable provider is rejected before any callback runs.
   * Arrange: Build callbacks lacking allocate, then lacking deallocate.
   * Act: Request a block through each.
   * Expect: Both return allocation_failure and the provider is untouched.
   */
  TEST(J5_046_OwnedBlock, RejectsMissingCallbacks)
  {
    /* Arrange */
    counting_provider provider{1};
    storage_callbacks no_allocate = provider.callbacks();
    no_allocate.allocate = nullptr;
    storage_callbacks no_deallocate = provider.callbacks();
    no_deallocate.deallocate = nullptr;
    /* Act */
    const auto first = allocate_block(no_allocate, 8, 8);
    const auto second = allocate_block(no_deallocate, 8, 8);
    const auto third = allocate_block(storage_callbacks{}, 8, 8);
    /* Expect */
    for (const auto* actual : {&first, &second, &third})
    {
      ASSERT_FALSE(actual->has_value());
      EXPECT_EQ(actual->error_if()->code, error_code::allocation_failure);
    }
    EXPECT_EQ(provider.allocations(), 0U);
    EXPECT_EQ(provider.failures(), 0U);
    EXPECT_EQ(provider.releases(), 0U);
  }

  /* ------------------------------------------------------------------------------- */

  class OwnedBlockAlignmentUnits : public ::testing::TestWithParam<std::size_t>
  {
  };

  /* Verify alignments that are not a non-zero power of two are rejected.
   * Arrange: Create a provider and choose an invalid alignment.
   * Act: Request a block with that alignment.
   * Expect: The result is resource_limit and the provider is not called.
   */
  TEST_P(OwnedBlockAlignmentUnits, RejectsInvalidAlignmentBeforeProviderCall)
  {
    /* Arrange */
    counting_provider provider{1};
    /* Act */
    const auto actual = allocate_block(provider.callbacks(), 8, GetParam());
    /* Expect */
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(provider.allocations(), 0U);
    EXPECT_EQ(provider.failures(), 0U);
  }

  INSTANTIATE_TEST_SUITE_P(J5_046_OwnedBlock, OwnedBlockAlignmentUnits,
                           ::testing::Values<std::size_t>(0, 3, 6, 12, c_max)); // Invalid alignment in bytes.

  /* ------------------------------------------------------------------------------- */

  /* Verify array allocation multiplies exactly and owns the result.
   * Arrange: Create a provider with one allocation available.
   * Act: Request twelve four-byte elements.
   * Expect: One 48-byte block with four-byte alignment is owned and released.
   */
  TEST(J5_046_OwnedBlock, AllocatesArrayWithExactSize)
  {
    /* Arrange */
    counting_provider provider{1};
    /* Act */
    {
      const auto actual = allocate_array(provider.callbacks(), 12, 4, 4);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.value_if()->size(), 48U);
      EXPECT_EQ(actual.value_if()->alignment(), 4U);
    }
    EXPECT_EQ(provider.releases(), 1U);
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  /* ------------------------------------------------------------------------------- */

  /* Verify array size overflow stops before the provider is called.
   * Arrange: Create a provider and choose count and element size whose product overflows.
   * Act: Request the array.
   * Expect: The result is resource_limit without location and the provider is untouched.
   */
  TEST(J5_046_OwnedBlock, ArrayOverflowReportsResourceLimitWithoutProviderCall)
  {
    /* Arrange */
    counting_provider provider{1};
    /* Act */
    const auto actual = allocate_array(provider.callbacks(), c_max / 2 + 1, 2, 8);
    /* Expect */
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, error_code::resource_limit);
    EXPECT_FALSE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(provider.allocations(), 0U);
    EXPECT_EQ(provider.failures(), 0U);
  }

  /* ================================= End Test_J5_046 ================================= */

  /* =================================== Test_J5_051 =================================== */

  /* Verify a failed request publishes nothing and a later request still succeeds.
   * Arrange: Create a provider with two allocations available.
   * Act: Request a valid block, an oversized block that fails, then another valid block.
   * Expect: The failure carries no block, both valid blocks succeed and all storage is released.
   */
  TEST(J5_051_OwnedBlock, ProviderRemainsUsableAfterFailedRequest)
  {
    /* Arrange */
    counting_provider provider{2};
    /* Act */
    {
      const auto first = allocate_block(provider.callbacks(), 8, 8);
      const auto oversized = allocate_block(provider.callbacks(), counting_provider::c_slot_bytes + 1, 8);
      const auto third = allocate_block(provider.callbacks(), 8, 8);
      /* Expect */
      ASSERT_TRUE(first.has_value());
      ASSERT_FALSE(oversized.has_value());
      EXPECT_EQ(oversized.value_if(), nullptr);
      EXPECT_EQ(oversized.error_if()->code, error_code::allocation_failure);
      ASSERT_TRUE(third.has_value());
      EXPECT_NE(first.value_if()->data(), third.value_if()->data());
      EXPECT_EQ(provider.outstanding_bytes(), 16U);
    }
    EXPECT_EQ(provider.allocations(), 2U);
    EXPECT_EQ(provider.failures(), 1U);
    EXPECT_EQ(provider.releases(), 2U);
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  /* ================================= End Test_J5_051 ================================= */

  /* =================================== Test_J5_040 =================================== */

  static_assert(std::is_nothrow_default_constructible_v<owned_block>);
  static_assert(std::is_nothrow_move_constructible_v<owned_block>);
  static_assert(std::is_nothrow_destructible_v<owned_block>);
  static_assert(!std::is_copy_constructible_v<owned_block>);
  static_assert(!std::is_copy_assignable_v<owned_block>);
  static_assert(std::is_nothrow_move_assignable_v<owned_block>);

  /* Verify a default block owns nothing.
   * Arrange: Choose the default owned block.
   * Act: Construct it and let it be destroyed.
   * Expect: Its pointer is null and its size and alignment are zero.
   */
  TEST(J5_040_OwnedBlock, DefaultOwnsNothing)
  {
    /* Arrange */
    /* Act */
    const owned_block actual{};
    /* Expect */
    EXPECT_EQ(actual.data(), nullptr);
    EXPECT_EQ(actual.size(), 0U);
    EXPECT_EQ(actual.alignment(), 0U);
  }

  /* ------------------------------------------------------------------------------- */

  /* Verify destruction releases the block once with its original description.
   * Arrange: Create a provider with one allocation available.
   * Act: Allocate a block, record it and leave its scope.
   * Expect: One release occurs for the same address, size and alignment; nothing is outstanding.
   */
  TEST(J5_040_OwnedBlock, ReleasesOnceOnDestruction)
  {
    /* Arrange */
    counting_provider provider{1};
    void* address = nullptr;
    /* Act */
    {
      auto allocated = allocate_block(provider.callbacks(), 24, 8);
      ASSERT_TRUE(allocated.has_value());
      address = allocated.value_if()->data();
      EXPECT_NE(address, nullptr);
      EXPECT_EQ(allocated.value_if()->size(), 24U);
      EXPECT_EQ(allocated.value_if()->alignment(), 8U);
      EXPECT_EQ(provider.releases(), 0U);
    }
    /* Expect */
    EXPECT_EQ(provider.allocations(), 1U);
    EXPECT_EQ(provider.releases(), 1U);
    EXPECT_EQ(provider.last_released(), address);
    EXPECT_EQ(provider.last_alignment(), 8U);
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  /* ------------------------------------------------------------------------------- */

  /* Verify moving transfers ownership without a double release.
   * Arrange: Allocate a block from a one-allocation provider.
   * Act: Move it into another owner and destroy both.
   * Expect: The target owns the address, the source is empty and exactly one release occurs.
   */
  TEST(J5_040_OwnedBlock, MoveTransfersOwnershipWithoutDoubleRelease)
  {
    /* Arrange */
    counting_provider provider{1};
    void* address = nullptr;
    /* Act */
    {
      auto allocated = allocate_block(provider.callbacks(), 16, 8);
      ASSERT_TRUE(allocated.has_value());
      owned_block source = std::move(*allocated.value_if());
      address = source.data();
      owned_block target = std::move(source);
      /* Expect */
      EXPECT_EQ(source.data(), nullptr);
      EXPECT_EQ(source.size(), 0U);
      EXPECT_EQ(target.data(), address);
      EXPECT_EQ(target.size(), 16U);
      EXPECT_EQ(provider.releases(), 0U);
    }
    EXPECT_EQ(provider.releases(), 1U);
    EXPECT_EQ(provider.last_released(), address);
    EXPECT_EQ(provider.outstanding_bytes(), 0U);
  }

  /* ------------------------------------------------------------------------------- */

  /* Verify a zero-byte request yields an empty block without using the provider.
   * Arrange: Create a provider that would fail every request.
   * Act: Request zero bytes and destroy the block.
   * Expect: The request succeeds with a null empty block and the provider is never called.
   */
  TEST(J5_040_OwnedBlock, ZeroByteRequestSucceedsWithoutProviderCall)
  {
    /* Arrange */
    counting_provider provider{0};
    /* Act */
    {
      const auto actual = allocate_block(provider.callbacks(), 0, 8);
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.value_if()->data(), nullptr);
      EXPECT_EQ(actual.value_if()->size(), 0U);
    }
    EXPECT_EQ(provider.allocations(), 0U);
    EXPECT_EQ(provider.failures(), 0U);
    EXPECT_EQ(provider.releases(), 0U);
  }

  /* ================================= End Test_J5_040 ================================= */
} // namespace
