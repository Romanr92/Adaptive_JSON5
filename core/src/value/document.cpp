#include "adaptive_json5/value/document.hpp"

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
#include "value/container_links.hpp"
#include "value/document_links.hpp"
#include "value/member_lookup.hpp"
#include "value/value_record.hpp"

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
    [[nodiscard]] result<value_index> add_payload(detail::value_storage& values, detail::value_payload payload) noexcept
    {
      detail::value_record record{std::move(payload), {}, {}, {}, {}, {}};
      return values.append(std::move(record));
    }
  } // namespace

  /**
   * @brief Construct an empty document with explicit storage and record limit.
   * @par Requirements
   * P030; J5-028; J5-040.
   * @note Allocates nothing and initially has no root.
   */
  document::document(const storage_callbacks& storage, std::size_t max_records) noexcept
      : m_values(storage, max_records)
  {
  }

  /**
   * @brief Transfer record ownership and leave the source empty.
   * @par Requirements
   * P030; J5-040.
   * @note Existing views into either moved-from document state are invalid.
   * The source retains its storage configuration and can be reused.
   */
  document::document(document&& other) noexcept : m_values(std::move(other.m_values)), m_root(other.m_root)
  {
    other.m_root = value_index{};
  }

  /**
   * @brief Create a detached null value.
   * @par Requirements
   * P030; J5-028; J5-046.
   */
  result<value_index> document::make_null() noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<std::monostate>});
  }

  /**
   * @brief Create a detached Boolean without numeric coercion.
   * @par Requirements
   * P030; J5-028; J5-046.
   */
  result<value_index> document::make_boolean(bool value) noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<bool>, value});
  }

  /**
   * @brief Transfer decoded content into a detached string value.
   * @par Requirements
   * P030; J5-028; J5-040; J5-046.
   * @note Consumes value on entry, even if record allocation subsequently fails.
   * Empty decoded strings are valid values.
   */
  result<value_index> document::make_string(decoded_string value) noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<decoded_string>, std::move(value)});
  }

  /**
   * @brief Transfer validated numeric spelling into a detached numeric value.
   * @par Requirements
   * P030; J5-028; J5-034; J5-038; J5-040; J5-046.
   * @note Consumes value on entry, even if record allocation subsequently fails.
   * Rejects moved-from owners with empty spelling.
   */
  result<value_index> document::make_number(owned_number value) noexcept
  {
    if (value.spelling().size == 0)
    {
      return result<value_index>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return add_payload(m_values, detail::value_payload{std::in_place_type<owned_number>, std::move(value)});
  }

  /**
   * @brief Create a detached empty array.
   * @par Requirements
   * P030; J5-028; J5-030; J5-046.
   */
  result<value_index> document::make_array() noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<array_data>});
  }

  /**
   * @brief Create a detached empty object.
   * @par Requirements
   * P030; J5-028; J5-031; J5-046.
   */
  result<value_index> document::make_object() noexcept
  {
    return add_payload(m_values, detail::value_payload{std::in_place_type<object_data>});
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
  status_result document::append_element(value_index array, value_index child) noexcept
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
  status_result document::append_member(value_index object, decoded_string&& name, value_index child) noexcept
  {
    return detail::attach_child(m_values, m_root, object, child, &name);
  }

  /**
   * @brief Append a named detached value with its original name-token range.
   * @par Requirements
   * P036; J5-031; J5-032; J5-039; J5-040; J5-041.
   * @param object Object index belonging to this document.
   * @param name Owned decoded name, transferred only on success.
   * @param child Detached child index belonging to this document.
   * @param name_span Original member-name token offset and length in bytes.
   * @param input_size Original input size used to validate name_span.
   * @return Success or an explicit relationship/access/range failure.
   * @note Preserves source member order, empty names and repeated names.
   * Failure leaves relationships, metadata and the supplied name unchanged.
   * Success transfers the name and records its span atomically.
   * Stores no input pointer and performs no allocation.
   * Does not enforce duplicate-name rejection.
   */
  status_result document::append_member(value_index object, decoded_string&& name, value_index child,
                                        adaptive_json5::source_span name_span, std::size_t input_size) noexcept
  {
    return detail::attach_child(m_values, m_root, object, child, &name, name_span, input_size);
  }

  /**
   * @brief Assign or replace a value's original source range.
   * @par Requirements
   * P032; J5-039; J5-041.
   * @param index Value index belonging to this document.
   * @param span Original zero-based byte offset and byte length.
   * @param input_size Original input size in bytes.
   * @return Success or an explicit access/range failure.
   * @note Failure leaves existing metadata unchanged.
   * A zero-length range at input_size is permitted.
   * Stores no input pointer and performs no allocation.
   * The caller must supply the correct original input size and provenance.
   * Bounds validation does not prove that the range spells this value.
   */
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

  /**
   * @brief Assign or replace an object member name's original source range.
   * @par Requirements
   * P036; J5-032; J5-039; J5-041; J5-044.
   * @param index Object-member value index belonging to this document.
   * @param span Original member-name token offset and length in bytes.
   * @param input_size Original input size in bytes.
   * @return Success or an explicit access/range failure.
   * @note Requires an existing member_name, including a valid empty name.
   * The span describes source spelling, including quotes when present.
   * Failure leaves existing metadata unchanged.
   * Stores no input pointer and performs no allocation.
   * The caller supplies the correct input size and source provenance.
   */
  status_result document::set_member_name_source_span(value_index index, adaptive_json5::source_span span,
                                                      std::size_t input_size) noexcept
  {
    const auto resolved = m_values.at(index);

    if (!resolved.has_value())
    {
      return status_result::failure(*resolved.error_if());
    }

    auto& record = **resolved.value_if();

    // Empty decoded names are valid; absence means this is not a named member.
    if (!record.member_name.has_value())
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // Subtraction checks the end without overflowing offset plus length.
    if ((span.byte_offset > input_size) || (span.byte_length > (input_size - span.byte_offset)))
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    record.member_name_source_span.emplace(span);

    return status_result::success();
  }

  /**
   * @brief Establish one detached value as the document root.
   * @par Requirements
   * P030; J5-028; J5-039.
   * @note Rejects an existing root, invalid index or attached/member record.
   * Failure leaves the document unchanged.
   */
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

  /**
   * @brief Borrow checked read-only access to a document-relative value.
   * @par Requirements
   * P030; J5-039; J5-040.
   * @note The returned view does not extend this document's lifetime.
   */
  result<value_view> document::view(value_index index) const noexcept
  {
    const auto resolved = m_values.at(index);

    if (!resolved.has_value())
    {
      return result<value_view>::failure(*resolved.error_if());
    }

    return result<value_view>::success(value_view{m_values, index});
  }

  /**
   * @brief Begin source-order iteration over a container's direct children.
   * @par Requirements
   * P030; J5-030; J5-031; J5-039; J5-040.
   * @param container Array or object index belonging to this document.
   * @note Rejects invalid indices and scalar values.
   * Empty containers produce valid iterators that immediately report completion.
   * The iterator borrows this document and allocates nothing.
   * Moving or destroying this document invalidates the iterator.
   * Do not modify document relationships during iteration.
   */
  result<value_iterator> document::iterate(value_index container) const noexcept
  {
    return value_iterator::create(m_values, container);
  }

  /**
   * @brief Inspect an object's members for an exact decoded-name match.
   * @par Requirements
   * P030; J5-031; J5-033; J5-039; J5-040.
   * @param object Object index belonging to this document.
   * @param name Decoded name borrowed for this call only.
   * @return Absent, unique or ambiguous match, or an explicit access failure.
   * @note Rejects invalid indices, arrays and scalar values.
   * Only a unique match supplies a borrowed value view.
   * Preserves duplicate members and never selects among ambiguous matches.
   * Allocates nothing and does not modify the document or name.
   * Moving or destroying the document invalidates any returned view.
   */
  result<member_lookup> document::lookup_member(value_index object, const decoded_string& name) const noexcept
  {
    return detail::lookup_member(m_values, object, name);
  }

  /**
   * @brief Borrow the root or report that no root has been established.
   * @par Requirements
   * P030; J5-028; J5-039; J5-040.
   * @note An absent root is an access error, not a null value.
   */
  result<value_view> document::root() const noexcept
  {
    return view(m_root);
  }

  /**
   * @brief Return the number of owned records, including detached values.
   * @par Requirements
   * P030; J5-028.
   */
  std::size_t document::size() const noexcept
  {
    return m_values.size();
  }
} // namespace adaptive_json5
