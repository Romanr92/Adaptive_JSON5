#ifndef ACCOUNTED_STORAGE_HPP_
#define ACCOUNTED_STORAGE_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Enforce an outstanding-byte ceiling over caller-provided storage.
   * @par Requirements
   * P031; P032; J5-040; J5-046; J5-047; J5-051.
   * @note Counts requested allocation bytes, including unused capacity.
   * Provider-internal overhead is excluded.
   * This object and its provider state must outlive every returned allocation.
   * Its address must remain stable while any returned allocation exists.
   * Operations require external synchronization if shared between threads.
   * Provider callbacks must not reenter this accounting object.
   * Construction performs no allocation.
   */
  class accounted_storage
  {
    private:
      storage_callbacks m_provider;
      std::size_t m_limit;
      std::size_t m_current_bytes{};
      std::size_t m_peak_bytes{};

      /**
       * @brief Check the byte ceiling, allocate and account for success.
       * @par Requirements
       * P031; P032; J5-046; J5-047.
       * @note Called through allocate_block() with validated alignment
       * and nonzero bytes after provider usability has been checked.
       * Preserves explicit upstream failures without fallback.
       * Failure leaves current and peak accounting unchanged.
       */
      static result<void*> allocate_callback(void* context, std::size_t bytes, std::size_t alignment) noexcept;

      /**
       * @brief Release one owned allocation and remove its accounted bytes.
       * @par Requirements
       * P031; J5-040; J5-046; J5-051.
       * @note owned_block supplies the original allocation description once.
       * The originating accounting object and provider must still be alive.
       */
      static void deallocate_callback(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept;

    public:
      /**
       * @brief Borrow a provider and set its outstanding-byte ceiling.
       * @par Requirements
       * P031; J5-040; J5-047.
       * @param provider Caller storage whose state outlives returned blocks.
       * @param max_owned_bytes Inclusive maximum outstanding requested bytes.
       * @note Zero permits only empty blocks.
       */
      accounted_storage(const storage_callbacks& provider, std::size_t max_owned_bytes) noexcept;

      /**
       * @brief End accounting after every returned allocation has been released.
       * @par Requirements
       * P031; J5-040.
       * @note Does not own or release outstanding blocks itself.
       */
      ~accounted_storage() noexcept = default;

      // Returned blocks retain this object's address as their callback context.
      accounted_storage(const accounted_storage&) = delete;
      accounted_storage& operator=(const accounted_storage&) = delete;
      accounted_storage(accounted_storage&&) = delete;
      accounted_storage& operator=(accounted_storage&&) = delete;

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
      storage_callbacks callbacks() noexcept;

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
      result<owned_block> allocate(std::size_t bytes, std::size_t alignment) noexcept;

      /**
       * @brief Allocate an element block after checked size multiplication.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note Overflow and byte-limit failures never call the provider.
       * Raw storage is returned; element lifetimes are managed by the caller.
       */
      result<owned_block> allocate_array(std::size_t count, std::size_t element_size, std::size_t alignment) noexcept;

      /**
       * @brief Return the configured outstanding-byte ceiling.
       * @par Requirements
       * P032; J5-047.
       * @note Does not change the ceiling or current accounting.
       */
      std::size_t byte_limit() const noexcept;

      /**
       * @brief Return currently outstanding requested allocation bytes.
       * @par Requirements
       * P031; J5-047.
       */
      std::size_t current_bytes() const noexcept;

      /**
       * @brief Return the highest outstanding-byte total reached.
       * @par Requirements
       * P031; J5-047.
       * @note Releasing blocks reduces current bytes without reducing the peak.
       */
      std::size_t peak_bytes() const noexcept;
  };
} // namespace adaptive_json5

#endif /* ACCOUNTED_STORAGE_HPP_ */
