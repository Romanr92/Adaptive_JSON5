#include "adaptive_json5/memory/buffer_arena.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <memory>

namespace adaptive_json5
{
  /**
   * @brief Take the next aligned part of the buffer that fits.
   * @par Requirements
   * P003; J5-046.
   * @return Aligned storage, or allocation_failure when the rest of the buffer is too small.
   */
  result<void*> buffer_arena::allocate_callback(void* context, std::size_t bytes, std::size_t alignment) noexcept
  {
    auto& self = *static_cast<buffer_arena*>(context);

    void* position = self.m_begin + self.m_used;
    std::size_t space = self.m_capacity - self.m_used;

    // std::align moves position up to the alignment and shrinks space by the padding, or
    // returns nullptr and leaves both unchanged when bytes no longer fit.
    void* aligned = std::align(alignment, bytes, position, space);

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
   * @brief Count one release and reset the arena when none remain.
   * @par Requirements
   * P003; J5-040; J5-051.
   */
  void buffer_arena::deallocate_callback(void* context, void* /* address */, std::size_t /* bytes */,
                                         std::size_t /* alignment */) noexcept
  {
    auto& self = *static_cast<buffer_arena*>(context);

    // A release without a live allocation would break the counting; ignore it.
    if (self.m_live == 0)
    {
      return;
    }

    --self.m_live;

    // Individual blocks are not reused; the whole buffer is, once nothing is live.
    if (self.m_live == 0)
    {
      self.m_used = 0;
    }
  }

  /**
   * @brief Use a caller-supplied byte buffer as allocation space.
   * @par Requirements
   * P003; J5-046; J5-101.
   */
  buffer_arena::buffer_arena(void* buffer, std::size_t size) noexcept
      : m_begin(static_cast<unsigned char*>(buffer)), m_capacity((buffer != nullptr) ? size : 0)
  {
  }

  /**
   * @brief Borrow callbacks that allocate from this arena.
   * @par Requirements
   * P003; J5-046.
   */
  storage_callbacks buffer_arena::callbacks() noexcept
  {
    return {this, nullptr, &deallocate_callback, &allocate_callback};
  }

  /**
   * @brief Report the buffer size this arena allocates from.
   * @par Requirements
   * J5-046.
   */
  const std::size_t buffer_arena::capacity() const noexcept
  {
    return m_capacity;
  }

  /**
   * @brief Report the bytes consumed so far, including alignment padding.
   * @par Requirements
   * J5-046.
   */
  const std::size_t buffer_arena::used() const noexcept
  {
    return m_used;
  }

  /**
   * @brief Report how many allocations have not been released yet.
   * @par Requirements
   * J5-040; J5-046.
   */
  const std::size_t buffer_arena::live_allocations() const noexcept
  {
    return m_live;
  }

} // namespace adaptive_json5
