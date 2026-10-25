#include "parser/parser_stack.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "common/checked_size.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_frame.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Address raw storage for one frame.
     * @par Requirements
     * P033; J5-046.
     * @note Private callers supply an index below allocated capacity.
     * No frame lifetime is assumed at this address.
     */
    void* parser_stack::slot_address(const owned_block& storage, std::size_t index) noexcept
    {
      auto* bytes = static_cast<unsigned char*>(storage.data());
      return bytes + (index * sizeof(parser_frame));
    }

    /**
     * @brief Access an already constructed frame.
     * @par Requirements
     * P033; J5-040.
     * @note Private callers supply an index below m_size.
     */
    parser_frame* parser_stack::live_frame(std::size_t index) noexcept
    {
      return std::launder(static_cast<parser_frame*>(slot_address(m_storage, index)));
    }

    /**
     * @brief Access an already constructed frame without mutation.
     * @par Requirements
     * P033; J5-040.
     * @note Private callers supply an index below m_size.
     */
    const parser_frame* parser_stack::live_frame(std::size_t index) const noexcept
    {
      return std::launder(static_cast<const parser_frame*>(slot_address(m_storage, index)));
    }

    /**
     * @brief Ensure storage for the requested number of frames.
     * @par Requirements
     * P033; J5-046; J5-048; J5-049; J5-051.
     * @note Failure preserves existing frames.
     * Growth costs one work unit per relocated frame.
     * Old and new allocations coexist during relocation.
     */
    status_result parser_stack::reserve(std::size_t required, work_budget& work) noexcept
    {
      if (required > m_max_frames)
      {
        return status_result::failure(error::without_location(error_code::resource_limit));
      }

      if (required <= m_capacity)
      {
        return status_result::success();
      }

      std::size_t capacity = required;
      const auto doubled = checked_add(m_capacity, m_capacity);

      // Doubling is optional; overflow can fall back to the required size.
      if (doubled.has_value())
      {
        const std::size_t bounded = (*doubled.value_if() > m_max_frames) ? m_max_frames : *doubled.value_if();

        if (bounded > capacity)
        {
          capacity = bounded;
        }
      }

      // Reserve relocation work before allocating or moving any frames.
      const auto charged = work.charge(m_size);

      if (!charged.has_value())
      {
        return status_result::failure(*charged.error_if());
      }

      auto allocation = allocate_array(m_provider, capacity, sizeof(parser_frame), alignof(parser_frame));

      if (!allocation.has_value())
      {
        return status_result::failure(*allocation.error_if());
      }

      for (std::size_t index{}; index < m_size; ++index)
      {
        // Construct new objects so their owned names transfer safely.
        ::new (slot_address(*allocation.value_if(), index)) parser_frame(std::move(*live_frame(index)));
      }

      const std::size_t retained_size = m_size;

      //  Non-throwing moves make it safe to destroy the old objects now.
      clear();
      m_storage = std::move(*allocation.value_if());
      m_size = retained_size;
      m_capacity = capacity;

      return status_result::success();
    }

    /**
     * @brief Construct an empty stack with an explicit frame limit.
     * @par Requirements
     * P033; J5-046; J5-048.
     * @note Construction allocates nothing. Zero permits no frames.
     */
    parser_stack::parser_stack(const storage_callbacks& provider, std::size_t max_frames) noexcept
        : m_provider(provider), m_max_frames(max_frames)
    {
    }

    /**
     * @brief Destroy live frames before releasing their raw storage.
     * @par Requirements
     * P033; J5-040; J5-048.
     * @note Cleanup is iterative and does not charge work.
     */
    parser_stack::~parser_stack() noexcept
    {
      clear();
    }

    /**
     * @brief Push an externally held frame after checking limits and storage.
     * @par Requirements
     * P033; J5-046; J5-048; J5-049; J5-051.
     * @note frame must not reside inside this stack.
     * Failure preserves frames and the incoming frame.
     * Each push attempt passing the frame limit costs one work unit.
     * Successful work charges are not refunded after later failure.
     */
    status_result parser_stack::push(parser_frame&& frame, work_budget& work) noexcept
    {
      if (m_size >= m_max_frames)
      {
        return status_result::failure(error::without_location(error_code::resource_limit));
      }

      const auto charged = work.charge(1);

      if (!charged.has_value())
      {
        return status_result::failure(*charged.error_if());
      }

      // The frame-limit check proves adding one cannot overflow.
      const std::size_t required = m_size + 1;
      const auto reserved = reserve(required, work);

      if (!reserved.has_value())
      {
        return status_result::failure(*reserved.error_if());
      }

      ::new (slot_address(m_storage, m_size)) parser_frame(std::move(frame));
      m_size = required;

      return status_result::success();
    }

    /**
     * @brief Inspect the innermost open container.
     * @par Requirements
     * P033; J5-013.
     * @return Pointer to the top frame, or nullptr when empty.
     * @note Growth, removal or destruction can invalidate the pointer.
     */
    parser_frame* parser_stack::top() noexcept
    {
      return (m_size == 0) ? nullptr : live_frame(m_size - 1);
    }

    /**
     * @brief Inspect the innermost container without mutation.
     * @par Requirements
     * P033; J5-013.
     * @return Pointer to the top frame, or nullptr when empty.
     */
    const parser_frame* parser_stack::top() const noexcept
    {
      return (m_size == 0) ? nullptr : live_frame(m_size - 1);
    }

    /**
     * @brief Remove the innermost frame if present.
     * @par Requirements
     * P033; J5-040; J5-048.
     * @return True when a frame was removed.
     * @note Performs no allocation or work charge.
     */
    bool parser_stack::pop() noexcept
    {
      if (m_size == 0)
      {
        return false;
      }

      --m_size;
      live_frame(m_size)->~parser_frame();
      return true;
    }

    /**
     * @brief Destroy all frames while retaining reusable raw storage.
     * @par Requirements
     * P033; J5-040; J5-048.
     * @note Also works after work-budget exhaustion.
     */
    void parser_stack::clear() noexcept
    {
      while (m_size != 0)
      {
        pop();
      }
    }

    /**
     * @brief Return the number of live frames.
     * @par Requirements
     * P033; J5-048.
     */
    std::size_t parser_stack::size() const noexcept
    {
      return m_size;
    }
  } // namespace detail
} // namespace adaptive_json5
