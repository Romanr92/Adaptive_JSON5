#include "adaptive_json5/memory/buffer_arena.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "memory/pointer_alignment.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Take the next aligned part of the buffer that fits.
   * @par Requirements
   * P003; J5-046.
   * @return Aligned storage, or allocation_failure when the rest of the buffer is too small.
   * @par ID SWU-MEMORY-010
   */
  result<void*> buffer_arena::allocate_callback(void* context, std::size_t bytes, std::size_t alignment) noexcept
  {
    // MISRA justification: The allocation callback receives the live buffer_arena instance that callbacks()
    // registered as its context, so it can allocate from that arena.
    // codeql::misra_deviation_next_line(DEV-002.03)
    auto& self = *static_cast<buffer_arena*>(context);

    void* position = self.m_begin + self.m_used;
    std::size_t space = self.m_capacity - self.m_used;

    // detail::align_with moves position up to the alignment and shrinks space by the padding, or
    // returns nullptr and leaves both unchanged when bytes no longer fit.
    void* aligned = detail::align_within(alignment, bytes, position, space);

    if (aligned == nullptr)
    {
      return result<void*>::failure(error::without_location(error_code::allocation_failure));
    }

    // Everything before the remaining space, plus this block, is now in use.
    self.m_used = (self.m_capacity - space) + bytes;
    ++self.m_live;

    return result<void*>::success(aligned);
  }

  /**
   * @brief Count one release, return the most recent block and reset the arena when none remain.
   * @par Requirements
   * P003; J5-040; J5-051.
   * @par ID SWU-MEMORY-011
   */
  void buffer_arena::deallocate_callback(void* context, void* address, std::size_t bytes,
                                         std::size_t /* alignment */) noexcept
  {
    // MISRA justification: The release callback receives the live buffer_arena instance that callbacks()
    // registered as its context, so it can apply that arena's release rules.
    // codeql::misra_deviation_next_line(DEV-002.04)
    auto& self = *static_cast<buffer_arena*>(context);

    // A release without a live allocation would break the counting; ignore it.
    if (self.m_live == 0)
    {
      return;
    }

    --self.m_live;

    // The whole buffer is reused once nothing is live.
    if (self.m_live == 0)
    {
      self.m_used = 0;
      return;
    }

    // MISRA justification: Top of stack release: the block ends exactly at the used mark, so give it back.
    // The release callback accepts a block from this arena. Use its byte address
    // to compare the block's end to the arena's used mark without reading the payload.
    // codeql::misra_deviation_next_line(DEV-002.05)
    auto* const block = static_cast<std::byte*>(address);
    if ((block != nullptr) && (block >= self.m_begin) &&
        ((static_cast<std::size_t>(block - self.m_begin) + bytes) == self.m_used))
    {
      self.m_used = static_cast<std::size_t>(block - self.m_begin);
    }
  }

  /**
   * @brief Use a caller-supplied byte buffer as allocation space.
   * @par Requirements
   * P003; J5-046; J5-101.
   * @par ID SWU-MEMORY-012
   */
  buffer_arena::buffer_arena(void* buffer, std::size_t size) noexcept
      // MISRA justification: Keep the caller's buffer as a byte view so arena offsets use byte units.
      // The capacity records the supplied extent and is zero for a null buffer.
      // codeql::misra_deviation_next_line(DEV-002.06)
      : m_begin(static_cast<std::byte*>(buffer)), m_capacity((buffer != nullptr) ? size : 0)
  {
  }

  /**
   * @brief Borrow callbacks that allocate from this arena.
   * @par Requirements
   * P003; J5-046.
   * @par ID SWU-MEMORY-013
   */
  storage_callbacks buffer_arena::callbacks() noexcept
  {
    return {this, &allocate_callback, &deallocate_callback};
  }

  /**
   * @brief Report the buffer size this arena allocates from.
   * @par Requirements
   * J5-046.
   * @par ID SWU-MEMORY-014
   */
  std::size_t buffer_arena::capacity() const noexcept
  {
    return m_capacity;
  }

  /**
   * @brief Report the bytes consumed so far, including alignment padding.
   * @par Requirements
   * J5-046.
   * @par ID SWU-MEMORY-015
   */
  std::size_t buffer_arena::used() const noexcept
  {
    return m_used;
  }

  /**
   * @brief Report how many allocations have not been released yet.
   * @par Requirements
   * J5-040; J5-046.
   * @par ID SWU-MEMORY-016
   */
  std::size_t buffer_arena::live_allocations() const noexcept
  {
    return m_live;
  }

} // namespace adaptive_json5
