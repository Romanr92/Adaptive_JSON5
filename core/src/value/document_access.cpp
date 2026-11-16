#include "value/document_access.hpp"

#include "value/document_links.hpp"

#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Append a completed value to a detached array without ancestor traversal.
     * @par ID SWU-VALUE-042
     */
    status_result document_access::attach_parsed_element(document& values, value_index array,
                                                         value_index child) noexcept
    {
      return attach_to_detached_parent(values.m_values, values.m_root, array, child, nullptr);
    }

    /**
     * @brief Append a completed named value to a detached object without ancestor traversal.
     * @par ID SWU-VALUE-043
     */
    status_result document_access::attach_parsed_member(document& values, value_index object, decoded_string&& name,
                                                        value_index child, source_span name_span,
                                                        std::size_t input_size) noexcept
    {
      return attach_to_detached_parent(values.m_values, values.m_root, object, child, &name, name_span, input_size);
    }

    /**
     * @brief Reserve exactly the counted number of records in an empty document.
     * @par ID SWU-VALUE-044
     */
    status_result document_access::reserve_records_exact(document& values, std::size_t records) noexcept
    {
      return values.m_values.reserve_exact(records);
    }

    /**
     * @brief Construct an empty document; construction is internal (ADR 0007).
     * @par ID SWU-VALUE-158
     */
    document document_access::create(const storage_callbacks& storage, std::size_t max_records) noexcept
    {
      return document{storage, max_records};
    }

    /**
     * @brief Create a detached null value.
     * @par ID SWU-VALUE-159
     */
    result<value_index> document_access::make_null(document& values) noexcept
    {
      return values.make_null();
    }

    /**
     * @brief Create a detached Boolean value.
     * @par ID SWU-VALUE-160
     */
    result<value_index> document_access::make_boolean(document& values, bool value) noexcept
    {
      return values.make_boolean(value);
    }

    /**
     * @brief Transfer decoded content into a detached string value.
     * @par ID SWU-VALUE-161
     */
    result<value_index> document_access::make_string(document& values, decoded_string value) noexcept
    {
      return values.make_string(std::move(value));
    }

    /**
     * @brief Transfer validated numeric spelling into a detached numeric value.
     * @par ID SWU-VALUE-162
     */
    result<value_index> document_access::make_number(document& values, owned_number value) noexcept
    {
      return values.make_number(std::move(value));
    }

    /**
     * @brief Create a detached empty array.
     * @par ID SWU-VALUE-163
     */
    result<value_index> document_access::make_array(document& values) noexcept
    {
      return values.make_array();
    }

    /**
     * @brief Create a detached empty object.
     * @par ID SWU-VALUE-164
     */
    result<value_index> document_access::make_object(document& values) noexcept
    {
      return values.make_object();
    }

    /**
     * @brief Append a detached value or subtree to an array, checking ancestors.
     * @par ID SWU-VALUE-165
     */
    status_result document_access::append_element(document& values, value_index array, value_index child) noexcept
    {
      return values.append_element(array, child);
    }

    /**
     * @brief Append a named detached value or subtree to an object, checking ancestors.
     * @par ID SWU-VALUE-166
     */
    status_result document_access::append_member(document& values, value_index object, decoded_string&& name,
                                                 value_index child) noexcept
    {
      return values.append_member(object, std::move(name), child);
    }

    /**
     * @brief Append a named detached value with its name-token range, checking ancestors.
     * @par ID SWU-VALUE-167
     */
    status_result document_access::append_member(document& values, value_index object, decoded_string&& name,
                                                 value_index child, source_span name_span,
                                                 std::size_t input_size) noexcept
    {
      return values.append_member(object, std::move(name), child, name_span, input_size);
    }

    /**
     * @brief Assign or replace a value's original source range.
     * @par ID SWU-VALUE-168
     */
    status_result document_access::set_source_span(document& values, value_index index, source_span span,
                                                   std::size_t input_size) noexcept
    {
      return values.set_source_span(index, span, input_size);
    }

    /**
     * @brief Assign or replace an object member name's original source range.
     * @par ID SWU-VALUE-169
     */
    status_result document_access::set_member_name_source_span(document& values, value_index index, source_span span,
                                                               std::size_t input_size) noexcept
    {
      return values.set_member_name_source_span(index, span, input_size);
    }

    /**
     * @brief Establish one detached value as the document root.
     * @par ID SWU-VALUE-170
     */
    status_result document_access::set_root(document& values, value_index index) noexcept
    {
      return values.set_root(index);
    }
  } // namespace detail
} // namespace adaptive_json5
