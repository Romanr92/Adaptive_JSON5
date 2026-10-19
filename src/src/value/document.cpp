// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/value/document.hpp"

#include "adaptive_json5/detail/value/container_links.hpp"
#include "adaptive_json5/detail/value/document_links.hpp"
#include "adaptive_json5/detail/value/member_lookup.hpp"
#include "adaptive_json5/detail/value/value_record.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/member_lookup.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstddef>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Append one detached payload using the document's flat record storage.
     * @par Requirements
     * P030; J5-028; J5-040; J5-046.
     * @note Ownership transfers on entry; existing records survive append failure.
     */
    result<value_index> add_payload(detail::value_storage& values, detail::value_payload payload) noexcept
    {
      detail::value_record record{std::move(payload), {}, {}, {}, {}, {}};
      return values.append(std::move(record));
    }
  } // namespace

  document::document(const storage_callbacks& storage, std::size_t max_records) noexcept
      : m_values(storage, max_records)
  {
  }

  document::document(document&& other) noexcept : m_values(std::move(other.m_values)), m_root(other.m_root)
  {
    other.m_root = value_index{};
  }

  result<value_index> document::make_null() noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<std::monostate>});
  }

  result<value_index> document::make_boolean(bool value) noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<bool>, value});
  }

  result<value_index> document::make_string(decoded_string value) noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<decoded_string>, std::move(value)});
  }

  result<value_index> document::make_number(owned_number value) noexcept
  {
    if (value.spelling().size == 0)
    {
      return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return add_payload(m_values, detail::value_payload{std::in_place_type<owned_number>, std::move(value)});
  }

  result<value_index> document::make_array() noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<array_data>});
  }

  result<value_index> document::make_object() noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<object_data>});
  }

  status_result document::append_element(value_index array, value_index child) noexcept
  {
    return detail::attach_child(m_values, m_root, array, child, nullptr);
  }

  status_result document::append_member(value_index object, decoded_string&& name, value_index child) noexcept
  {
    return detail::attach_child(m_values, m_root, object, child, &name);
  }

  status_result document::set_source_span(value_index index, adaptive_json5::source_span span,
                                          std::size_t input_size) noexcept
  {
    const auto resolved = m_values.at(index);

    if (!resolved.has_value())
    {
      return status_result::failure(*resolved.error_if());
    }

    // Subtraction validates the range without overflowing its end position.
    if ((span.byte_offset > input_size) || (span.byte_length > (input_size - span.byte_offset)))
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // Replace metadata only after all validation succeeds.
    (*resolved.value_if())->source_span.emplace(span);

    return status_result::success();
  }

  status_result document::set_root(value_index index) noexcept
  {
    if (m_root.is_set())
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    const auto resolved = m_values.at(index);

    if (!resolved.has_value())
    {
      return status_result::failure(*resolved.error_if());
    }

    const auto& record = **resolved.value_if();
    if (record.parent.is_set() || record.next_sibling.is_set() || record.member_name.has_value())
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    m_root = index;
    return status_result::success();
  }

  result<value_view> document::view(value_index index) const noexcept
  {
    const auto resolved = m_values.at(index);

    if (!resolved.has_value())
    {
      return result<value_view>::failure(*resolved.error_if());
    }

    return result<value_view>::success(value_view{m_values, index});
  }

  result<value_iterator> document::iterate(value_index container) const noexcept
  {
    return value_iterator::create(m_values, container);
  }

  result<member_lookup> document::lookup_member(value_index object, const decoded_string& name) const noexcept
  {
    return detail::lookup_member(m_values, object, name);
  }

  result<value_view> document::root() const noexcept
  {
    return view(m_root);
  }

  std::size_t document::size() const noexcept
  {
    return m_values.size();
  }
} // namespace adaptive_json5
