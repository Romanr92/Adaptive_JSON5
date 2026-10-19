// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/memory/decoded_string_builder.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/unicode/utf16.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace adaptive_json5
{
  status_result decoded_string_builder::reserve(std::size_t required) noexcept
  {
    if (required > m_max_units)
    {
      return status_result::failure(error::without_location(error_code::resource_limit));
    }

    if (required <= m_capacity)
    {
      return status_result::success();
    }

    std::size_t new_capacity = required;

    // Geometric growth avoids reallocation for every appended character.
    const auto doubled = checked_add(m_capacity, m_capacity);
    if (doubled.has_value())
    {
      const std::size_t bounded_capacity = (*doubled.value_if() > m_max_units) ? m_max_units : *doubled.value_if();

      if (bounded_capacity > new_capacity)
      {
        new_capacity = bounded_capacity;
      }
    }

    auto allocation = allocate_array(m_provider, new_capacity, sizeof(char16_t), alignof(char16_t));

    if (!allocation.has_value())
    {
      return status_result::failure(*allocation.error_if());
    }

    // Empty storage may have a null pointer, so omit the empty copy.
    if (m_size != std::size_t{})
    {
      std::memcpy(allocation.value_if()->data(), m_storage.data(), m_size * sizeof(char16_t));
    }

    // Replace the old woner only after allocation and coy succeeded.
    m_storage = std::move(*allocation.value_if());
    m_capacity = new_capacity;

    return status_result::success();
  }

  status_result decoded_string_builder::append_units(const char16_t* units, std::size_t count) noexcept
  {
    const auto new_size = checked_add(m_size, count);
    if (!new_size.has_value())
    {
      return status_result::failure(*new_size.error_if());
    }

    const auto reserved = reserve(*new_size.value_if());
    if (!reserved.has_value())
    {
      return status_result::failure(*reserved.error_if());
    }

    auto* bytes = static_cast<unsigned char*>(m_storage.data());

    // Reserve proved that the old offset and complete range fit.
    std::memcpy(bytes + (m_size * sizeof(char16_t)), units, (count * sizeof(char16_t)));
    m_size = *new_size.value_if();

    return status_result::success();
  }

  decoded_string_builder::decoded_string_builder(const storage_callbacks& provider, std::size_t max_units) noexcept
      : m_provider(provider), m_max_units(max_units)
  {
  }

  status_result decoded_string_builder::append_unit(char16_t unit) noexcept
  {
    return append_units(&unit, encoded_utf16::c_single_unit_count);
  }

  status_result decoded_string_builder::append_scalar(char32_t scalar) noexcept
  {
    const auto encoded = encode_utf16(scalar);
    if (!encoded.has_value())
    {
      return status_result::failure(*encoded.error_if());
    }

    return append_units(encoded.value_if()->units, encoded.value_if()->size);
  }

  decoded_string decoded_string_builder::finish() noexcept
  {
    decoded_string output{std::move(m_storage), m_size};

    // The builder no longwe owns storage or has capacity to append into.
    m_size = std::size_t{};
    m_capacity = std::size_t{};

    return output;
  }

  std::size_t decoded_string_builder::size() const noexcept
  {
    return m_size;
  }
} // namespace adaptive_json5
