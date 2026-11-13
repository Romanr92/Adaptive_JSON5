#include "adaptive_json5/memory/accounted_storage.hpp"

#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "common/checked_size.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Check the byte ceiling, allocate and account for success.
   * @par Requirements
   * P031; P032; J5-046; J5-047; issue #125.
   * @note Called through allocate_block() with validated alignment
   * and nonzero bytes after provider usability has been checked.
   * Fails with internal_error once faulted(), with wrong_type_or_access (invalid_argument)
   * for zero bytes and with resource_limit (owned_bytes) above the ceiling; none of these
   * call the provider. Preserves explicit upstream failures without fallback.
   * Failure leaves current and peak accounting unchanged.
   * Success counts the bytes and one outstanding block.
   * @par ID SWU-MEMORY-001
   */
  result<void*> accounted_storage::allocate_callback(void* context, std::size_t bytes, std::size_t alignment) noexcept
  {
    auto& self = *static_cast<accounted_storage*>(context);

    // A refused release left the accounting unreliable, so nothing is allocated any more.
    if (self.m_faulted)
    {
      return result<void*>::failure(error::internal_fault());
    }

    // allocate_block() never passes zero bytes; every counted block has at least one byte.
    if (bytes == 0U)
    {
      return result<void*>::failure(error::access_failure(access_error_reason::invalid_argument));
    }

    // Every allocation through this adapter must respect the byte ceiling.
    if ((self.m_current_bytes > self.m_limit) || (bytes > (self.m_limit - self.m_current_bytes)))
    {
      return result<void*>::failure(error::limit_without_location(resource_limit_reason::owned_bytes));
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
    ++self.m_outstanding_blocks;

    if (self.m_current_bytes > self.m_peak_bytes)
    {
      self.m_peak_bytes = self.m_current_bytes;
    }

    return result<void*>::success(address);
  }

  /**
   * @brief Release one owned allocation and remove its accounted bytes.
   * @par Requirements
   * P031; J5-040; J5-046; J5-047; J5-051; issue #125.
   * @note owned_block supplies the original allocation description once.
   * The originating accounting object and provider must still be alive.
   * A release that cannot match an outstanding block is refused: a null address, zero
   * bytes, no block outstanding, more bytes than the other blocks leave room for, or a
   * last block whose size differs from the remaining bytes. A refused release does not
   * reach the provider, leaves the counters unchanged and latches faulted() (ME-03).
   * A too-small release or a double release while two or more blocks are outstanding
   * cannot be detected here; releasing only through owned_block prevents it (A-09).
   * @par ID SWU-MEMORY-002
   */
  void accounted_storage::deallocate_callback(void* context, void* address, std::size_t bytes,
                                              std::size_t alignment) noexcept
  {
    auto& self = *static_cast<accounted_storage*>(context);

    // Every outstanding block has at least one byte, so the others still need blocks - 1 bytes.
    const bool no_block = (self.m_outstanding_blocks == 0U);
    const bool too_large = no_block || (bytes > (self.m_current_bytes - (self.m_outstanding_blocks - 1U)));
    const bool last_mismatch = (self.m_outstanding_blocks == 1U) && (bytes != self.m_current_bytes);

    // A release that matches no outstanding allocation is refused before the provider sees it,
    // so it can neither corrupt the accounting nor double-free (ME-03).
    if ((address == nullptr) || (bytes == 0U) || too_large || last_mismatch)
    {
      self.m_faulted = true;
      return;
    }

    self.m_provider.deallocate(self.m_provider.context, address, bytes, alignment);
    self.m_current_bytes -= bytes;
    --self.m_outstanding_blocks;
  }

  /**
   * @brief Borrow a provider and set its outstanding-byte ceiling.
   * @par Requirements
   * P031; J5-040; J5-047.
   * @param provider Caller storage whose state outlives returned blocks.
   * @param max_owned_bytes Inclusive maximum outstanding requested bytes.
   * @note Zero permits only empty blocks.
   * @par ID SWU-MEMORY-003
   */
  accounted_storage::accounted_storage(const storage_callbacks& provider, std::size_t max_owned_bytes) noexcept
      : m_provider(provider), m_limit(max_owned_bytes)
  {
  }

  /**
   * @brief Report whether a refused release has stopped all further allocation.
   * @par Requirements
   * J5-046; J5-047; issue #125.
   * @return True after a release that matched no outstanding block (ME-03).
   * @note The latch is permanent for this object. While it is set, every allocation fails
   * with internal_error; valid releases still reach the provider, so real blocks are freed.
   * Check it after releasing a document: release has no return value, so this query is the
   * only way to see a refused release when no allocation follows (A-09).
   * @par ID SWU-MEMORY-076
   */
  bool accounted_storage::faulted() const noexcept
  {
    return m_faulted;
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
   * Deallocation requires the original description exactly once; a release that matches
   * no outstanding block is refused and latches faulted().
   * @par ID SWU-MEMORY-004
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
   * P031; P032; J5-040; J5-046; J5-047; J5-051; issue #125.
   * @return Owned block, explicit upstream failure, resource_limit (owned_bytes),
   * allocation_failure, or internal_error once faulted().
   * @note Provider and alignment validation follow allocate_block().
   * Zero bytes produce an empty block without invoking the provider.
   * Failures leave current and peak accounting unchanged.
   * @par ID SWU-MEMORY-005
   */
  result<owned_block> accounted_storage::allocate(std::size_t bytes, std::size_t alignment) noexcept
  {
    return adaptive_json5::allocate_block(callbacks(), bytes, alignment);
  }

  /**
   * @brief Allocate an element block after checked size multiplication.
   * @par Requirements
   * P031; J5-046; J5-047; issue #125.
   * @note Overflow, byte-limit and fault failures never call the provider.
   * Raw storage is returned; element lifetimes are managed by the caller.
   * @par ID SWU-MEMORY-006
   */
  result<owned_block> accounted_storage::allocate_array(std::size_t count, std::size_t element_size,
                                                        std::size_t alignment) noexcept
  {
    const auto bytes = checked_multiply(count, element_size, resource_limit_reason::owned_bytes);
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
   * @par ID SWU-MEMORY-007
   */
  std::size_t accounted_storage::byte_limit() const noexcept
  {
    return m_limit;
  }

  /**
   * @brief Return currently outstanding requested allocation bytes.
   * @par Requirements
   * P031; J5-047.
   * @par ID SWU-MEMORY-008
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
   * @par ID SWU-MEMORY-009
   */
  std::size_t accounted_storage::peak_bytes() const noexcept
  {
    return m_peak_bytes;
  }
} // namespace adaptive_json5
