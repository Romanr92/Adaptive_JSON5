#ifndef ACCOUNTED_STORAGE_HPP_
#define ACCOUNTED_STORAGE_HPP_

#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Enforce an outstanding-byte ceiling over caller-provided storage.
   * @par Requirements
   * P031; P032; J5-040; J5-046; J5-047; J5-051; issue #125.
   * @note Counts requested allocation bytes, including unused capacity, and outstanding blocks.
   * Provider-internal overhead is excluded.
   * This object and its provider state must outlive every returned allocation.
   * Its address must remain stable while any returned allocation exists.
   * Operations require external synchronization if shared between threads.
   * Provider callbacks must not reenter this accounting object.
   * A release that matches no outstanding block is refused and latches faulted(); after
   * that every allocation fails with internal_error. Release blocks only through
   * owned_block, never through callbacks() directly, and check faulted() after releasing
   * a document (A-09).
   * Construction performs no allocation.
   */
  class [[nodiscard]] accounted_storage
  {
    private:
      storage_callbacks m_provider;
      std::size_t m_limit;
      std::size_t m_current_bytes{};
      std::size_t m_peak_bytes{};
      std::size_t m_outstanding_blocks{};
      bool m_faulted{false};

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
      [[nodiscard]] static result<void*> allocate_callback(void* context, std::size_t bytes,
                                                           std::size_t alignment) noexcept;

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
      static void deallocate_callback(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept;

    public:
      /**
       * @brief Borrow a provider and set its outstanding-byte ceiling.
       * @par Requirements
       * P031; J5-040; J5-047.
       * @param provider Caller storage whose state outlives returned blocks.
       * @param max_owned_bytes Inclusive maximum outstanding requested bytes.
       * @note Zero permits only empty blocks.
       * @par ID SWU-MEMORY-003
       */
      accounted_storage(const storage_callbacks& provider, std::size_t max_owned_bytes) noexcept;

      /**
       * @brief End accounting after every returned allocation has been released.
       * @par Requirements
       * P031; J5-040.
       * @note Does not own or release outstanding blocks itself.
       */
      ~accounted_storage() noexcept = default;

      /**
       * @brief Copying is not allowed.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note Returned blocks keep this object's address as their callback context; a copy
       * would account for blocks it never allocated.
       */
      accounted_storage(const accounted_storage&) = delete;

      /**
       * @brief Copy assignment is not allowed.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note Same reason as the copy constructor.
       */
      accounted_storage& operator=(const accounted_storage&) = delete;

      /**
       * @brief Moving is not allowed.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note Returned blocks keep this object's address, so it must stay where it is until
       * every block is released.
       */
      accounted_storage(accounted_storage&&) = delete;

      /**
       * @brief Move assignment is not allowed.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note Same reason as the move constructor.
       */
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
       * Deallocation requires the original description exactly once; a release that matches
       * no outstanding block is refused and latches faulted().
       * @par ID SWU-MEMORY-004
       */
      [[nodiscard]] storage_callbacks callbacks() noexcept;

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
      [[nodiscard]] result<owned_block> allocate(std::size_t bytes, std::size_t alignment) noexcept;

      /**
       * @brief Allocate an element block after checked size multiplication.
       * @par Requirements
       * P031; J5-046; J5-047; issue #125.
       * @note Overflow, byte-limit and fault failures never call the provider.
       * Raw storage is returned; element lifetimes are managed by the caller.
       * @par ID SWU-MEMORY-006
       */
      [[nodiscard]] result<owned_block> allocate_array(std::size_t count, std::size_t element_size,
                                                       std::size_t alignment) noexcept;

      /**
       * @brief Return the configured outstanding-byte ceiling.
       * @par Requirements
       * P032; J5-047.
       * @note Does not change the ceiling or current accounting.
       * @par ID SWU-MEMORY-007
       */
      [[nodiscard]] std::size_t byte_limit() const noexcept;

      /**
       * @brief Return currently outstanding requested allocation bytes.
       * @par Requirements
       * P031; J5-047.
       * @par ID SWU-MEMORY-008
       */
      [[nodiscard]] std::size_t current_bytes() const noexcept;

      /**
       * @brief Return the highest outstanding-byte total reached.
       * @par Requirements
       * P031; J5-047.
       * @note Releasing blocks reduces current bytes without reducing the peak.
       * @par ID SWU-MEMORY-009
       */
      [[nodiscard]] std::size_t peak_bytes() const noexcept;

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
      [[nodiscard]] bool faulted() const noexcept;
  };
} // namespace adaptive_json5

#endif /* ACCOUNTED_STORAGE_HPP_ */
