#include "adaptive_json5/value/value_iterator.hpp"

#include "adaptive_json5/detail/value/container_links.hpp"
#include "adaptive_json5/detail/value/value_record.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <optional>
#include <variant>

namespace adaptive_json5
{
  /**
   * @brief Retain the validated container's direct-child traversal state.
   * @par Requirements
   * P030; J5-030; J5-031; J5-039.
   */
  value_iterator::value_iterator(const detail::value_storage& storage, value_index parent, const container_links& links,
                                 bool object) noexcept
      : m_storage(&storage), m_parent(parent), m_next(links.first), m_remaining(links.count), m_object(object)
  {
  }

  /**
   * @brief Begin direct-child iteration over an array or object.
   * @par Requirements
   * P030; J5-030; J5-031; J5-039.
   * @note Rejects scalar values and invalid container indices.
   * Requires relationships maintained by validated document operations.
   */
  result<value_iterator> value_iterator::create(const detail::value_storage& storage, value_index parent) noexcept
  {
    const auto resolved = storage.at(parent);
    if (!resolved.has_value())
    {
      return result<value_iterator>::failure(*resolved.error_if());
    }

    const auto& payload = (*resolved.value_if())->payload;
    const container_links* links = nullptr;
    bool object = false;

    if (const auto* array = std::get_if<array_data>(&payload); array != nullptr)
    {
      links = &array->elements;
    }
    else if (const auto* members = std::get_if<object_data>(&payload); members != nullptr)
    {
      links = &members->members;
      object = true;
    }
    else
    {
      return result<value_iterator>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // Reject impossible counts before starting traversal.
    if (links->count > storage.size())
    {
      return result<value_iterator>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    if (links->count == 0)
    {
      if (links->first.is_set() || links->last.is_set())
      {
        return result<value_iterator>::failure(error::without_location(error_code::wrong_type_or_access));
      }
    }
    else if (!links->first.is_set() || !links->last.is_set())
    {
      return result<value_iterator>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return result<value_iterator>::success(value_iterator{storage, parent, *links, object});
  }

  /**
   * @brief Return the next direct child or successful end of iteration.
   * @par Requirements
   * P030; J5-030; J5-031; J5-039; J5-040.
   * @note Failure leaves traversal state unchanged.
   * Repeated calls after completion return successful empty optionals.
   * Each call performs constant work; P031 adds work accounting.
   */
  result<std::optional<value_view>> value_iterator::next() noexcept
  {
    if (m_storage == nullptr)
    {
      return result<std::optional<value_view>>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    if (m_remaining == 0)
    {
      return result<std::optional<value_view>>::success(std::nullopt);
    }

    const auto resolved = m_storage->at(m_next);

    if (!resolved.has_value())
    {
      return result<std::optional<value_view>>::failure(*resolved.error_if());
    }

    const auto& child = **resolved.value_if();

    // A sibling chain must remain inside its originating container.
    if (!child.parent.is_set() || (child.parent.position() != m_parent.position()) ||
        (child.member_name.has_value() != m_object))
    {
      return result<std::optional<value_view>>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // The recorded count and the end of the chain must agree.
    if ((m_remaining == 1 && child.next_sibling.is_set()) || (m_remaining > 1 && !child.next_sibling.is_set()))
    {
      return result<std::optional<value_view>>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    const value_view current{*m_storage, m_next};

    // Advance only after all check for this child succeed.
    m_next = child.next_sibling;
    --m_remaining;

    return result<std::optional<value_view>>::success(std::optional<value_view>{current});
  }
} // namespace adaptive_json5
