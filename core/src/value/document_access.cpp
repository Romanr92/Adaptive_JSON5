#include "value/document_access.hpp"

#include "value/document_links.hpp"

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
  } // namespace detail
} // namespace adaptive_json5
