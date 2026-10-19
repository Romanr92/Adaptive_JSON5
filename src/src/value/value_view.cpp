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
  /**
   * @brief Resolve the current record through checked storage access.
   * @par Requirements
   * P030; J5-039.
   * @note Reacquires the record instead of retaining a growth-sensitive pointer.
   */
  result<const detail::value_record*> value_view::record() const noexcept
  {
    if (m_storage == nullptr)
    {
      return result<const detail::value_record*>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return m_storage->at(m_index);
  }

  /**
   * @brief Inspect the payload only when its stored type matches T.
   * @par Requirements
   * P030; J5-028; J5-039.
   * @note Returns a borrowed pointer without numeric or Boolean coercion.
   */
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

  /**
   * @brief Resolve a direct child by its zero-based source-order position.
   * @par Requirements
   * P030; J5-030; J5-031; J5-039.
   * @param position Direct-child ordinal, not a document record index.
   * @param object True for object members, false for array elements.
   * @note Requires document relationships maintained by validated operations.
   * Traversal is iterative and visits at most position plus one records.
   * P031 adds configurable work accounting.
   */
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

  /**
   * @brief Borrow a storage owner and a document-relative value index.
   * @par Requirements
   * P030; J5-039; J5-040.
   * @note The storage must remain alive at the same address.
   * Construction does not validate the index; accessors perform that check.
   */
  value_view::value_view(const detail::value_storage& storage, value_index index) noexcept
      : m_storage(&storage), m_index(index)
  {
  }

  /**
   * @brief Return the stored value type without conversion.
   * @par Requirements
   * P030; J5-028; J5-039.
   */
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

  /**
   * @brief Read a Boolean value without accepting numeric substitutes.
   * @par Requirements
   * P030; J5-028; J5-039.
   */
  result<bool> value_view::as_boolean() const noexcept
  {
    const auto boolean = payload_as<bool>();

    if (!boolean.has_value())
    {
      return result<bool>::failure(*boolean.error_if());
    }

    return result<bool>::success(**boolean.value_if());
  }

  /**
   * @brief Borrow decoded string content after checking the value type.
   * @par Requirements
   * P030; J5-028; J5-039; J5-040.
   * @note The returned pointer is invalidated by storage growth or destruction.
   */
  result<const decoded_string*> value_view::as_string() const noexcept
  {
    return payload_as<decoded_string>();
  }

  /**
   * @brief Borrow an owned number after checking the value type.
   * @par Requirements
   * P030; J5-028; J5-039; J5-040.
   * @note Performs no numeric conversion.
   * The returned pointer is invalidated by storage growth or destruction.
   */
  result<const owned_number*> value_view::as_number() const noexcept
  {
    return payload_as<owned_number>();
  }

  /**
   * @brief Copy this value's optional original source range.
   * @par Requirements
   * P032; J5-039; J5-041.
   * @return Stored span, successful nullopt when absent, or an access failure.
   * @note Absence of metadata is distinct from an invalid view.
   * Returned metadata owns no input text and extends no input lifetime.
   * The copied span remains valid independently of record storage.
   */
  result<std::optional<adaptive_json5::source_span>> value_view::source_span() const noexcept
  {
    const auto resolved = record();

    if (!resolved.has_value())
    {
      return result<std::optional<adaptive_json5::source_span>>::failure(*resolved.error_if());
    }

    return result<std::optional<adaptive_json5::source_span>>::success((*resolved.value_if())->source_span);
  }

  /**
   * @brief Return the direct element or member count.
   * @par Requirements
   * P030; J5-030; J5-031; J5-039.
   * @note Rejects scalar values rather than treating them as empty containers.
   */
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

  /**
   * @brief Borrow an array element by zero-based source-order position.
   * @par Requirements
   * P030; J5-030; J5-039; J5-040.
   * @note Rejects nonarrays and out-of-range positions.
   * Linear traversal; repeated indexed enumeration has quadratic total cost.
   */
  result<value_view> value_view::element_at(std::size_t position) const noexcept
  {
    return child_at(position, false);
  }

  /**
   * @brief Borrow an object member's value by zero-based source-order position.
   * @par Requirements
   * P030; J5-031; J5-039; J5-040.
   * @note Includes every duplicate-name occurrence.
   * Rejects nonobjects and out-of-range positions.
   * Linear traversal; repeated indexed enumeration has quadratic total cost.
   */
  result<value_view> value_view::member_at(std::size_t position) const noexcept
  {
    return child_at(position, true);
  }

  /**
   * @brief Borrow the decoded name attached to an object-member value.
   * @par Requirements
   * P030; J5-031; J5-039; J5-040.
   * @note An empty member name succeeds; an absent name is an access error.
   * The returned pointer is invalidated by storage growth or destruction.
   */
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

  /**
   * @brief Copy an object member name's optional original source range.
   * @par Requirements
   * P036; J5-032; J5-039; J5-041; J5-044.
   * @return Stored span, successful nullopt when metadata is absent,
   * or an access failure when the view is invalid or has no member name.
   * @note An empty decoded member name is valid.
   * Quoted-name spans include their quotation marks.
   * Returned metadata contains no source pointer and performs no allocation.
   */
  result<std::optional<adaptive_json5::source_span>> value_view::member_name_source_span() const noexcept
  {
    using span_result = result<std::optional<adaptive_json5::source_span>>;

    const auto resolved = record();

    if (!resolved.has_value())
    {
      return span_result::failure(*resolved.error_if());
    }

    const auto& member = **resolved.value_if();

    // A missing name is invalid access; missing source metadata is permitted.
    if (!member.member_name.has_value())
    {
      return span_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return span_result::success(member.member_name_source_span);
  }
} // namespace adaptive_json5
