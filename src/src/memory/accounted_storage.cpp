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
  /**
   * @brief Check the byte ceiling, allocate and account for success.
   * @par Requirements
   * P031; P032; J5-046; J5-047.
   * @note Called through allocate_block() with validated alignment
   * and nonzero bytes after provider usability has been checked.
   * Preserves explicit upstream failures without fallback.
   * Failure leaves current and peak accounting unchanged.
   */
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

  /**
   * @brief Release one owned allocation and remove its accounted bytes.
   * @par Requirements
   * P031; J5-040; J5-046; J5-051.
   * @note owned_block supplies the original allocation description once.
   * The originating accounting object and provider must still be alive.
   */
  void accounted_storage::deallocate_callback(void* context, void* address, std::size_t bytes,
                                              std::size_t alignment) noexcept
  {
    auto& self = *static_cast<accounted_storage*>(context);

    self.m_provider.deallocate(self.m_provider.context, address, bytes, alignment);

    // Matching release descriptions keep outstanding-byte accounting exact.
    self.m_current_bytes -= bytes;
  }

  /**
   * @brief Borrow a provider and set its outstanding-byte ceiling.
   * @par Requirements
   * P031; J5-040; J5-047.
   * @param provider Caller storage whose state outlives returned blocks.
   * @param max_owned_bytes Inclusive maximum outstanding requested bytes.
   * @note Zero permits only empty blocks.
   */
  accounted_storage::accounted_storage(const storage_callbacks& provider, std::size_t max_owned_bytes) noexcept
      : m_provider(provider), m_limit(max_owned_bytes)
  {
  }

  /**
   * @brief Borrow result-preserving callbacks for accounted allocation.
   * @par Requirements
   * P032; J5-040; J5-046; J5-047.
   * @return Adapter callbacks, or an unusable empty provider when upstream
   * allocation or deallocation callbacks are missing.
   * @note This object must outlive all allocations through these callbacks.
   * Use allocate_block()/allocate_array() for validated allocation dispatch.
   * Direct callback use must satisfy the storage callback contracts.
   * Deallocation requires the original description exactly once.
   */
  storage_callbacks accounted_storage::callbacks() noexcept
  {
    if ((m_provider.deallocate == nullptr) ||
        ((m_provider.allocate == nullptr) && (m_provider.allocate_result == nullptr)))
    {
      return {};
    }

    return {this, nullptr, &deallocate_callback, &allocate_callback};
  }

  /**
   * @brief Allocate a block through the accounted callback adapter.
   * @par Requirements
   * P031; P032; J5-040; J5-046; J5-047; J5-051.
   * @return Owned block, explicit upstream failure, resource_limit
   * or allocation_failure.
   * @note Provider and alignment validation follow allocate_block().
   * Zero bytes produce an empty block without invoking the provider.
   * Failures leave current and peak accounting unchanged.
   */
  result<owned_block> accounted_storage::allocate(std::size_t bytes, std::size_t alignment) noexcept
  {
    return adaptive_json5::allocate_block(callbacks(), bytes, alignment);
  }

  /**
   * @brief Allocate an element block after checked size multiplication.
   * @par Requirements
   * P031; J5-046; J5-047.
   * @note Overflow and byte-limit failures never call the provider.
   * Raw storage is returned; element lifetimes are managed by the caller.
   */
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

  /**
   * @brief Return the configured outstanding-byte ceiling.
   * @par Requirements
   * P032; J5-047.
   * @note Does not change the ceiling or current accounting.
   */
  std::size_t accounted_storage::byte_limit() const noexcept
  {
    return m_limit;
  }

  /**
   * @brief Return currently outstanding requested allocation bytes.
   * @par Requirements
   * P031; J5-047.
   */
  std::size_t accounted_storage::current_bytes() const noexcept
  {
    return m_current_bytes;
  }

  /**
   * @brief Return the highest outstanding-byte total reached.
   * @par Requirements
   * P031; J5-047.
   * @note Releasing blocks reduces current bytes without reducing the peak.
   */
  std::size_t accounted_storage::peak_bytes() const noexcept
  {
    return m_peak_bytes;
  }
} // namespace adaptive_json5
