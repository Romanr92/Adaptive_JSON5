#ifndef VALUE_VIEW_HPP_
#define VALUE_VIEW_HPP_

#include "adaptive_json5/container_links.hpp"
#include "adaptive_json5/value_record.hpp"
#include "decoded_string.hpp"
#include "error.hpp"
#include "owned_number.hpp"
#include "result.hpp"
#include "value_index.hpp"
#include "value_kind.hpp"
#include "value_storage.hpp"

#include <variant>

namespace adaptive_json5
{
  /**
   * @brief Borrow checked, read-only access to one document-owned value.
   * @par Requirements
   * P030; J5-028; J5-039; J5-040.
   * @note Owns no storage and does not extend the document's lifetime.
   * Moving or destroying the storage invalidates this view.
   * Appending records preserves indices, but growth invalidates returned pointers.
   * A default-constructed view is invalid, not a null value.
   * Operations allocate nothing and never coerce between value types.
   */
  class value_view
  {
    private:
      const detail::value_storage* m_storage = nullptr;
      value_index m_index{};

      /**
       * @brief Resolve the current record through checked storage access.
       * @par Requirements
       * P030; J5-039.
       * @note Reacquires the record instead of retaining a growth-sensitive pointer.
       */
      result<const detail::value_record*> record() const noexcept
      {
        if (m_storage == nullptr)
        {
          return result<const detail::value_record*>::failure(
            error::without_location(error_code::wrong_type_or_access));
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
      result<const T*> payload_as() const noexcept
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

    public:
      /**
       * @brief Construct an invalid view without storage.
       * @par Requirements
       * P030; J5-039.
       */
      constexpr value_view() noexcept = default;

      /**
       * @brief Borrow a storage owner and a document-relative value index.
       * @par Requirements
       * P030; J5-039; J5-040.
       * @note The storage must remain alive at the same address.
       * Construction does not validate the index; accessors perform that check.
       */
      value_view(const detail::value_storage& storage, value_index index) noexcept : m_storage(&storage), m_index(index)
      {
      }

      /**
       * @brief Return the stored value type without conversion.
       * @par Requirements
       * P030; J5-028; J5-039.
       */
      result<value_kind> kind() const noexcept
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
      result<bool> as_boolean() const noexcept
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
      result<const decoded_string*> as_string() const noexcept
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
      result<const owned_number*> as_number() const noexcept
      {
        return payload_as<owned_number>();
      }
  };
} // namespace adaptive_json5

#endif /* VALUE_VIEW_HPP_ */
