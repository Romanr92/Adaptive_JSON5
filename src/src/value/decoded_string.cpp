// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/value/decoded_string.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace adaptive_json5
{
  decoded_string::decoded_string(owned_block&& storage, std::size_t size) noexcept
      : m_storage(std::move(storage)), m_size(size)
  {
  }

  decoded_string::decoded_string(decoded_string&& other) noexcept
      : m_storage(std::move(other.m_storage)), m_size(other.m_size)
  {
    // The moved-from string must not advertise units it no longer owns.
    other.m_size = 0;
  }

  result<decoded_string> decoded_string::copy_units(const storage_callbacks& storage, const char16_t* units,
                                                    std::size_t count) noexcept
  {
    if (count == 0)
    {
      return result<decoded_string>::success(decoded_string{});
    }

    if (units == nullptr)
    {
      return result<decoded_string>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    auto allocation = allocate_array(storage, count, sizeof(char16_t), alignof(char16_t));
    if (!allocation.has_value())
    {
      return result<decoded_string>::failure(*allocation.error_if());
    }

    // Store representations as bytes; typed reads copy into a live char16_t.
    std::memcpy(allocation.value_if()->data(), units, allocation.value_if()->size());

    return result<decoded_string>::success(decoded_string{std::move(*allocation.value_if()), count});
  }

  std::size_t decoded_string::size() const noexcept
  {
    return m_size;
  }

  result<char16_t> decoded_string::unit_at(std::size_t index) const noexcept
  {
    if (index >= m_size)
    {
      return result<char16_t>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    const auto* bytes = static_cast<const unsigned char*>(m_storage.data());
    char16_t unit{};

    // Successful allocation alteady proved every in-range unit offset fits,
    std::memcpy(&unit, bytes + (index * sizeof(char16_t)), sizeof(unit));

    return result<char16_t>::success(unit);
  }
} // namespace adaptive_json5
