// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/memory/accounted_storage.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  result<void*> accounted_storage::allocate_callback(void* context, std::size_t bytes, std::size_t alignment) noexcept
  {
    auto& self = *static_cast<accounted_storage*>(context);

    // Every allocation through this adapter must respect the byte ceiling.
    if ((self.m_current_bytes > self.m_limit) || (bytes > (self.m_limit - self.m_current_bytes)))
    {
      return result<void*>::failure(error::without_location(error_code::resource_limit));
    }

    void* address = nullptr;

    if (self.m_provider.allocate_result != nullptr)
    {
      const auto allocated = self.m_provider.allocate_result(self.m_provider.context, bytes, alignment);

      if (!allocated.has_value())
      {
        return result<void*>::failure(*allocated.error_if());
      }

      address = *allocated.value_if();
    }
    else
    {
      address = self.m_provider.allocate(self.m_provider.context, bytes, alignment);
    }

    if (address == nullptr)
    {
      return result<void*>::failure(error::without_location(error_code::allocation_failure));
    }

    // The byte-ceiling check proved this addition cannot overflow.
    self.m_current_bytes += bytes;

    if (self.m_current_bytes > self.m_peak_bytes)
    {
      self.m_peak_bytes = self.m_current_bytes;
    }

    return result<void*>::success(address);
  }

  void accounted_storage::deallocate_callback(void* context, void* address, std::size_t bytes,
                                              std::size_t alignment) noexcept
  {
    auto& self = *static_cast<accounted_storage*>(context);

    self.m_provider.deallocate(self.m_provider.context, address, bytes, alignment);

    // Matching release descriptions keep outstanding-byte accounting exact.
    self.m_current_bytes -= bytes;
  }

  accounted_storage::accounted_storage(const storage_callbacks& provider, std::size_t max_owned_bytes) noexcept
      : m_provider(provider), m_limit(max_owned_bytes)
  {
  }

  storage_callbacks accounted_storage::callbacks() noexcept
  {
    if ((m_provider.deallocate == nullptr) ||
        ((m_provider.allocate == nullptr) && (m_provider.allocate_result == nullptr)))
    {
      return {};
    }

    return {this, nullptr, &deallocate_callback, &allocate_callback};
  }

  result<owned_block> accounted_storage::allocate(std::size_t bytes, std::size_t alignment) noexcept
  {
    return adaptive_json5::allocate_block(callbacks(), bytes, alignment);
  }

  result<owned_block> accounted_storage::allocate_array(std::size_t count, std::size_t element_size,
                                                        std::size_t alignment) noexcept
  {
    const auto bytes = checked_multiply(count, element_size);
    if (!bytes.has_value())
    {
      return result<owned_block>::failure(*bytes.error_if());
    }

    return allocate(*bytes.value_if(), alignment);
  }

  std::size_t accounted_storage::byte_limit() const noexcept
  {
    return m_limit;
  }

  std::size_t accounted_storage::current_bytes() const noexcept
  {
    return m_current_bytes;
  }

  std::size_t accounted_storage::peak_bytes() const noexcept
  {
    return m_peak_bytes;
  }
} // namespace adaptive_json5
