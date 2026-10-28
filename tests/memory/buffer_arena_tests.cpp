#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/buffer_arena.hpp"
#include "adaptive_json5/memory/owned_block.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* =================================== Test_J5_046 =================================== */
  static_assert(!std::is_copy_constructible_v<buffer_arena>);
  static_assert(!std::is_move_constructible_v<buffer_arena>);
  static_assert(!std::is_copy_assignable_v<buffer_arena>);
  static_assert(!std::is_move_assignable_v<buffer_arena>);
  static_assert(std::is_nothrow_constructible_v<buffer_arena, void*, std::size_t>);
  static_assert(noexcept(std::declval<const buffer_arena&>().used()));

  /**
   * @brief Alignment cases with exact addresses and consumed-byte expectations.
   * @par Requirements
   * P003; J5-046.
   */
  struct alignment_case
  {
      std::size_t alignment; // Input alignment in bytes.
      std::size_t offset;    // Expected offset from the aligned backing buffer in bytes.
  };
  class J5_046_BufferArenaAlignment : public testing::TestWithParam<alignment_case>
  {
  };
  /*
   * Align allocations from a deliberately unaligned buffer.
   * Arrange: Start the arena one byte after a 64-byte aligned address.
   * Act: Allocate three bytes with the requested alignment.
   * Expect: Exact aligned address, padding charge and one live allocation.
   */
  TEST_P(J5_046_BufferArenaAlignment, ChargesPaddingAndReturnsAlignedStorage)
  {
    /* Arrange */
    alignas(64) unsigned char bytes[128]{};
    buffer_arena arena{bytes + 1, 127};
    const auto input = GetParam();
    /* Act */
    const auto block = allocate_block(arena.callbacks(), 3, input.alignment);
    /* Expect */
    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block.value_if()->data(), bytes + input.offset);
    EXPECT_EQ(arena.used(), input.offset - 1 + 3);
    EXPECT_EQ(arena.live_allocations(), 1u);
    EXPECT_EQ(arena.capacity(), 127u);
  }
  INSTANTIATE_TEST_SUITE_P(P003, J5_046_BufferArenaAlignment,
                           testing::Values(alignment_case{1, 1}, alignment_case{2, 2}, alignment_case{8, 8},
                                           alignment_case{64, 64}));
  /* ----------------------------------------------------------------------------------- */
  /*
   * Exhaustion is atomic and an exact-fit retry succeeds.
   * Arrange: Keep three bytes live in an eight-byte arena.
   * Act: Reject six bytes and then allocate the remaining five.
   * Expect: Failure preserves state and existing contents; exact fit succeeds.
   */
  TEST(J5_046_BufferArena, ExhaustionPreservesStateAndContents)
  {
    /* Arrange */
    unsigned char bytes[8]{};
    buffer_arena arena{bytes, sizeof(bytes)};
    const auto first = allocate_block(arena.callbacks(), 3, 1);
    ASSERT_TRUE(first.has_value());
    bytes[0] = 42;
    /* Act */
    const auto denied = allocate_block(arena.callbacks(), 6, 1);
    /* Expect */
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(arena.used(), 3u);
    EXPECT_EQ(arena.live_allocations(), 1u);
    EXPECT_EQ(bytes[0], 42);
    const auto exact = allocate_block(arena.callbacks(), 5, 1);
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(exact.value_if()->data(), bytes + 3);
    EXPECT_EQ(arena.used(), 8u);
    EXPECT_EQ(arena.live_allocations(), 2u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Empty arenas fail explicitly while zero-byte requests remain empty successes.
   * Arrange: Create null and nonnull zero-capacity arenas.
   * Act: Request one byte and zero bytes from each.
   * Expect: allocation_failure for nonempty requests and unchanged zero counters.
   */
  TEST(J5_046_BufferArena, EmptyBuffersAndZeroRequests)
  {
    /* Arrange */
    unsigned char byte{};
    buffer_arena null_arena{nullptr, 100};
    buffer_arena empty_arena{&byte, 0};
    /* Act */
    for (auto* arena : {&null_arena, &empty_arena})
    {
      const auto denied = allocate_block(arena->callbacks(), 1, 1);
      const auto empty = allocate_block(arena->callbacks(), 0, 1);
      /* Expect */
      ASSERT_FALSE(denied.has_value());
      EXPECT_EQ(denied.error_if()->code, error_code::allocation_failure);
      ASSERT_TRUE(empty.has_value());
      EXPECT_EQ(empty.value_if()->data(), nullptr);
      EXPECT_EQ(arena->capacity(), 0u);
      EXPECT_EQ(arena->used(), 0u);
      EXPECT_EQ(arena->live_allocations(), 0u);
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Oversized and invalid requests never consume arena storage.
   * Arrange: Prepare an eight-byte arena.
   * Act: Request maximum size, invalid alignment and overflowing array size.
   * Expect: Explicit failure categories with zero usage and live allocations.
   */
  TEST(J5_046_BufferArena, RejectsUnrepresentableAndInvalidRequests)
  {
    /* Arrange */
    unsigned char bytes[8]{};
    buffer_arena arena{bytes, sizeof(bytes)};
    /* Act */
    const auto huge = allocate_block(arena.callbacks(), std::numeric_limits<std::size_t>::max(), 1);
    const auto invalid = allocate_block(arena.callbacks(), 1, 3);
    const auto overflow = allocate_array(arena.callbacks(), std::numeric_limits<std::size_t>::max(), 2, 1);
    /* Expect */
    ASSERT_FALSE(huge.has_value());
    EXPECT_EQ(huge.error_if()->code, error_code::allocation_failure);
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(overflow.has_value());
    EXPECT_EQ(overflow.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.live_allocations(), 0u);
  }
  /* ================================= End Test_J5_046 ================================= */

  /* =================================== Test_J5_051 =================================== */
  /*
   * Partial release and ownership moves do not reuse occupied arena space.
   * Arrange: Fill an eight-byte arena with two blocks.
   * Act: Move the second owner, release the first, then release the last owner.
   * Expect: No reuse until the final release; the full buffer is then reusable.
   */
  TEST(J5_051_BufferArena, ResetsOnlyAfterLastOwnerReleases)
  {
    /* Arrange */
    unsigned char bytes[8]{};
    buffer_arena arena{bytes, sizeof(bytes)};
    auto first = allocate_block(arena.callbacks(), 3, 1);
    auto second = allocate_block(arena.callbacks(), 5, 1);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    /* Act */
    {
      owned_block moved{std::move(*second.value_if())};
      *first.value_if() = owned_block{};
      /* Expect */
      EXPECT_EQ(second.value_if()->data(), nullptr);
      EXPECT_EQ(arena.live_allocations(), 1u);
      EXPECT_EQ(arena.used(), 8u);
      const auto denied = allocate_block(arena.callbacks(), 1, 1);
      ASSERT_FALSE(denied.has_value());
      EXPECT_EQ(denied.error_if()->code, error_code::allocation_failure);
    }
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.live_allocations(), 0u);
    const auto retry = allocate_block(arena.callbacks(), 8, 1);
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(retry.value_if()->data(), bytes);
  }
  /* ================================= End Test_J5_051 ================================= */

  /* =================================== Test_J5_047 =================================== */
  /*
   * Accounting limits and physical arena exhaustion remain distinct.
   * Arrange: Wrap an eight-byte arena in a six-byte accounting ceiling.
   * Act: Reach the ceiling, reject another byte, release all blocks and retry.
   * Expect: resource_limit preserves both counters, cleanup resets the arena.
   */
  TEST(J5_047_BufferArena, AccountingLimitAndReset)
  {
    /* Arrange */
    unsigned char bytes[8]{};
    buffer_arena arena{bytes, sizeof(bytes)};
    accounted_storage storage{arena.callbacks(), 6};
    /* Act */
    {
      const auto first = storage.allocate(6, 1);
      ASSERT_TRUE(first.has_value());
      const auto denied = storage.allocate(1, 1);
      /* Expect */
      ASSERT_FALSE(denied.has_value());
      EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(storage.current_bytes(), 6u);
      EXPECT_EQ(storage.peak_bytes(), 6u);
      EXPECT_EQ(arena.used(), 6u);
      EXPECT_EQ(arena.live_allocations(), 1u);
    }
    EXPECT_EQ(storage.current_bytes(), 0u);
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.live_allocations(), 0u);
    const auto retry = storage.allocate(6, 1);
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(retry.value_if()->data(), bytes);
  }
  /* ================================= End Test_J5_047 ================================= */
} // namespace
