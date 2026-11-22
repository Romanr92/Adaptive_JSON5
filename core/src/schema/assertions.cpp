#include "schema/assertions.hpp"

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/number_comparison.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "memory/work_budget.hpp"
#include "schema/exact_number.hpp"
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
      type_bit bit = type_bit::null_type;
      switch (*kind.value_if())
      {
      case value_kind::null:
        bit = type_bit::null_type;
        break;
      case value_kind::boolean:
        bit = type_bit::boolean_type;
        break;
      case value_kind::object:
        bit = type_bit::object_type;
        break;
      case value_kind::array:
        bit = type_bit::array_type;
        break;
      case value_kind::string:
        bit = type_bit::string_type;
        break;
      case value_kind::number:
      default:
        bit = type_bit::number_type;
        break;
      }
      if ((mask & static_cast<std::uint8_t>(bit)) != 0U)
      {
        return bool_result::success(true);
      }
      if ((bit != type_bit::number_type) || ((mask & static_cast<std::uint8_t>(type_bit::integer_type)) == 0U))
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
        value_index schema_node::*bound;
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
        if ((node.numeric_flags & static_cast<std::uint8_t>(rule.flag)) == 0U)
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
        failed |= passed ? keyword_mask{0U} : keyword_bit(rule.keyword);
      }
      if ((node.numeric_flags & static_cast<std::uint8_t>(numeric_bit::multiple_of)) == 0U)
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
    if (!issue.has_value() && (node.numeric_flags != 0U))
    {
      const auto kind = instance.kind();
      if (!kind.has_value())
      {
        return mask_result::failure(failure_of(kind.error_if()));
      }
      if (*kind.value_if() == value_kind::number)
      {
        issue = check_numeric(schema, node, instance, scratch, failed);
      }
    }
    if (issue.has_value())
    {
      return mask_result::failure(*issue);
    }
    return mask_result::success(failed);
  }
} // namespace adaptive_json5::detail
