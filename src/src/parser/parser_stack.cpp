// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/parser/parser_stack.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    void* parser_stack::slot_address(const owned_block& storage, std::size_t index) noexcept
    {
      auto* bytes = static_cast<unsigned char*>(storage.data());
      return bytes + (index * sizeof(parser_frame));
    }

    parser_frame* parser_stack::live_frame(std::size_t index) noexcept
    {
      return std::launder(static_cast<parser_frame*>(slot_address(m_storage, index)));
    }

    const parser_frame* parser_stack::live_frame(std::size_t index) const noexcept
    {
      return std::launder(static_cast<const parser_frame*>(slot_address(m_storage, index)));
    }

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

    parser_stack::parser_stack(const storage_callbacks& provider, std::size_t max_frames) noexcept
        : m_provider(provider), m_max_frames(max_frames)
    {
    }

    parser_stack::~parser_stack() noexcept
    {
      clear();
    }

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

    parser_frame* parser_stack::top() noexcept
    {
      return (m_size == 0) ? nullptr : live_frame(m_size - 1);
    }

    const parser_frame* parser_stack::top() const noexcept
    {
      return (m_size == 0) ? nullptr : live_frame(m_size - 1);
    }

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

    void parser_stack::clear() noexcept
    {
      while (m_size != 0)
      {
        pop();
      }
    }

    std::size_t parser_stack::size() const noexcept
    {
      return m_size;
    }
  } // namespace detail
} // namespace adaptive_json5
