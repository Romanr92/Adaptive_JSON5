#ifndef BUFFER_ARENA_HPP_
#define BUFFER_ARENA_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Provide fallible storage from one caller-supplied byte buffer.
   * @par Requirements
   * P003; J5-046; J5-051; J5-101.
   * @note Allocates by advancing through the buffer and uses no heap or global state.
   * Released memory is reused only once every allocation has been released, which
   * resets the arena to the start of the buffer.
   * A request that does not fit reports allocation_failure and changes nothing.
   * The buffer and this object must outlive every allocation. This object's address is
   * the callbacks' context, so it must not move while allocations exist.
   * Combine with accounted_storage to enforce max_owned_bytes.
   * Not synchronized: use one arena per thread or synchronize externally.
   */
  class [[nodiscard]] buffer_arena
  {
    private:
      unsigned char* m_begin;
      std::size_t m_capacity;
      std::size_t m_used{};
      std::size_t m_live{};

      /**
       * @brief Take the next aligned part of the buffer that fits.
       * @par Requirements
       * P003; J5-046.
       * @return Aligned storage, or allocation_failure when the rest of the buffer is too small.
       * @note Called through allocate_block() with nonzero bytes and a validated power-of-two
       * alignment. Failure changes nothing.
       */
      [[nodiscard]] static result<void*> allocate_callback(void* context, std::size_t bytes,
                                                           std::size_t alignment) noexcept;

      /**
       * @brief Count one release and reset the arena when none remain.
       * @par Requirements
       * P003; J5-040; J5-051.
       * @note The memory itself is reused only after the last live allocation is released.
       * A release without a live allocation is ignored.
       */
      static void deallocate_callback(void* context, void* /* address */, std::size_t /* bytes */,
                                      std::size_t /* alignment */) noexcept;

    public:
      /**
       * @brief Use a caller-supplied byte buffer as allocation space.
       * @par Requirements
       * P003; J5-046; J5-101.
       * @param buffer Start of the caller's buffer; it must outlive every allocation.
       * @param size Usable bytes in buffer.
       * @note A null buffer gives an arena with no capacity. Performs no allocation.
       */
      buffer_arena(void* buffer, std::size_t size) noexcept;

      /**
       * @brief End the arena.
       * @par Requirements
       * J5-040.
       * @note Every allocation must already be released; the buffer itself belongs to the caller.
       */
      ~buffer_arena() noexcept = default;

      /**
       * @brief Copying is not allowed.
       * @par Requirements
       * J5-046.
       * @note Two arenas over one buffer would hand out the same memory twice.
       */
      buffer_arena(const buffer_arena&) = delete;

      /**
       * @brief Copy assignment is not allowed.
       * @par Requirements
       * J5-046.
       * @note Same reason as the copy constructor.
       */
      buffer_arena& operator=(const buffer_arena&) = delete;

      /**
       * @brief Moving is not allowed.
       * @par Requirements
       * J5-040; J5-046.
       * @note Allocations keep this object's address as their callback context, so it must
       * stay where it is until every allocation is released.
       */
      buffer_arena(buffer_arena&&) = delete;

      /**
       * @brief Move assignment is not allowed.
       * @par Requirements
       * J5-040; J5-046.
       * @note Same reason as the move constructor.
       */
      buffer_arena& operator=(buffer_arena&&) = delete;

      /**
       * @brief Borrow callbacks that allocate from this arena.
       * @par Requirements
       * P003; J5-046.
       * @return Callbacks whose context is this object.
       * @note Pass them to accounted_storage, or use them directly with allocate_block().
       */
      [[nodiscard]] storage_callbacks callbacks() noexcept;

      /**
       * @brief Report the buffer size this arena allocates from.
       * @par Requirements
       * J5-046.
       */
      [[nodiscard]] std::size_t capacity() const noexcept;

      /**
       * @brief Report the bytes consumed so far, including alignment padding.
       * @par Requirements
       * J5-046.
       * @note Returns to zero when the last live allocation is released.
       */
      [[nodiscard]] std::size_t used() const noexcept;

      /**
       * @brief Report how many allocations have not been released yet.
       * @par Requirements
       * J5-040; J5-046.
       */
      [[nodiscard]] std::size_t live_allocations() const noexcept;
  };
} // namespace adaptive_json5

#endif /* BUFFER_ARENA_HPP_ */
