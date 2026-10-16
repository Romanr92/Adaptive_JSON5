#ifndef DOCUMENT_LINKS_HPP_
#define DOCUMENT_LINKS_HPP_

#include "adaptive_json5/value_record.hpp"
#include "checked_size.hpp"
#include "container_links.hpp"
#include "decoded_string.hpp"
#include "error.hpp"
#include "result.hpp"
#include "value_index.hpp"
#include "value_storage.hpp"

#include <cstddef>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Attach a detached child at the end of an ordered container.
     * @par Requirements
     * P030; J5-030; J5-031; J5-039; J5-040.
     * @param values Storage belonging to one document.
     * @param root Current document root, possibly unset.
     * @param parent Index of the destination array or object.
     * @param child Index of a detached value or detached subtree.
     * @param member_name External owned name for an object, or nullptr for an array.
     * @return Success or an explicit relationship/access error.
     * @note All indices must belong to values; indices do not encode owner identity.
     * Existing relationships must have been created through validated document operations.
     * Rejects an existing parent, root reuse, self-attachment and ancestor cycles.
     * Object names are preserved exactly, including empty and repeated names.
     * Failure leaves relationships and the supplied name unchanged.
     * Success transfers the supplied name, if present.
     * Performs no allocation or recursive traversal.
     * Ancestor inspection is bounded by stored record count; P031 adds work budgets.
     */
    inline status_result attach_child(value_storage& values, value_index root, value_index parent, value_index child,
                                      decoded_string* member_name) noexcept
    {
      const auto parent_result = values.at(parent);

      if (!parent_result.has_value())
      {
        return status_result::failure(*parent_result.error_if());
      }

      const auto child_result = values.at(child);

      if (!child_result.has_value())
      {
        return status_result::failure(*child_result.error_if());
      }

      auto& parent_record = **parent_result.value_if();
      auto& child_record = **child_result.value_if();

      if ((parent.position() == child.position()) || ((root.is_set()) && (root.position() == child.position())))
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      if (child_record.parent.is_set() || child_record.next_sibling.is_set() || child_record.member_name.has_value())
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      container_links* links = nullptr;

      if (member_name == nullptr)
      {
        auto* array = std::get_if<array_data>(&parent_record.payload);
        if (array == nullptr)
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        links = &array->elements;
      }
      else
      {
        auto* object = std::get_if<object_data>(&parent_record.payload);
        if (object == nullptr)
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        links = &object->members;
      }
      // A detached subtree must not be attached beneath one of its own descendants.
      value_index ancestor = parent;
      std::size_t remaining = values.size();

      while (ancestor.is_set())
      {
        if ((remaining == 0) || (ancestor.position() == child.position()))
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        const auto resolved = values.at(ancestor);

        if (!resolved.has_value())
        {
          return status_result::failure(*resolved.error_if());
        }

        ancestor = (*resolved.value_if())->parent;
        --remaining;
      }

      const auto new_count = checked_add(links->count, 1);

      if (!new_count.has_value())
      {
        return status_result::failure(*new_count.error_if());
      }

      value_record* previous_last = nullptr;

      if (links->count == 0)
      {
        if (links->first.is_set() || links->last.is_set())
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }
      }
      else
      {
        if (!links->first.is_set() || !links->last.is_set())
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        const auto last_result = values.at(links->last);

        if (!last_result.has_value())
        {
          return status_result::failure(*last_result.error_if());
        }

        previous_last = *last_result.value_if();

        if (!previous_last->parent.is_set() || (previous_last->parent.position() != parent.position()) ||
            previous_last->next_sibling.is_set())
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }
      }

      // All fallible checks precede ownership transfer and relationship change.
      if (member_name != nullptr)
      {
        child_record.member_name.emplace(std::move(*member_name));
      }

      child_record.parent = parent;

      if (previous_last == nullptr)
      {
        links->first = child;
      }
      else
      {
        previous_last->next_sibling = child;
      }

      links->last = child;
      links->count = *new_count.value_if();

      return status_result::success();
    }
  } // namespace detail
} // namespace adaptive_json5

#endif /* DOCUMENT_LINKS_HPP_ */
