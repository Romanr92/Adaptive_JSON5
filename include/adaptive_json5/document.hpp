#ifndef DOCUMENT_HPP_
#define DOCUMENT_HPP_

#include "adaptive_json5/container_links.hpp"
#include "decoded_string.hpp"
#include "document_links.hpp"
#include "error.hpp"
#include "owned_number.hpp"
#include "result.hpp"
#include "storage.hpp"
#include "value_index.hpp"
#include "value_record.hpp"
#include "value_storage.hpp"
#include "value_view.hpp"

#include <cstddef>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  /**
   * @brief Own value records and expose checked read-only views.
   * @par Requirements
   * P030; J5-028; J5-039; J5-040; J5-030; J5-031.
   * @note Values are created detached until selected as root or attached later.
   * All indices passed to this document must originate from this document.
   * Indices do not encode document identity; bounds checking cannot detect
   * an in-range index incorrectly taken from another document.
   * Moving or destroying the document invalidates its existing views.
   * Appending preserves indices but may invalidate borrowed payload pointers.
   * Every storage provider used by records or payloads must outlive its allocations.
   * This component record limit does not replace P031 resource accounting.
   */
  class document
  {
    private:
      detail::value_storage m_values;
      value_index m_root{};

      /**
       * @brief Transfer a payload into a new detached record.
       * @par Requirements
       * P030; J5-028; J5-040; J5-046.
       * @note Payload ownership transfers on entry, including on failure.
       * Existing records and the root remain unchanged if append fails.
       */
      result<value_index> add_payload(detail::value_payload payload) noexcept
      {
        detail::value_record record{std::move(payload), {}, {}, {}, {}, {}};
        return m_values.append(std::move(record));
      }

    public:
      /**
       * @brief Construct an empty document with explicit storage and record limit.
       * @par Requirements
       * P030; J5-028; J5-040.
       * @note Allocates nothing and initially has no root.
       */
      document(const storage_callbacks& storage, std::size_t max_records) noexcept : m_values(storage, max_records) {}

      /**
       * @brief Transfer record ownership and leave the source empty.
       * @par Requirements
       * P030; J5-040.
       * @note Existing views into either moved-from document state are invalid.
       * The source retains its storage configuration and can be reused.
       */
      document(document&& other) noexcept : m_values(std::move(other.m_values)), m_root(other.m_root)
      {
        other.m_root = value_index{};
      }

      /**
       * @brief Release all records and their owned payloads iteratively.
       * @par Requirements
       * P030; J5-040.
       */
      ~document() noexcept = default;

      document(const document&) = delete;
      document& operator=(const document&) = delete;
      document& operator=(document&&) = delete;

      /**
       * @brief Create a detached null value.
       * @par Requirements
       * P030; J5-028; J5-046.
       */
      result<value_index> make_null() noexcept
      {
        return add_payload(detail::value_payload{std::in_place_type<std::monostate>});
      }

      /**
       * @brief Create a detached Boolean without numeric coercion.
       * @par Requirements
       * P030; J5-028; J5-046.
       */
      result<value_index> make_boolean(bool value) noexcept
      {
        return add_payload(detail::value_payload{std::in_place_type<bool>, value});
      }

      /**
       * @brief Transfer decoded content into a detached string value.
       * @par Requirements
       * P030; J5-028; J5-040; J5-046.
       * @note Consumes value on entry, even if record allocation subsequently fails.
       * Empty decoded strings are valid values.
       */
      result<value_index> make_string(decoded_string value) noexcept
      {
        return add_payload(detail::value_payload{std::in_place_type<decoded_string>, std::move(value)});
      }

      /**
       * @brief Transfer validated numeric spelling into a detached numeric value.
       * @par Requirements
       * P030; J5-028; J5-034; J5-038; J5-040; J5-046.
       * @note Consumes value on entry, even if record allocation subsequently fails.
       * Rejects moved-from owners with empty spelling.
       */
      result<value_index> make_number(owned_number value) noexcept
      {
        if (value.spelling().size == 0)
        {
          return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
        }

        return add_payload(detail::value_payload{std::in_place_type<owned_number>, std::move(value)});
      }

      /**
       * @brief Create a detached empty array.
       * @par Requirements
       * P030; J5-028; J5-030; J5-046.
       */
      result<value_index> make_array() noexcept
      {
        return add_payload(detail::value_payload{std::in_place_type<array_data>});
      }

      /**
       * @brief Create a detached empty object.
       * @par Requirements
       * P030; J5-028; J5-031; J5-046.
       */
      result<value_index> make_object() noexcept
      {
        return add_payload(detail::value_payload{std::in_place_type<object_data>});
      }

      /**
       * @brief Append a detached value or subtree to an array.
       * @par Requirements
       * P030; J5-030; J5-039; J5-040.
       * @param array Array index belonging to this document.
       * @param child Detached child index belonging to this document.
       * @return Success or an explicit relationship/access error.
       * @note Preserves element order and performs no allocation.
       * Rejects wrong container type, existing attachment, root reuse and cycles.
       * Failure leaves the document unchanged.
       */
      status_result append_element(value_index array, value_index child) noexcept
      {
        return detail::attach_child(m_values, m_root, array, child, nullptr);
      }

      /**
       * @brief Append a named detached value or subtree to an object.
       * @par Requirements
       * P030; J5-031; J5-039; J5-040.
       * @param object Object index belonging to this document.
       * @param name External owned decoded name, transferred only on success.
       * @param child Detached child index belonging to this document.
       * @return Success or an explicit relationship/access error.
       * @note Preserves member order, empty names and duplicate names.
       * Performs no allocation; the name must already own its decoded content.
       * Failure leaves the document and supplied name unchanged.
       * Success moves from name. Its provider must outlive the transferred allocation.
       */
      status_result append_member(value_index object, decoded_string&& name, value_index child) noexcept
      {
        return detail::attach_child(m_values, m_root, object, child, &name);
      }

      /**
       * @brief Establish one detached value as the document root.
       * @par Requirements
       * P030; J5-028; J5-039.
       * @note Rejects an existing root, invalid index or attached/member record.
       * Failure leaves the document unchanged.
       */
      status_result set_root(value_index index) noexcept
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

      /**
       * @brief Borrow checked read-only access to a document-relative value.
       * @par Requirements
       * P030; J5-039; J5-040.
       * @note The returned view does not extend this document's lifetime.
       */
      result<value_view> view(value_index index) const noexcept
      {
        const auto resolved = m_values.at(index);

        if (!resolved.has_value())
        {
          return result<value_view>::failure(*resolved.error_if());
        }

        return result<value_view>::success(value_view{m_values, index});
      }

      /**
       * @brief Borrow the root or report that no root has been established.
       * @par Requirements
       * P030; J5-028; J5-039; J5-040.
       * @note An absent root is an access error, not a null value.
       */
      result<value_view> root() const noexcept
      {
        return view(m_root);
      }

      /**
       * @brief Return the number of owned records, including detached values.
       * @par Requirements
       * P030; J5-028.
       */
      std::size_t size() const noexcept
      {
        return m_values.size();
      }
  };
} // namespace adaptive_json5

#endif /* DOCUMENT_HPP_ */
