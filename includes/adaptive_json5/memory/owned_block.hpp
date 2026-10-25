#ifndef OWNED_BLOCK_HPP_
#define OWNED_BLOCK_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Move-only owner of one block obtained from caller-supplied storage.
   * @par Requirements
   * P003; J5-040; J5-046; J5-051; J5-101.
   * @note The provider state must outlive the block.
   * Destruction releases the block exactly once and never throws or allocates.
   */
  class [[nodiscard]] owned_block
  {
    private:
      // Only allocate_block may create a non-empty owner for a successful allocation.
      friend result<owned_block> allocate_block(const storage_callbacks&, std::size_t, std::size_t) noexcept;

      /**
       * @brief Adopt one successfully allocated block.
       * @par Requirements
       * P003; J5-040; J5-046.
       * @note The allocation description must match the originating provider.
       */
      owned_block(const storage_callbacks& storage, void* data, std::size_t bytes, std::size_t alignment) noexcept;

      storage_callbacks m_storage{};
      void* m_data = nullptr;
      std::size_t m_bytes = 0;
      std::size_t m_alignment = 0;

    public:
      /**
       * @brief Construct an empty block that owns nothing.
       * @par Requirements
       * P003; J5-040.
       */
      constexpr owned_block() noexcept = default;

      /**
       * @brief Transfer ownership and leave the source empty.
       * @par Requirements
       * P003; J5-040; J5-051.
       */
      owned_block(owned_block&& other) noexcept;

      /**
       * @brief Release the owned block, if any, through its original provider.
       * @par Requirements
       * P003; J5-040; J5-051; J5-101.
       */
      ~owned_block()
      {
        if (m_data != nullptr)
        {
          m_storage.deallocate(m_storage.context, m_data, m_bytes, m_alignment);
        }
      }

      // Copying would duplicate ownership.
      owned_block(const owned_block&) = delete;
      owned_block& operator=(const owned_block&) = delete;

      /**
       * @brief Release current storage and take ownership from another block.
       * @par Requirements
       * P003; P013; J5-040; J5-051.
       * @return This owner after assignment.
       * @note Self-move leaves the block unchanged.
       * Otherwise, the source becomes empty.
       * Views into the released destination allocation become invalid.
       * No allocation is performed.
       */
      owned_block& operator=(owned_block&& other) noexcept;

      /**
       * @brief Access the block's first byte.
       * @par Requirements
       * P003; J5-040.
       * @return Block address, or nullptr when empty.
       * @note The pointer remains dependent on this owner's lifetime.
       */
      [[nodiscard]] void* data() const noexcept;

      /**
       * @brief Return the owned allocation size in bytes.
       * @par Requirements
       * P003; J5-040.
       */
      [[nodiscard]] std::size_t size() const noexcept;

      /**
       * @brief Return the owned allocation's requested alignment.
       * @par Requirements
       * P003; J5-040.
       */
      [[nodiscard]] std::size_t alignment() const noexcept;
  };

  /**
   * @brief Allocate one block from caller-supplied storage.
   * @par Requirements
   * P003; P032; J5-003; J5-046; J5-047; J5-051; J5-101.
   * @return Owning block, an explicit result-callback failure,
   * allocation_failure for an unusable provider or null allocation,
   * or resource_limit for invalid alignment.
   * @note Requires deallocate and at least one allocation callback.
   * Prefers allocate_result and never falls back after its failure.
   * A zero-byte request succeeds after provider/alignment validation
   * without invoking either allocation callback.
   * Explicit callback errors are preserved, including location metadata.
   * Failure reporting requires no allocation.
   */
  [[nodiscard]] result<owned_block> allocate_block(const storage_callbacks& storage, std::size_t bytes,
                                                   std::size_t alignment) noexcept;

  /**
   * @brief Allocate storage for count elements without size overflow.
   * @par Requirements
   * P003; J5-046; J5-051; J5-101.
   * @return Owning block, resource_limit when count * element_size is unrepresentable,
   * or the allocate_block failure.
   * @note The provider is not called when the size calculation fails.
   */
  [[nodiscard]] result<owned_block> allocate_array(const storage_callbacks& storage, std::size_t count,
                                                   std::size_t element_size, std::size_t alignment) noexcept;
} // namespace adaptive_json5

#endif /* OWNED_BLOCK_HPP_ */
