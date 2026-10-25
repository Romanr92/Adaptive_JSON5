#include "adaptive_json5/memory/owned_block.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "common/checked_size.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Adopt one successfully allocated block.
   * @par Requirements
   * P003; J5-040; J5-046.
   * @note The allocation description must match the originating provider.
   */
  owned_block::owned_block(const storage_callbacks& storage, void* data, std::size_t bytes,
                           std::size_t alignment) noexcept
      : m_storage(storage), m_data(data), m_bytes(bytes), m_alignment(alignment)
  {
  }

  /**
   * @brief Transfer ownership and leave the source empty.
   * @par Requirements
   * P003; J5-040; J5-051.
   */
  owned_block::owned_block(owned_block&& other) noexcept
      : m_storage(other.m_storage), m_data(other.m_data), m_bytes(other.m_bytes), m_alignment(other.m_alignment)
  {
    // Clearing the source prevents a double release when it is destroyed.
    other.m_data = nullptr;
    other.m_bytes = 0;
    other.m_alignment = 0;
  }

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
  owned_block& owned_block::operator=(owned_block&& other) noexcept
  {
    // Self-move must not release the allocation we intend to keep.
    if (this == &other)
    {
      return *this;
    }

    // Release through the old provider before adopting the new one.
    if (m_data != nullptr)
    {
      m_storage.deallocate(m_storage.context, m_data, m_bytes, m_alignment);
    }

    m_storage = other.m_storage;
    m_data = other.m_data;
    m_bytes = other.m_bytes;
    m_alignment = other.m_alignment;

    // Only the destination may release the transferred allocation.
    other.m_data = nullptr;
    other.m_bytes = 0;
    other.m_alignment = 0;

    return *this;
  }

  /**
   * @brief Access the block's first byte.
   * @par Requirements
   * P003; J5-040.
   * @return Block address, or nullptr when empty.
   * @note The pointer remains dependent on this owner's lifetime.
   */
  void* owned_block::data() const noexcept
  {
    return m_data;
  }

  /**
   * @brief Return the owned allocation size in bytes.
   * @par Requirements
   * P003; J5-040.
   */
  std::size_t owned_block::size() const noexcept
  {
    return m_bytes;
  }

  /**
   * @brief Return the owned allocation's requested alignment.
   * @par Requirements
   * P003; J5-040.
   */
  std::size_t owned_block::alignment() const noexcept
  {
    return m_alignment;
  }

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
  result<owned_block> allocate_block(const storage_callbacks& storage, std::size_t bytes,
                                     std::size_t alignment) noexcept
  {
    // Reject unusable providers before any callback can be invoked.
    if ((storage.deallocate == nullptr) || ((storage.allocate == nullptr) && (storage.allocate_result == nullptr)))
    {
      return result<owned_block>::failure(error::without_location(error_code::allocation_failure));
    }

    // A power of two has exactly one set bit, so clearing the lowest bit gives zero.
    if ((alignment == 0) || ((alignment & (alignment - 1)) != 0))
    {
      return result<owned_block>::failure(error::without_location(error_code::resource_limit));
    }

    if (bytes == 0)
    {
      return result<owned_block>::success(owned_block{});
    }

    void* data = nullptr;

    if (storage.allocate_result != nullptr)
    {
      const auto allocated = storage.allocate_result(storage.context, bytes, alignment);

      if (!allocated.has_value())
      {
        return result<owned_block>::failure(*allocated.error_if());
      }

      data = *allocated.value_if();
    }
    else
    {
      data = storage.allocate(storage.context, bytes, alignment);
    }

    // Neither callback may create a nonempty owner around a null address.
    if (data == nullptr)
    {
      return result<owned_block>::failure(error::without_location(error_code::allocation_failure));
    }

    return result<owned_block>::success(owned_block{storage, data, bytes, alignment});
  }

  /**
   * @brief Allocate storage for count elements without size overflow.
   * @par Requirements
   * P003; J5-046; J5-051; J5-101.
   * @return Owning block, resource_limit when count * element_size is unrepresentable,
   * or the allocate_block failure.
   * @note The provider is not called when the size calculation fails.
   */
  result<owned_block> allocate_array(const storage_callbacks& storage, std::size_t count, std::size_t element_size,
                                     std::size_t alignment) noexcept
  {
    const auto bytes = checked_multiply(count, element_size);
    if (!bytes.has_value())
    {
      return result<owned_block>::failure(*bytes.error_if());
    }

    return allocate_block(storage, *bytes.value_if(), alignment);
  }
} // namespace adaptive_json5
