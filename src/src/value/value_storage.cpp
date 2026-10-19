// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/value/value_record.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    static_assert(std::is_nothrow_move_constructible_v<value_record>);
    static_assert(std::is_nothrow_destructible_v<value_record>);
    void* value_storage::slot_address(const owned_block& storage, std::size_t index) noexcept
    {
      auto* bytes = static_cast<unsigned char*>(storage.data());
      return bytes + index * sizeof(value_record);
    }

    value_record* value_storage::live_record(std::size_t index) noexcept
    {
      return std::launder(static_cast<value_record*>(slot_address(m_storage, index)));
    }

    const value_record* value_storage::live_record(std::size_t index) const noexcept
    {
      return std::launder(static_cast<const value_record*>(slot_address(m_storage, index)));
    }

    void value_storage::destroy_records() noexcept
    {
      while (m_size != c_empty_size)
      {
        --m_size;
        live_record(m_size)->~value_record();
      }
    }

    status_result value_storage::reserve(std::size_t required) noexcept
    {
      if (required > m_max_records)
      {
        return status_result::failure(error::without_location(error_code::resource_limit));
      }

      if (required <= m_capacity)
      {
        return status_result::success();
      }

      std::size_t capacity = required;
      const auto doubled = checked_add(m_capacity, m_capacity);

      // Doubling is optional; an overflowing growth estimate can fall back to required.
      if (doubled.has_value())
      {
        const std::size_t bounded = (*doubled.value_if() > m_max_records) ? m_max_records : *doubled.value_if();

        if (bounded > capacity)
        {
          capacity = bounded;
        }
      }

      auto allocation = allocate_array(m_provider, capacity, sizeof(value_record), alignof(value_record));
      if (!allocation.has_value())
      {
        return status_result::failure(*allocation.error_if());
      }

      for (std::size_t index{}; index < m_size; ++index)
      {
        // Start each new object's lifetime by transferring it's owned payload.
        ::new (slot_address(*allocation.value_if(), index)) value_record(std::move(*live_record(index)));
      }

      const std::size_t retained_size = m_size;

      // All moves are non-throwing, so old object can now be destroyed safely.
      destroy_records();
      m_storage = std::move(*allocation.value_if());
      m_size = retained_size;
      m_capacity = capacity;

      return status_result::success();
    }

    value_storage::value_storage(const storage_callbacks& provider, std::size_t max_records) noexcept
        : m_provider(provider), m_max_records(max_records)
    {
    }

    value_storage::value_storage(value_storage&& other) noexcept
        : m_provider(other.m_provider), m_storage(std::move(other.m_storage)), m_size(other.m_size),
          m_capacity(other.m_capacity), m_max_records(other.m_max_records)
    {
      other.m_size = 0;
      other.m_capacity = 0;
    }

    value_storage::~value_storage() noexcept
    {
      destroy_records();
    }

    result<value_index> value_storage::append(value_record&& record) noexcept
    {
      if (m_size >= m_max_records)
      {
        return result<value_index>::failure(error::without_location(error_code::resource_limit));
      }

      // The limit check proves adding one cannot overflow size_t.
      const std::size_t required = m_size + 1;
      const auto reserved = reserve(required);

      if (!reserved.has_value())
      {
        return result<value_index>::failure(*reserved.error_if());
      }

      const value_index index{m_size};

      ::new (slot_address(m_storage, m_size)) value_record(std::move(record));
      m_size = required;

      return result<value_index>::success(index);
    }

    result<value_record*> value_storage::at(value_index index) noexcept
    {
      if (!index.is_set() || (index.position() >= m_size))
      {
        return result<value_record*>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      return result<value_record*>::success(live_record(index.position()));
    }

    result<const value_record*> value_storage::at(value_index index) const noexcept
    {
      if (!index.is_set() || (index.position() >= m_size))
      {
        return result<const value_record*>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      return result<const value_record*>::success(live_record(index.position()));
    }

    std::size_t value_storage::size() const noexcept
    {
      return m_size;
    }
  } // namespace detail
} // namespace adaptive_json5
