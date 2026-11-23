#include "schema/assertions.hpp"

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/number_comparison.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "memory/work_budget.hpp"
#include "schema/exact_number.hpp"
#include "schema/regex.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_values.hpp"
#include "schema/schema_work.hpp"
#include "schema/value_equality.hpp"
#include "value/number_analysis.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace adaptive_json5::detail
{
  namespace
  {
    using bool_result = result<bool, schema_error>;

    /**
     * @brief Translate a document access failure.
     * @par Requirements
     * P047; J5-043.
     * @par ID SWU-SCHEMA-208
     */
    schema_error failure_of(const error* failure) noexcept
    {
      return (failure == nullptr) ? schema_error::of(schema_error_code::internal_error)
                                  : schema_error::from_storage(*failure);
    }

    /**
     * @brief Report whether a number is mathematically an integer, independent of spelling.
     * @par Requirements
     * P047; J5-059.
     * @par ID SWU-SCHEMA-209
     */
    bool_result is_integer(const value_view& instance) noexcept
    {
      const auto number = instance.as_number();
      if (!number.has_value())
      {
        return bool_result::failure(failure_of(number.error_if()));
      }
      work_budget analysis_work{resource_limits{}.max_work_units};
      const auto analysis = analyse_number(**number.value_if(), analysis_work);
      if (!analysis.has_value())
      {
        return bool_result::failure(failure_of(analysis.error_if()));
      }
      return bool_result::success(analysis.value_if()->is_integral);
    }

    /**
     * @brief Return the type bit of a value kind; numbers map to number_type.
     * @par Requirements
     * P047; J5-057.
     * @par ID SWU-SCHEMA-309
     */
    type_bit kind_bit(value_kind kind) noexcept
    {
      switch (kind)
      {
      case value_kind::null:
        return type_bit::null_type;
      case value_kind::boolean:
        return type_bit::boolean_type;
      case value_kind::object:
        return type_bit::object_type;
      case value_kind::array:
        return type_bit::array_type;
      case value_kind::string:
        return type_bit::string_type;
      case value_kind::number:
      default:
        return type_bit::number_type;
      }
    }

    /**
     * @brief Draft-07 `type`: the instance kind (or integer by value) is one of the listed types.
     * @par Requirements
     * P047; J5-057; J5-059; Draft-07 Validation section 6.1.1.
     * @par ID SWU-SCHEMA-210
     */
    bool_result type_matches(const value_view& instance, std::uint8_t mask) noexcept
    {
      const auto kind = instance.kind();
      if (!kind.has_value())
      {
        return bool_result::failure(failure_of(kind.error_if()));
      }
      const type_bit bit = kind_bit(*kind.value_if());
      if (has_flag(mask, bit))
      {
        return bool_result::success(true);
      }
      if ((bit != type_bit::number_type) || (!has_flag(mask, type_bit::integer_type)))
      {
        return bool_result::success(false);
      }
      return is_integer(instance);
    }

    /**
     * @brief Draft-07 `enum`: the instance equals one of the listed values.
     * @par Requirements
     * P047; J5-057; J5-061; Draft-07 Validation section 6.1.2.
     * @par ID SWU-SCHEMA-211
     */
    bool_result enum_matches(const value_view& instance, const value_view& values, schema_work& work,
                             const storage_callbacks& scratch) noexcept
    {
      auto candidates = values.children();
      if (!candidates.has_value())
      {
        return bool_result::failure(failure_of(candidates.error_if()));
      }
      for (;;)
      {
        const auto candidate = candidates.value_if()->next();
        if (!candidate.has_value())
        {
          return bool_result::failure(failure_of(candidate.error_if()));
        }
        if (!candidate.value_if()->has_value())
        {
          return bool_result::success(false);
        }
        const auto equal = values_equal(instance, **candidate.value_if(), work, scratch);
        if (!equal.has_value() || *equal.value_if())
        {
          return equal;
        }
      }
    }

    /**
     * @brief Borrow a copied schema value.
     * @par Requirements
     * P047; J5-061.
     * @par ID SWU-SCHEMA-212
     */
    result<value_view, schema_error> schema_value(const compiled_schema& schema, value_index index) noexcept
    {
      const document* values = compiled_schema_access::values(schema);
      if (values == nullptr)
      {
        return result<value_view, schema_error>::failure(schema_error::of(schema_error_code::internal_error));
      }
      const auto view = values->view(index);
      if (!view.has_value())
      {
        return result<value_view, schema_error>::failure(failure_of(view.error_if()));
      }
      return result<value_view, schema_error>::success(*view.value_if());
    }

    /**
     * @brief Add keyword to failed when a check returned false; pass errors on.
     * @par Requirements
     * P047; J5-063.
     * @par ID SWU-SCHEMA-213
     */
    std::optional<schema_error> collect(const bool_result& passed, schema_keyword keyword,
                                        keyword_mask& failed) noexcept
    {
      if (!passed.has_value())
      {
        return *passed.error_if();
      }
      if (!*passed.value_if())
      {
        failed |= keyword_bit(keyword);
      }
      return std::nullopt;
    }

    /**
     * @brief Evaluate const and enum against the copied schema values.
     * @par Requirements
     * P047; J5-057; J5-061.
     * @par ID SWU-SCHEMA-214
     */
    std::optional<schema_error> check_values(const compiled_schema& schema, const schema_node& node,
                                             const value_view& instance, schema_work& work,
                                             const storage_callbacks& scratch, keyword_mask& failed) noexcept
    {
      if (node.has_const)
      {
        const auto expected = schema_value(schema, node.const_value);
        if (!expected.has_value())
        {
          return *expected.error_if();
        }
        const auto issue =
          collect(values_equal(instance, *expected.value_if(), work, scratch), schema_keyword::const_value, failed);
        if (issue.has_value())
        {
          return issue;
        }
      }
      if (!node.has_enum)
      {
        return std::nullopt;
      }
      const auto values = schema_value(schema, node.enum_values);
      if (!values.has_value())
      {
        return *values.error_if();
      }
      return collect(enum_matches(instance, *values.value_if(), work, scratch), schema_keyword::enum_values, failed);
    }
    /**
     * @brief Compare the instance number with one copied bound.
     * @par Requirements
     * P048; J5-059.
     * @par ID SWU-SCHEMA-227
     */
    result<number_order, schema_error> order_against(const compiled_schema& schema, const owned_number& instance,
                                                     value_index bound, const storage_callbacks& scratch) noexcept
    {
      using order_result = result<number_order, schema_error>;
      const auto view = schema_value(schema, bound);
      if (!view.has_value())
      {
        return order_result::failure(*view.error_if());
      }
      const auto number = view.value_if()->as_number();
      if (!number.has_value())
      {
        return order_result::failure(failure_of(number.error_if()));
      }
      const auto order = compare_numbers(instance, **number.value_if(), scratch);
      if (!order.has_value())
      {
        return order_result::failure(failure_of(order.error_if()));
      }
      return order_result::success(*order.value_if());
    }

    /**
     * @brief One bound keyword: which flag, which copied number, and which orders pass.
     * @par Requirements
     * P048; J5-057.
     */
    struct bound_rule
    {
        numeric_bit flag;
        value_index schema_node::* bound;
        schema_keyword keyword;
        bool pass_less;
        bool pass_equal;
        bool pass_greater;
    };

    /**
     * @brief Draft-07 numeric bounds and multipleOf on a number instance (P048).
     * @par Requirements
     * P048; J5-057; J5-059; Draft-07 Validation sections 6.2.1 to 6.2.5.
     * @par ID SWU-SCHEMA-228
     */
    std::optional<schema_error> check_numeric(const compiled_schema& schema, const schema_node& node,
                                              const value_view& instance, const storage_callbacks& scratch,
                                              keyword_mask& failed) noexcept
    {
      const auto number = instance.as_number();
      if (!number.has_value())
      {
        return failure_of(number.error_if());
      }
      constexpr std::array<bound_rule, 4> c_bounds{
        {{numeric_bit::minimum, &schema_node::minimum, schema_keyword::minimum, false, true, true},
         {numeric_bit::maximum, &schema_node::maximum, schema_keyword::maximum, true, true, false},
         {numeric_bit::exclusive_minimum, &schema_node::exclusive_minimum, schema_keyword::exclusive_minimum, false,
          false, true},
         {numeric_bit::exclusive_maximum, &schema_node::exclusive_maximum, schema_keyword::exclusive_maximum, true,
          false, false}}};
      for (const auto& rule : c_bounds)
      {
        if (!has_flag(node.numeric_flags, rule.flag))
        {
          continue;
        }
        const auto order = order_against(schema, **number.value_if(), node.*rule.bound, scratch);
        if (!order.has_value())
        {
          return *order.error_if();
        }
        const number_order value = *order.value_if();
        const bool passed = ((value == number_order::less) && rule.pass_less) ||
                            ((value == number_order::equal) && rule.pass_equal) ||
                            ((value == number_order::greater) && rule.pass_greater);
        failed |= passed ? c_no_keywords : keyword_bit(rule.keyword);
      }
      if (!has_flag(node.numeric_flags, numeric_bit::multiple_of))
      {
        return std::nullopt;
      }
      const auto divisor = schema_value(schema, node.multiple_of);
      if (!divisor.has_value())
      {
        return *divisor.error_if();
      }
      const auto divisor_number = divisor.value_if()->as_number();
      if (!divisor_number.has_value())
      {
        return failure_of(divisor_number.error_if());
      }
      return collect(is_multiple_of(**number.value_if(), **divisor_number.value_if(), scratch),
                     schema_keyword::multiple_of, failed);
    }
    /**
     * @brief Count the code points of a decoded string (a surrogate pair is one character).
     * @par Requirements
     * P049; J5-060; Draft-07 Validation section 6.3.1.
     * @note The instance check has rejected unpaired surrogates, so every high surrogate is
     * followed by a low one.
     * @par ID SWU-SCHEMA-280
     */
    result<std::size_t, schema_error> code_points(const decoded_string& text) noexcept
    {
      std::size_t count = 0U;
      std::size_t index = 0U;
      while (index < text.size())
      {
        const auto unit = text.unit_at(index);
        if (!unit.has_value())
        {
          return result<std::size_t, schema_error>::failure(failure_of(unit.error_if()));
        }
        const bool low_half = (*unit.value_if() >= 0xDC00U) && (*unit.value_if() <= 0xDFFFU);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.81)
        count += low_half ? 0U : 1U;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.80)
        ++index;
      }
      return result<std::size_t, schema_error>::success(count);
    }

    /**
     * @brief Draft-07 minLength, maxLength and pattern on a string instance (P049).
     * @par Requirements
     * P049; J5-057; J5-060; Draft-07 Validation sections 6.3.1 to 6.3.3.
     * @par ID SWU-SCHEMA-281
     */
    std::optional<schema_error> check_string(const compiled_schema& schema, const schema_node& node,
                                             const value_view& instance, schema_work& work,
                                             const storage_callbacks& scratch, keyword_mask& failed) noexcept
    {
      const auto text = instance.as_string();
      if (!text.has_value())
      {
        return failure_of(text.error_if());
      }
      const auto length = code_points(**text.value_if());
      if (!length.has_value())
      {
        return *length.error_if();
      }
      const bool has_min = has_flag(node.string_flags, string_bit::min_length);
      const bool has_max = has_flag(node.string_flags, string_bit::max_length);
      failed |=
        (has_min && (*length.value_if() < node.min_length)) ? keyword_bit(schema_keyword::min_length) : c_no_keywords;
      failed |=
        (has_max && (*length.value_if() > node.max_length)) ? keyword_bit(schema_keyword::max_length) : c_no_keywords;
      if (!has_flag(node.string_flags, string_bit::pattern))
      {
        return std::nullopt;
      }
      return collect(
        regex_search(compiled_schema_access::regex(schema), node.pattern, **text.value_if(), work, scratch),
        schema_keyword::pattern, failed);
    }
    /**
     * @brief Report whether all elements of an array are pairwise unequal (uniqueItems).
     * @par Requirements
     * P050; J5-057; J5-061; Draft-07 Validation section 6.4.5.
     * @note Compares every pair with the exact deep equality; each comparison is charged to
     * max_equality_steps and max_work_steps.
     * @par ID SWU-SCHEMA-288
     */
    bool_result all_unique(const value_view& instance, schema_work& work, const storage_callbacks& scratch) noexcept
    {
      auto outer = instance.children();
      if (!outer.has_value())
      {
        return bool_result::failure(failure_of(outer.error_if()));
      }
      for (;;)
      {
        const auto first = outer.value_if()->next();
        if (!first.has_value())
        {
          return bool_result::failure(failure_of(first.error_if()));
        }
        if (!first.value_if()->has_value())
        {
          return bool_result::success(true);
        }
        value_iterator later = *outer.value_if();
        for (;;)
        {
          const auto second = later.next();
          if (!second.has_value())
          {
            return bool_result::failure(failure_of(second.error_if()));
          }
          if (!second.value_if()->has_value())
          {
            break;
          }
          const auto equal = values_equal(**first.value_if(), **second.value_if(), work, scratch);
          if (!equal.has_value() || *equal.value_if())
          {
            return equal.has_value() ? bool_result::success(false) : equal;
          }
        }
      }
    }

    /**
     * @brief Draft-07 minItems, maxItems and uniqueItems on an array instance (P050).
     * @par Requirements
     * P050; J5-057; J5-061; Draft-07 Validation sections 6.4.3 to 6.4.5.
     * @par ID SWU-SCHEMA-289
     */
    std::optional<schema_error> check_array(const schema_node& node, const value_view& instance, schema_work& work,
                                            const storage_callbacks& scratch, keyword_mask& failed) noexcept
    {
      const auto size = instance.container_size();
      if (!size.has_value())
      {
        return failure_of(size.error_if());
      }
      const bool has_min = has_flag(node.array_flags, array_bit::min_items);
      const bool has_max = has_flag(node.array_flags, array_bit::max_items);
      failed |=
        (has_min && (*size.value_if() < node.min_items)) ? keyword_bit(schema_keyword::min_items) : c_no_keywords;
      failed |=
        (has_max && (*size.value_if() > node.max_items)) ? keyword_bit(schema_keyword::max_items) : c_no_keywords;
      if (!has_flag(node.array_flags, array_bit::unique_items))
      {
        return std::nullopt;
      }
      return collect(all_unique(instance, work, scratch), schema_keyword::unique_items, failed);
    }
    /**
     * @brief Report whether an object instance has a member with the decoded name.
     * @par Requirements
     * P051; J5-057.
     * @note One work step per name comparison.
     * @par ID SWU-SCHEMA-296
     */
    bool_result has_member(const value_view& object, const decoded_string& name, schema_work& work) noexcept
    {
      auto members = object.children();
      if (!members.has_value())
      {
        return bool_result::failure(failure_of(members.error_if()));
      }
      for (;;)
      {
        const auto member = members.value_if()->next();
        if (!member.has_value() || !member.value_if()->has_value())
        {
          return member.has_value() ? bool_result::success(false) : bool_result::failure(failure_of(member.error_if()));
        }
        const auto stepped = work.step();
        const auto member_name = (*member.value_if())->member_name();
        if (!stepped.has_value() || !member_name.has_value())
        {
          return stepped.has_value() ? bool_result::failure(failure_of(member_name.error_if()))
                                     : bool_result::failure(*stepped.error_if());
        }
        const auto equal = decoded_name_equal(**member_name.value_if(), name);
        if (!equal.has_value() || *equal.value_if())
        {
          return equal.has_value() ? bool_result::success(true) : bool_result::failure(failure_of(equal.error_if()));
        }
      }
    }

    /**
     * @brief Report whether an object has every name of a copied name list.
     * @par Requirements
     * P051; J5-057.
     * @par ID SWU-SCHEMA-297
     */
    bool_result has_all(const value_view& object, const value_view& names, schema_work& work) noexcept
    {
      auto entries = names.children();
      if (!entries.has_value())
      {
        return bool_result::failure(failure_of(entries.error_if()));
      }
      for (;;)
      {
        const auto entry = entries.value_if()->next();
        if (!entry.has_value() || !entry.value_if()->has_value())
        {
          return entry.has_value() ? bool_result::success(true) : bool_result::failure(failure_of(entry.error_if()));
        }
        const auto text = (*entry.value_if())->as_string();
        const auto present = text.has_value() ? has_member(object, **text.value_if(), work)
                                              : bool_result::failure(failure_of(text.error_if()));
        if (!present.has_value() || !*present.value_if())
        {
          return present;
        }
      }
    }

    /**
     * @brief Array-form dependencies: when a trigger member is present, its listed names must be too.
     * @par Requirements
     * P051; J5-057; Draft-07 Validation section 6.5.7.
     * @par ID SWU-SCHEMA-298
     */
    bool_result dependencies_hold(const value_view& object, const value_view& dependencies, schema_work& work) noexcept
    {
      auto entries = dependencies.children();
      if (!entries.has_value())
      {
        return bool_result::failure(failure_of(entries.error_if()));
      }
      for (;;)
      {
        const auto entry = entries.value_if()->next();
        if (!entry.has_value() || !entry.value_if()->has_value())
        {
          return entry.has_value() ? bool_result::success(true) : bool_result::failure(failure_of(entry.error_if()));
        }
        const auto trigger = (*entry.value_if())->member_name();
        const auto present = trigger.has_value() ? has_member(object, **trigger.value_if(), work)
                                                 : bool_result::failure(failure_of(trigger.error_if()));
        if (!present.has_value())
        {
          return present;
        }
        if (!*present.value_if())
        {
          continue;
        }
        const auto satisfied = has_all(object, **entry.value_if(), work);
        if (!satisfied.has_value() || !*satisfied.value_if())
        {
          return satisfied;
        }
      }
    }

    /**
     * @brief Draft-07 minProperties, maxProperties, required and array-form dependencies (P051).
     * @par Requirements
     * P051; J5-057; Draft-07 Validation sections 6.5.1 to 6.5.3 and 6.5.7.
     * @par ID SWU-SCHEMA-299
     */
    std::optional<schema_error> check_object(const compiled_schema& schema, const schema_node& node,
                                             const value_view& instance, schema_work& work,
                                             keyword_mask& failed) noexcept
    {
      const auto size = instance.container_size();
      if (!size.has_value())
      {
        return failure_of(size.error_if());
      }
      const bool has_min = has_flag(node.object_flags, object_bit::min_properties);
      const bool has_max = has_flag(node.object_flags, object_bit::max_properties);
      failed |= (has_min && (*size.value_if() < node.min_properties)) ? keyword_bit(schema_keyword::min_properties)
                                                                      : c_no_keywords;
      failed |= (has_max && (*size.value_if() > node.max_properties)) ? keyword_bit(schema_keyword::max_properties)
                                                                      : c_no_keywords;
      if (has_flag(node.object_flags, object_bit::required))
      {
        const auto names = schema_value(schema, node.required_names);
        const auto issue = names.has_value()
                             ? collect(has_all(instance, *names.value_if(), work), schema_keyword::required, failed)
                             : std::optional<schema_error>{*names.error_if()};
        if (issue.has_value())
        {
          return issue;
        }
      }
      if (!has_flag(node.object_flags, object_bit::dependencies))
      {
        return std::nullopt;
      }
      const auto dependencies = schema_value(schema, node.dependency_names);
      return dependencies.has_value() ? collect(dependencies_hold(instance, *dependencies.value_if(), work),
                                                schema_keyword::dependencies, failed)
                                      : std::optional<schema_error>{*dependencies.error_if()};
    }

    /**
     * @brief Run the type-specific assertions that apply to the instance's kind.
     * @par Requirements
     * P048; P049; P050; J5-057.
     * @par ID SWU-SCHEMA-291
     */
    std::optional<schema_error> check_by_kind(const compiled_schema& schema, const schema_node& node,
                                              const value_view& instance, schema_work& work,
                                              const storage_callbacks& scratch, keyword_mask& failed) noexcept
    {
      if ((node.numeric_flags == 0U) && (node.string_flags == 0U) && (node.array_flags == 0U) &&
          (node.object_flags == 0U))
      {
        return std::nullopt;
      }
      const auto kind = instance.kind();
      if (!kind.has_value())
      {
        return failure_of(kind.error_if());
      }
      if ((*kind.value_if() == value_kind::number) && (node.numeric_flags != 0U))
      {
        return check_numeric(schema, node, instance, scratch, failed);
      }
      if ((*kind.value_if() == value_kind::string) && (node.string_flags != 0U))
      {
        return check_string(schema, node, instance, work, scratch, failed);
      }
      if ((*kind.value_if() == value_kind::array) && (node.array_flags != 0U))
      {
        return check_array(node, instance, work, scratch, failed);
      }
      if ((*kind.value_if() == value_kind::object) && (node.object_flags != 0U))
      {
        return check_object(schema, node, instance, work, failed);
      }
      return std::nullopt;
    }
  } // namespace

  /**
   * @brief Evaluate a node's assertion keywords (those without subschemas) on one instance value.
   * @par Requirements
   * P047; J5-057; J5-059; J5-061; Draft-07 Validation section 6.
   * @par ID SWU-SCHEMA-207
   */
  result<keyword_mask, schema_error> check_assertions(const compiled_schema& schema, const schema_node& node,
                                                      const value_view& instance, schema_work& work,
                                                      const storage_callbacks& scratch) noexcept
  {
    using mask_result = result<keyword_mask, schema_error>;
    keyword_mask failed = 0U;
    if (node.type_mask != 0U)
    {
      const auto issue = collect(type_matches(instance, node.type_mask), schema_keyword::type, failed);
      if (issue.has_value())
      {
        return mask_result::failure(*issue);
      }
    }
    auto issue = check_values(schema, node, instance, work, scratch, failed);
    if (!issue.has_value())
    {
      issue = check_by_kind(schema, node, instance, work, scratch, failed);
    }
    if (issue.has_value())
    {
      return mask_result::failure(*issue);
    }
    return mask_result::success(failed);
  }
} // namespace adaptive_json5::detail
