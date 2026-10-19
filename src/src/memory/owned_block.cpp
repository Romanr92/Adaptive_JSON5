// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/memory/owned_block.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  owned_block::owned_block(const storage_callbacks& storage, void* data, std::size_t bytes,
                           std::size_t alignment) noexcept
      : m_storage(storage), m_data(data), m_bytes(bytes), m_alignment(alignment)
  {
  }

  owned_block::owned_block(owned_block&& other) noexcept
      : m_storage(other.m_storage), m_data(other.m_data), m_bytes(other.m_bytes), m_alignment(other.m_alignment)
  {
    // Clearing the source prevents a double release when it is destroyed.
    other.m_data = nullptr;
    other.m_bytes = 0;
    other.m_alignment = 0;
  }

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

  void* owned_block::data() const noexcept
  {
    return m_data;
  }

  std::size_t owned_block::size() const noexcept
  {
    return m_bytes;
  }

  std::size_t owned_block::alignment() const noexcept
  {
    return m_alignment;
  }

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
