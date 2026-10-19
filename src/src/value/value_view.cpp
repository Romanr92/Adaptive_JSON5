// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/value/value_view.hpp"

#include "adaptive_json5/detail/value/container_links.hpp"
#include "adaptive_json5/detail/value/value_record.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_kind.hpp"

#include <cstddef>
#include <optional>
#include <variant>

namespace adaptive_json5
{
  result<const detail::value_record*> value_view::record() const noexcept
  {
    if (m_storage == nullptr)
    {
      return result<const detail::value_record*>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return m_storage->at(m_index);
  }

  template <typename T>
  result<const T*> value_view::payload_as() const noexcept
  {
    const auto resolved = record();

    if (!resolved.has_value())
    {
      return result<const T*>::failure(*resolved.error_if());
    }

    const auto* payload = std::get_if<T>(&(*resolved.value_if())->payload);
    if (payload == nullptr)
    {
      return result<const T*>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return result<const T*>::success(payload);
  }

  result<value_view> value_view::child_at(std::size_t position, bool object) const noexcept
  {
    const auto resolved = record();
    if (!resolved.has_value())
    {
      return result<value_view>::failure(*resolved.error_if());
    }

    const auto& payload = (*resolved.value_if())->payload;
    const container_links* links = nullptr;

    if (object)
    {
      const auto* data = std::get_if<object_data>(&payload);
      if (data == nullptr)
      {
        return result<value_view>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      links = &data->members;
    }
    else
    {
      const auto* data = std::get_if<array_data>(&payload);
      if (data == nullptr)
      {
        return result<value_view>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      links = &data->elements;
    }

    if ((position >= links->count) || (links->count > m_storage->size()))
    {
      return result<value_view>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    value_index current = links->first;

    for (std::size_t ordinal{}; ordinal <= position; ++ordinal)
    {
      const auto child = m_storage->at(current);

      if (!child.has_value())
      {
        return result<value_view>::failure(*child.error_if());
      }

      const auto& child_record = **child.value_if();

      if (!child_record.parent.is_set() || (child_record.parent.position() != m_index.position()) ||
          (child_record.member_name.has_value() != object))
      {
        return result<value_view>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      if (ordinal == position)
      {
        return result<value_view>::success(value_view{*m_storage, current});
      }

      current = child_record.next_sibling;
    }

    return result<value_view>::failure(error::without_location(error_code::wrong_type_or_access));
  }

  value_view::value_view(const detail::value_storage& storage, value_index index) noexcept
      : m_storage(&storage), m_index(index)
  {
  }

  result<value_kind> value_view::kind() const noexcept
  {
    const auto resolved = record();
    if (!resolved.has_value())
    {
      return result<value_kind>::failure(*resolved.error_if());
    }

    const auto& payload = (*resolved.value_if())->payload;

    if (std::holds_alternative<std::monostate>(payload))
    {
      return result<value_kind>::success(value_kind::null);
    }
    if (std::holds_alternative<bool>(payload))
    {
      return result<value_kind>::success(value_kind::boolean);
    }
    if (std::holds_alternative<decoded_string>(payload))
    {
      return result<value_kind>::success(value_kind::string);
    }
    if (std::holds_alternative<owned_number>(payload))
    {
      return result<value_kind>::success(value_kind::number);
    }
    if (std::holds_alternative<array_data>(payload))
    {
      return result<value_kind>::success(value_kind::array);
    }
    if (std::holds_alternative<object_data>(payload))
    {
      return result<value_kind>::success(value_kind::object);
    }

    return result<value_kind>::failure(error::without_location(error_code::wrong_type_or_access));
  }

  result<bool> value_view::as_boolean() const noexcept
  {
    const auto boolean = payload_as<bool>();

    if (!boolean.has_value())
    {
      return result<bool>::failure(*boolean.error_if());
    }

    return result<bool>::success(**boolean.value_if());
  }

  result<const decoded_string*> value_view::as_string() const noexcept
  {
    return payload_as<decoded_string>();
  }

  result<const owned_number*> value_view::as_number() const noexcept
  {
    return payload_as<owned_number>();
  }

  result<std::optional<adaptive_json5::source_span>> value_view::source_span() const noexcept
  {
    const auto resolved = record();

    if (!resolved.has_value())
    {
      return result<std::optional<adaptive_json5::source_span>>::failure(*resolved.error_if());
    }

    return result<std::optional<adaptive_json5::source_span>>::success((*resolved.value_if())->source_span);
  }

  result<std::size_t> value_view::container_size() const noexcept
  {
    const auto resolved = record();

    if (!resolved.has_value())
    {
      return result<std::size_t>::failure(*resolved.error_if());
    }

    const auto& payload = (*resolved.value_if())->payload;

    if (const auto* array = std::get_if<array_data>(&payload); array != nullptr)
    {
      return result<std::size_t>::success(array->elements.count);
    }

    if (const auto* object = std::get_if<object_data>(&payload); object != nullptr)
    {
      return result<std::size_t>::success(object->members.count);
    }

    return result<std::size_t>::failure(error::without_location(error_code::wrong_type_or_access));
  }

  result<value_view> value_view::element_at(std::size_t position) const noexcept
  {
    return child_at(position, false);
  }

  result<value_view> value_view::member_at(std::size_t position) const noexcept
  {
    return child_at(position, true);
  }

  result<const decoded_string*> value_view::member_name() const noexcept
  {
    const auto resolved = record();

    if (!resolved.has_value())
    {
      return result<const decoded_string*>::failure(*resolved.error_if());
    }

    const auto& name = (*resolved.value_if())->member_name;
    if (!name.has_value())
    {
      return result<const decoded_string*>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return result<const decoded_string*>::success(&*name);
  }
} // namespace adaptive_json5
