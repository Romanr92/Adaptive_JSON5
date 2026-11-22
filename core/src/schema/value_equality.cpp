#include "schema/value_equality.hpp"

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/number_comparison.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/pod_vector.hpp"
#include "schema/schema_work.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5::detail
{
  namespace
  {
    using verdict = result<bool, schema_error>;

    /**
     * @brief Two values still to be compared.
     * @par Requirements
     * P045; J5-061.
     */
    struct value_pair
    {
        value_view left{};
        value_view right{};
    };

    /**
     * @brief Translate a document access or comparison failure.
     * @par Requirements
     * P045; J5-043.
     * @par ID SWU-SCHEMA-128
     */
    verdict failed(const error* failure) noexcept
    {
      return (failure == nullptr) ? verdict::failure(schema_error::of(schema_error_code::internal_error))
                                  : verdict::failure(schema_error::from_storage(*failure));
    }

    /**
     * @brief Compare two decoded strings unit by unit.
     * @par Requirements
     * P045; J5-061.
     * @par ID SWU-SCHEMA-129
     */
    verdict strings_equal(const decoded_string& left, const decoded_string& right) noexcept
    {
      if (left.size() != right.size())
      {
        return verdict::success(false);
      }
      for (std::size_t index = 0U; index < left.size(); ++index)
      {
        const auto a = left.unit_at(index);
        const auto b = right.unit_at(index);
        if (!a.has_value())
        {
          return failed(a.error_if());
        }
        if (!b.has_value())
        {
          return failed(b.error_if());
        }
        if (*a.value_if() != *b.value_if())
        {
          return verdict::success(false);
        }
      }
      return verdict::success(true);
    }

    /**
     * @brief Compare two scalars of the same kind.
     * @par Requirements
     * P045; J5-059; J5-061.
     * @par ID SWU-SCHEMA-130
     */
    verdict scalars_equal(const value_pair& pair, value_kind kind, const storage_callbacks& scratch) noexcept
    {
      if (kind == value_kind::null)
      {
        return verdict::success(true);
      }
      if (kind == value_kind::boolean)
      {
        const auto a = pair.left.as_boolean();
        const auto b = pair.right.as_boolean();
        if (!a.has_value() || !b.has_value())
        {
          return failed(a.has_value() ? b.error_if() : a.error_if());
        }
        return verdict::success(*a.value_if() == *b.value_if());
      }
      if (kind == value_kind::string)
      {
        const auto a = pair.left.as_string();
        const auto b = pair.right.as_string();
        if (!a.has_value() || !b.has_value())
        {
          return failed(a.has_value() ? b.error_if() : a.error_if());
        }
        return strings_equal(**a.value_if(), **b.value_if());
      }
      const auto a = pair.left.as_number();
      const auto b = pair.right.as_number();
      if (!a.has_value() || !b.has_value())
      {
        return failed(a.has_value() ? b.error_if() : a.error_if());
      }
      const auto order = compare_numbers(**a.value_if(), **b.value_if(), scratch);
      if (!order.has_value())
      {
        return failed(order.error_if());
      }
      return verdict::success(*order.value_if() == number_order::equal);
    }

    /**
     * @brief Find the member of object whose decoded name equals name.
     * @par Requirements
     * P045; J5-061.
     * @return The member, nullopt when absent, or an operational failure. One work step per
     * name comparison.
     * @par ID SWU-SCHEMA-131
     */
    result<std::optional<value_view>, schema_error> find_by_name(const value_view& object, const decoded_string& name,
                                                                 schema_work& work) noexcept
    {
      using found = result<std::optional<value_view>, schema_error>;
      auto members = object.children();
      if (!members.has_value())
      {
        return found::failure(*failed(members.error_if()).error_if());
      }
      for (;;)
      {
        const auto member = members.value_if()->next();
        if (!member.has_value())
        {
          return found::failure(*failed(member.error_if()).error_if());
        }
        if (!member.value_if()->has_value())
        {
          return found::success(std::nullopt);
        }
        const auto stepped = work.step();
        if (!stepped.has_value())
        {
          return found::failure(*stepped.error_if());
        }
        const auto candidate = (*member.value_if())->member_name();
        if (!candidate.has_value())
        {
          return found::failure(*failed(candidate.error_if()).error_if());
        }
        const auto equal = decoded_name_equal(name, **candidate.value_if());
        if (!equal.has_value())
        {
          return found::failure(*failed(equal.error_if()).error_if());
        }
        if (*equal.value_if())
        {
          return found::success(**member.value_if());
        }
      }
    }

    /**
     * @brief Queue the element pairs of two arrays of equal size.
     * @par Requirements
     * P045; J5-061.
     * @par ID SWU-SCHEMA-132
     */
    verdict push_elements(const value_pair& pair, pod_vector<value_pair>& stack) noexcept
    {
      auto left = pair.left.children();
      auto right = pair.right.children();
      if (!left.has_value() || !right.has_value())
      {
        return failed(left.has_value() ? right.error_if() : left.error_if());
      }
      for (;;)
      {
        const auto a = left.value_if()->next();
        const auto b = right.value_if()->next();
        if (!a.has_value() || !b.has_value())
        {
          return failed(a.has_value() ? b.error_if() : a.error_if());
        }
        if (!a.value_if()->has_value() || !b.value_if()->has_value())
        {
          return verdict::success(true);
        }
        const auto pushed = stack.push_back(value_pair{**a.value_if(), **b.value_if()});
        if (!pushed.has_value())
        {
          return verdict::failure(*pushed.error_if());
        }
      }
    }

    /**
     * @brief Queue the member pairs of two objects of equal size, matched by name.
     * @par Requirements
     * P045; J5-061.
     * @return false when a left member has no right counterpart.
     * @par ID SWU-SCHEMA-133
     */
    verdict push_members(const value_pair& pair, pod_vector<value_pair>& stack, schema_work& work) noexcept
    {
      auto left = pair.left.children();
      if (!left.has_value())
      {
        return failed(left.error_if());
      }
      for (;;)
      {
        const auto a = left.value_if()->next();
        if (!a.has_value())
        {
          return failed(a.error_if());
        }
        if (!a.value_if()->has_value())
        {
          return verdict::success(true);
        }
        const auto name = (*a.value_if())->member_name();
        if (!name.has_value())
        {
          return failed(name.error_if());
        }
        const auto match = find_by_name(pair.right, **name.value_if(), work);
        if (!match.has_value())
        {
          return verdict::failure(*match.error_if());
        }
        if (!match.value_if()->has_value())
        {
          return verdict::success(false);
        }
        const auto pushed = stack.push_back(value_pair{**a.value_if(), **match.value_if()});
        if (!pushed.has_value())
        {
          return verdict::failure(*pushed.error_if());
        }
      }
    }

    /**
     * @brief Compare one pair and queue its children.
     * @par Requirements
     * P045; J5-059; J5-061.
     * @return false when the pair differs, true when it matches so far.
     * @par ID SWU-SCHEMA-134
     */
    verdict compare_pair(const value_pair& pair, pod_vector<value_pair>& stack, schema_work& work,
                         const storage_callbacks& scratch) noexcept
    {
      const auto charged = work.equality_step();
      if (!charged.has_value())
      {
        return verdict::failure(*charged.error_if());
      }
      const auto left_kind = pair.left.kind();
      const auto right_kind = pair.right.kind();
      if (!left_kind.has_value() || !right_kind.has_value())
      {
        return failed(left_kind.has_value() ? right_kind.error_if() : left_kind.error_if());
      }
      const value_kind kind = *left_kind.value_if();
      if (kind != *right_kind.value_if())
      {
        return verdict::success(false);
      }
      if ((kind != value_kind::array) && (kind != value_kind::object))
      {
        return scalars_equal(pair, kind, scratch);
      }
      const auto left_size = pair.left.container_size();
      const auto right_size = pair.right.container_size();
      if (!left_size.has_value() || !right_size.has_value())
      {
        return failed(left_size.has_value() ? right_size.error_if() : left_size.error_if());
      }
      if (*left_size.value_if() != *right_size.value_if())
      {
        return verdict::success(false);
      }
      return (kind == value_kind::array) ? push_elements(pair, stack) : push_members(pair, stack, work);
    }
  } // namespace

  /**
   * @brief Type-aware deep equality of two JSON values (Draft-07 instance equality).
   * @par Requirements
   * P045; J5-059; J5-061; ADR 0006 decision 11.
   * @par ID SWU-SCHEMA-127
   */
  result<bool, schema_error> values_equal(const value_view& left, const value_view& right, schema_work& work,
                                          const storage_callbacks& scratch) noexcept
  {
    pod_vector<value_pair> stack{scratch};
    const auto first = stack.push_back(value_pair{left, right});
    if (!first.has_value())
    {
      return verdict::failure(*first.error_if());
    }
    while (stack.size() != 0U)
    {
      const value_pair pair = stack.get(stack.size() - 1U);
      stack.pop_back();
      const auto compared = compare_pair(pair, stack, work, scratch);
      if (!compared.has_value() || !*compared.value_if())
      {
        return compared;
      }
    }
    return verdict::success(true);
  }
} // namespace adaptive_json5::detail
