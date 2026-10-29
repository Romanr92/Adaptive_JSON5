#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "memory/buffer_document_storage.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  using detail::document_storage_owner;
  using detail::make_buffer_document_storage;

  /* =================================== Test_J5_040 =================================== */
  static_assert(std::is_nothrow_move_constructible_v<document_storage_owner>);
  static_assert(std::is_nothrow_destructible_v<document_storage_owner>);
  static_assert(!std::is_copy_constructible_v<document_storage_owner>);
  static_assert(!std::is_copy_assignable_v<document_storage_owner>);
  static_assert(!std::is_move_assignable_v<document_storage_owner>);

  /**
   * @brief Observe ownership release without allocating or freeing test storage.
   * @par Requirements
   * J5-040; J5-051.
   */
  struct release_probe
  {
      accounted_storage accounting{{}, 0};
      std::size_t releases{};

      /**
       * @brief Record each release through the original context address.
       * @par Requirements
       * J5-040; J5-051.
       */
      static void release(void* context) noexcept
      {
        ++static_cast<release_probe*>(context)->releases;
      }
  };
  /*
   * Moving an owner through a result preserves the accounting and releases once.
   * Arrange: Adopt a stack probe with a counted release callback.
   * Act: Move into another owner and then into and out of a result.
   * Expect: Moved-from owners are empty and only the final owner releases.
   */
  TEST(J5_040_DocumentStorageOwner, MovesPreserveAccountingAndReleaseOnce)
  {
    /* Arrange */
    release_probe probe;
    /* Act */
    {
      document_storage_owner original{&probe, &probe.accounting, &release_probe::release};
      {
        document_storage_owner moved{std::move(original)};
        auto wrapped = result<document_storage_owner>::success(std::move(moved));
        {
          document_storage_owner final_owner{std::move(*wrapped.value_if())};
          /* Expect */
          EXPECT_EQ(original.accounting(), nullptr);
          EXPECT_EQ(moved.accounting(), nullptr);
          EXPECT_EQ(wrapped.value_if()->accounting(), nullptr);
          EXPECT_EQ(final_owner.accounting(), &probe.accounting);
          EXPECT_EQ(probe.releases, 0u);
        }
        EXPECT_EQ(probe.releases, 1u);
      }
      EXPECT_EQ(probe.releases, 1u);
    }
    EXPECT_EQ(probe.releases, 1u);
  }
  /* ================================= End Test_J5_040 ================================= */

  /* =================================== Test_J5_046 =================================== */
  class J5_046_BufferDocumentAlignment : public testing::TestWithParam<std::size_t>
  {
  };
  /*
   * Construct storage at several offsets without touching surrounding bytes.
   * Arrange: Reserve 256 bytes inside a guarded backing buffer.
   * Act: Construct storage and allocate 16 bytes with alignment 32.
   * Expect: Accounting and allocation reside inside the slice; guards survive.
   */
  TEST_P(J5_046_BufferDocumentAlignment, FitsWithinCallerSlice)
  {
    /* Arrange */
    alignas(64) unsigned char bytes[384]{};
    const auto offset = 32 + GetParam();
    bytes[offset - 1] = 0x5a;
    bytes[offset + 256] = 0xa5;
    const auto begin = reinterpret_cast<std::uintptr_t>(bytes + offset);
    /* Act */
    {
      auto storage = make_buffer_document_storage(bytes + offset, 256, 64);
      ASSERT_TRUE(storage.has_value());
      auto* accounting = storage.value_if()->accounting();
      ASSERT_NE(accounting, nullptr);
      const auto block = accounting->allocate(16, 32);
      /* Expect */
      ASSERT_TRUE(block.has_value());
      const auto address = reinterpret_cast<std::uintptr_t>(block.value_if()->data());
      const auto accounting_address = reinterpret_cast<std::uintptr_t>(accounting);
      EXPECT_GE(accounting_address, begin);
      EXPECT_LE(accounting_address + sizeof(accounted_storage), begin + 256);
      EXPECT_GE(address, accounting_address + sizeof(accounted_storage));
      EXPECT_LE(address + 16, begin + 256);
      EXPECT_EQ(address % 32, 0u);
      EXPECT_EQ(accounting->current_bytes(), 16u);
    }
    EXPECT_EQ(bytes[offset - 1], 0x5a);
    EXPECT_EQ(bytes[offset + 256], 0xa5);
  }
  INSTANTIATE_TEST_SUITE_P(P003, J5_046_BufferDocumentAlignment, testing::Values(0u, 1u, 7u, 31u));
  /* ----------------------------------------------------------------------------------- */
  /*
   * Reject unusable control-block buffers without writing to the caller's bytes.
   * Arrange: Prepare a null buffer and a one-byte buffer with a sentinel.
   * Act: Attempt construction with null, zero-capacity and undersized storage.
   * Expect: Explicit allocation failures and unchanged sentinel.
   */
  TEST(J5_046_BufferDocumentStorage, RejectsNullEmptyAndUndersizedBuffers)
  {
    /* Arrange */
    unsigned char byte = 0x5a;
    /* Act */
    const auto null_storage = make_buffer_document_storage(nullptr, 1024, 64);
    const auto empty = make_buffer_document_storage(&byte, 0, 64);
    const auto small = make_buffer_document_storage(&byte, 1, 64);
    /* Expect */
    for (const auto* attempt : {&null_storage, &empty, &small})
    {
      ASSERT_FALSE(attempt->has_value());
      EXPECT_EQ(attempt->error_if()->code, error_code::allocation_failure);
    }
    EXPECT_EQ(byte, 0x5a);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Physical exhaustion preserves accounting and permits a smaller retry.
   * Arrange: Construct storage in a 256-byte buffer with a 1024-byte ceiling.
   * Act: Request 512 bytes, then request eight bytes.
   * Expect: allocation_failure without charges, followed by successful allocation.
   */
  TEST(J5_046_BufferDocumentStorage, PhysicalExhaustionAllowsRetry)
  {
    /* Arrange */
    alignas(64) unsigned char bytes[256]{};
    auto storage = make_buffer_document_storage(bytes, sizeof(bytes), 1024);
    ASSERT_TRUE(storage.has_value());
    auto* accounting = storage.value_if()->accounting();
    /* Act */
    const auto denied = accounting->allocate(512, 1);
    /* Expect */
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(accounting->current_bytes(), 0u);
    EXPECT_EQ(accounting->peak_bytes(), 0u);
    const auto retry = accounting->allocate(8, 1);
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(accounting->current_bytes(), 8u);
  }
  /* ================================= End Test_J5_046 ================================= */

  /* =================================== Test_J5_047 =================================== */
  /*
   * Control-block overhead is excluded from the requested-byte ceiling.
   * Arrange: Construct storage with a zero-byte ceiling.
   * Act: Request zero bytes and one byte.
   * Expect: Storage creation succeeds, empty allocation succeeds and one byte fails.
   */
  TEST(J5_047_BufferDocumentStorage, ZeroLimitExcludesControlBlock)
  {
    /* Arrange */
    alignas(64) unsigned char bytes[256]{};
    auto storage = make_buffer_document_storage(bytes, sizeof(bytes), 0);
    ASSERT_TRUE(storage.has_value());
    auto* accounting = storage.value_if()->accounting();
    /* Act */
    const auto empty = accounting->allocate(0, 1);
    const auto denied = accounting->allocate(1, 1);
    /* Expect */
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty.value_if()->data(), nullptr);
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(accounting->byte_limit(), 0u);
    EXPECT_EQ(accounting->current_bytes(), 0u);
    EXPECT_EQ(accounting->peak_bytes(), 0u);
  }
  /* ================================= End Test_J5_047 ================================= */

  /* =================================== Test_J5_051 =================================== */
  /*
   * Move storage with a live block and reuse the buffer after destruction.
   * Arrange: Allocate bytes through a buffer-backed owner.
   * Act: Move the owner, release the block, destroy storage and reconstruct it.
   * Expect: Accounting remains stable and reconstructed storage starts empty.
   */
  TEST(J5_051_BufferDocumentStorage, LiveAllocationSurvivesMoveAndBufferIsReusable)
  {
    /* Arrange */
    alignas(64) unsigned char bytes[256]{};
    void* original_address = nullptr;
    /* Act */
    {
      auto storage = make_buffer_document_storage(bytes, sizeof(bytes), 64);
      ASSERT_TRUE(storage.has_value());
      auto* accounting = storage.value_if()->accounting();
      auto block = accounting->allocate(8, 1);
      ASSERT_TRUE(block.has_value());
      original_address = block.value_if()->data();
      static_cast<unsigned char*>(original_address)[0] = 42;
      document_storage_owner moved{std::move(*storage.value_if())};
      /* Expect */
      EXPECT_EQ(storage.value_if()->accounting(), nullptr);
      EXPECT_EQ(moved.accounting(), accounting);
      EXPECT_EQ(static_cast<unsigned char*>(block.value_if()->data())[0], 42);
      *block.value_if() = owned_block{};
      EXPECT_EQ(accounting->current_bytes(), 0u);
    }
    auto rebuilt = make_buffer_document_storage(bytes, sizeof(bytes), 64);
    ASSERT_TRUE(rebuilt.has_value());
    EXPECT_EQ(rebuilt.value_if()->accounting()->current_bytes(), 0u);
    EXPECT_EQ(rebuilt.value_if()->accounting()->peak_bytes(), 0u);
    const auto retry = rebuilt.value_if()->accounting()->allocate(8, 1);
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(retry.value_if()->data(), original_address);
  }
  /* ================================= End Test_J5_051 ================================= */
} // namespace
