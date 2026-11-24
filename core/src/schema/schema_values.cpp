#include "schema/schema_values.hpp"

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/pod_vector.hpp"
#include "schema/schema_graph.hpp"
#include "value/document_access.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace adaptive_json5::detail
{
  namespace
  {
    using status = result<void, schema_error>;
    using index_result = result<value_index, schema_error>;

    /**
     * @brief Translate a document access or construction failure.
     * @par Requirements
     * P047; J5-043.
     * @par ID SWU-SCHEMA-195
     */
    schema_error failure_of(const error* failure) noexcept
    {
      return (failure == nullptr) ? schema_error::of(schema_error_code::internal_error)
                                  : schema_error::from_storage(*failure);
    }

    /**
     * @brief Return a keyword-shape failure at value.
     * @par Requirements
     * P047; J5-054.
     * @par ID SWU-SCHEMA-196
     */
    schema_error shape_at(const value_view& value) noexcept
    {
      const auto span = value.source_span();
      return schema_error::invalid(invalid_schema_reason::keyword_shape,
                                   span.has_value() ? *span.value_if() : std::nullopt);
    }

    /**
     * @brief Return the type_bit spelled by a decoded string, or zero for an unknown name.
     * @par Requirements
     * P047; J5-057.
     * @par ID SWU-SCHEMA-197
     */
    std::uint8_t type_from_name(const decoded_string& name) noexcept
    {
      constexpr std::array<std::pair<std::string_view, type_bit>, 7> c_names{{{"null", type_bit::null_type},
                                                                              {"boolean", type_bit::boolean_type},
                                                                              {"object", type_bit::object_type},
                                                                              {"array", type_bit::array_type},
                                                                              {"number", type_bit::number_type},
                                                                              {"string", type_bit::string_type},
                                                                              {"integer", type_bit::integer_type}}};
      for (const auto& entry : c_names)
      {
        bool equal = name.size() == entry.first.size();
        std::size_t index = 0U;
        while (equal && (index < entry.first.size()))
        {
          const auto unit = name.unit_at(index);
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA rule: RULE-7-0-5 cpp/misra/no-signedness-change-from-promotion
          // MISRA justification: Comparisons and arithmetic on std::uint16_t code units promote to int without
          // changing the value; the operands are unsigned values below 0x110000 and the results are used as such.
          // URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and code units by
          // value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.33)
          equal = unit.has_value() && (*unit.value_if() == static_cast<std::uint16_t>(entry.first[index]));
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.154)
          ++index;
        }
        if (equal)
        {
          return static_cast<std::uint8_t>(entry.second);
        }
      }
      return 0U;
    }

    /**
     * @brief Return the type_bit of one type-name value, or zero when it is no known name.
     * @par Requirements
     * P047; J5-057.
     * @par ID SWU-SCHEMA-198
     */
    std::uint8_t type_of_name_value(const value_view& value) noexcept
    {
      const auto text = value.as_string();
      return text.has_value() ? type_from_name(**text.value_if()) : static_cast<std::uint8_t>(0U);
    }

    /**
     * @brief Copy a decoded string into storage.
     * @par Requirements
     * P047; J5-066.
     * @par ID SWU-SCHEMA-199
     */
    result<decoded_string, schema_error> copy_text(const decoded_string& text, const storage_callbacks& storage,
                                                   pod_vector<std::uint16_t>& units) noexcept
    {
      using text_result = result<decoded_string, schema_error>;
      units.truncate(0U);
      std::size_t index = 0U;
      while (index < text.size())
      {
        const auto unit = text.unit_at(index);
        if (!unit.has_value())
        {
          return text_result::failure(failure_of(unit.error_if()));
        }
        const auto pushed = units.push_back(*unit.value_if());
        if (!pushed.has_value())
        {
          return text_result::failure(*pushed.error_if());
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.153)
        ++index;
      }
      auto copied = decoded_string::copy_units(storage, units.writable(), units.size());
      if (!copied.has_value())
      {
        return text_result::failure(failure_of(copied.error_if()));
      }
      // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
      // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
      // once.
      // codeql::misra_deviation_next_line(DEV-108.11)
      return text_result::success(std::move(*copied.value_if()));
    }

    /**
     * @brief Copy a number's spelling into storage.
     * @par Requirements
     * P047; J5-059; J5-066.
     * @par ID SWU-SCHEMA-200
     */
    index_result copy_number(const owned_number& number, document& target, const storage_callbacks& storage) noexcept
    {
      const input_view spelling = number.spelling();
      auto copied = owned_number::copy_spelling(spelling, syntax_mode::json5, storage, spelling.size);
      if (!copied.has_value())
      {
        return index_result::failure(failure_of(copied.error_if()));
      }
      // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
      // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
      // once.
      // codeql::misra_deviation_next_line(DEV-108.10)
      auto made = document_access::make_number(target, std::move(*copied.value_if()));
      return made.has_value() ? index_result::success(*made.value_if())
                              : index_result::failure(failure_of(made.error_if()));
    }

    /**
     * @brief Create the target value for one source value (containers are created empty).
     * @par Requirements
     * P047; J5-061; J5-066.
     * @par ID SWU-SCHEMA-201
     */
    index_result make_copy(const value_view& source, document& target, const storage_callbacks& storage,
                           pod_vector<std::uint16_t>& units) noexcept
    {
      const auto kind = source.kind();
      if (!kind.has_value())
      {
        return index_result::failure(failure_of(kind.error_if()));
      }
      if (*kind.value_if() == value_kind::null)
      {
        auto null = document_access::make_null(target);
        return null.has_value() ? index_result::success(*null.value_if())
                                : index_result::failure(failure_of(null.error_if()));
      }
      if (*kind.value_if() == value_kind::boolean)
      {
        const auto value = source.as_boolean();
        if (!value.has_value())
        {
          return index_result::failure(failure_of(value.error_if()));
        }
        auto boolean = document_access::make_boolean(target, *value.value_if());
        return boolean.has_value() ? index_result::success(*boolean.value_if())
                                   : index_result::failure(failure_of(boolean.error_if()));
      }
      if (*kind.value_if() == value_kind::number)
      {
        const auto number = source.as_number();
        return number.has_value() ? copy_number(**number.value_if(), target, storage)
                                  : index_result::failure(failure_of(number.error_if()));
      }
      if (*kind.value_if() == value_kind::string)
      {
        const auto text = source.as_string();
        if (!text.has_value())
        {
          return index_result::failure(failure_of(text.error_if()));
        }
        auto copied = copy_text(**text.value_if(), storage, units);
        if (!copied.has_value())
        {
          return index_result::failure(*copied.error_if());
        }
        // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
        // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
        // once.
        // codeql::misra_deviation_next_line(DEV-108.9)
        auto string = document_access::make_string(target, std::move(*copied.value_if()));
        return string.has_value() ? index_result::success(*string.value_if())
                                  : index_result::failure(failure_of(string.error_if()));
      }
      auto container = (*kind.value_if() == value_kind::array) ? document_access::make_array(target)
                                                               : document_access::make_object(target);
      return container.has_value() ? index_result::success(*container.value_if())
                                   : index_result::failure(failure_of(container.error_if()));
    }

    /**
     * @brief One container being copied: its remaining source children and its copy.
     * @par Requirements
     * P047; J5-048.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.40)
    struct copy_frame
    {
        value_iterator children{};
        value_index target{};
        bool object{};
    };

    /**
     * @brief Charge one compile step for a copied value.
     * @par Requirements
     * P047; J5-047.
     * @par ID SWU-SCHEMA-202
     */
    status charge_copy(std::size_t& steps, std::size_t step_limit) noexcept
    {
      if (steps >= step_limit)
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_compile_steps));
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.152)
      ++steps;
      return status::success();
    }

    /**
     * @brief Push a frame for a copied container, or do nothing for a scalar.
     * @par Requirements
     * P047; J5-048.
     * @par ID SWU-SCHEMA-203
     */
    status open_copy(const value_view& source, value_index copy, pod_vector<copy_frame>& stack) noexcept
    {
      const auto kind = source.kind();
      if (!kind.has_value())
      {
        return status::failure(failure_of(kind.error_if()));
      }
      if ((*kind.value_if() != value_kind::array) && (*kind.value_if() != value_kind::object))
      {
        return status::success();
      }
      auto children = source.children();
      if (!children.has_value())
      {
        return status::failure(failure_of(children.error_if()));
      }
      return stack.push_back(copy_frame{*children.value_if(), copy, *kind.value_if() == value_kind::object});
    }

    /**
     * @brief Attach a copied child to its container (with a copied name for an object member).
     * @par Requirements
     * P047; J5-061.
     * @par ID SWU-SCHEMA-204
     */
    status attach_copy(const copy_frame& frame, const value_view& child, value_index copy, document& target,
                       const storage_callbacks& storage, pod_vector<std::uint16_t>& units) noexcept
    {
      if (!frame.object)
      {
        const auto attached = document_access::append_element(target, frame.target, copy);
        return attached.has_value() ? status::success() : status::failure(failure_of(attached.error_if()));
      }
      const auto name = child.member_name();
      if (!name.has_value())
      {
        return status::failure(failure_of(name.error_if()));
      }
      auto copied = copy_text(**name.value_if(), storage, units);
      if (!copied.has_value())
      {
        return status::failure(*copied.error_if());
      }
      // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
      // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
      // once.
      // codeql::misra_deviation_next_line(DEV-108.8)
      const auto attached = document_access::append_member(target, frame.target, std::move(*copied.value_if()), copy);
      return attached.has_value() ? status::success() : status::failure(failure_of(attached.error_if()));
    }
  } // namespace

  /**
   * @brief Check that a value is an array of distinct strings (required, dependency lists).
   * @par Requirements
   * P051; J5-054.
   * @par ID SWU-SCHEMA-293
   */
  result<void, schema_error> check_name_list(const value_view& value) noexcept
  {
    const auto kind = value.kind();
    auto outer = value.children();
    if (!kind.has_value() || (*kind.value_if() != value_kind::array) || !outer.has_value())
    {
      return status::failure(shape_at(value));
    }
    for (;;)
    {
      const auto first = outer.value_if()->next();
      if (!first.has_value() || !first.value_if()->has_value())
      {
        return first.has_value() ? status::success() : status::failure(failure_of(first.error_if()));
      }
      const auto first_text = (*first.value_if())->as_string();
      if (!first_text.has_value())
      {
        return status::failure(shape_at(**first.value_if()));
      }
      value_iterator later = *outer.value_if();
      for (;;)
      {
        const auto second = later.next();
        if (!second.has_value())
        {
          return status::failure(failure_of(second.error_if()));
        }
        if (!second.value_if()->has_value())
        {
          break;
        }
        const auto second_text = (*second.value_if())->as_string();
        const auto equal = second_text.has_value()
                             ? decoded_name_equal(**first_text.value_if(), **second_text.value_if())
                             : result<bool>::success(false);
        if (!equal.has_value() || *equal.value_if())
        {
          return status::failure(shape_at(**second.value_if()));
        }
      }
    }
  }

  /**
   * @brief Copy the array-form entries of a `dependencies` object (name -> required names).
   * @par Requirements
   * P051; J5-054; J5-057; J5-066.
   * @par ID SWU-SCHEMA-292
   */
  result<value_index, schema_error> copy_dependency_arrays(const value_view& dependencies, document& target,
                                                           const storage_callbacks& storage, std::size_t& steps,
                                                           std::size_t step_limit) noexcept
  {
    auto object = document_access::make_object(target);
    auto members = dependencies.children();
    if (!object.has_value() || !members.has_value())
    {
      return index_result::failure(failure_of(object.has_value() ? members.error_if() : object.error_if()));
    }
    pod_vector<std::uint16_t> units{storage};
    for (;;)
    {
      const auto member = members.value_if()->next();
      if (!member.has_value() || !member.value_if()->has_value())
      {
        return member.has_value() ? index_result::success(*object.value_if())
                                  : index_result::failure(failure_of(member.error_if()));
      }
      const value_view& entry = **member.value_if();
      const auto kind = entry.kind();
      if (!kind.has_value() || (*kind.value_if() != value_kind::array))
      {
        continue;
      }
      const auto checked = check_name_list(entry);
      const auto copy = checked.has_value() ? copy_value(entry, target, storage, steps, step_limit)
                                            : index_result::failure(*checked.error_if());
      const auto attached = copy.has_value() ? attach_copy(copy_frame{value_iterator{}, *object.value_if(), true},
                                                           entry, *copy.value_if(), target, storage, units)
                                             : status::failure(*copy.error_if());
      if (!attached.has_value())
      {
        return index_result::failure(*attached.error_if());
      }
    }
  }

  /**
   * @brief Copy the members of a schema object whose names keep() accepts into a new object.
   * @par Requirements
   * P053; J5-058; J5-062; J5-066.
   * @par ID SWU-SCHEMA-315
   */
  result<value_index, schema_error> copy_members_if(const value_view& object, member_filter keep, document& target,
                                                    const storage_callbacks& storage, std::size_t& steps,
                                                    std::size_t step_limit, std::size_t& copied) noexcept
  {
    copied = 0U;
    auto copy = document_access::make_object(target);
    auto members = object.children();
    if (!copy.has_value() || !members.has_value())
    {
      return index_result::failure(failure_of(copy.has_value() ? members.error_if() : copy.error_if()));
    }
    pod_vector<std::uint16_t> units{storage};
    for (;;)
    {
      const auto member = members.value_if()->next();
      if (!member.has_value() || !member.value_if()->has_value())
      {
        return member.has_value() ? index_result::success(*copy.value_if())
                                  : index_result::failure(failure_of(member.error_if()));
      }
      const value_view& entry = **member.value_if();
      const auto name = entry.member_name();
      if (!name.has_value())
      {
        return index_result::failure(failure_of(name.error_if()));
      }
      if (!keep(**name.value_if()))
      {
        continue;
      }
      const auto value = copy_value(entry, target, storage, steps, step_limit);
      const auto attached = value.has_value() ? attach_copy(copy_frame{value_iterator{}, *copy.value_if(), true}, entry,
                                                            *value.value_if(), target, storage, units)
                                              : status::failure(*value.error_if());
      if (!attached.has_value())
      {
        return index_result::failure(*attached.error_if());
      }
      ++copied;
    }
  }

  /**
   * @brief Parse a `type` keyword value into a mask of type_bit values.
   * @par Requirements
   * P047; J5-054; J5-057.
   * @par ID SWU-SCHEMA-193
   */
  result<std::uint8_t, schema_error> parse_type_keyword(const value_view& value) noexcept
  {
    using mask_result = result<std::uint8_t, schema_error>;
    const std::uint8_t single = type_of_name_value(value);
    if (single != 0U)
    {
      return mask_result::success(single);
    }
    auto elements = value.children();
    const auto kind = value.kind();
    if (!kind.has_value() || (*kind.value_if() != value_kind::array) || !elements.has_value())
    {
      return mask_result::failure(shape_at(value));
    }
    std::uint8_t mask = 0U;
    for (;;)
    {
      const auto element = elements.value_if()->next();
      if (!element.has_value())
      {
        return mask_result::failure(failure_of(element.error_if()));
      }
      if (!element.value_if()->has_value())
      {
        break;
      }
      const std::uint8_t bit = type_of_name_value(**element.value_if());
      if ((bit == 0U) || has_flag(mask, bit))
      {
        return mask_result::failure(shape_at(**element.value_if()));
      }
      mask = with_flag(mask, bit);
    }
    return (mask != 0U) ? mask_result::success(mask) : mask_result::failure(shape_at(value));
  }

  /**
   * @brief Deep-copy a schema value (const or enum) into the compiled schema's value document.
   * @par Requirements
   * P047; J5-061; J5-066; ADR 0006 decision 4.
   * @par ID SWU-SCHEMA-194
   */
  result<value_index, schema_error> copy_value(const value_view& source, document& target,
                                               const storage_callbacks& storage, std::size_t& steps,
                                               std::size_t step_limit) noexcept
  {
    pod_vector<copy_frame> stack{storage};
    pod_vector<std::uint16_t> units{storage};
    const auto charged = charge_copy(steps, step_limit);
    if (!charged.has_value())
    {
      return index_result::failure(*charged.error_if());
    }
    const auto root = make_copy(source, target, storage, units);
    if (!root.has_value())
    {
      return root;
    }
    const auto opened = open_copy(source, *root.value_if(), stack);
    if (!opened.has_value())
    {
      return index_result::failure(*opened.error_if());
    }
    while (stack.size() != 0U)
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.151)
      copy_frame frame = stack.get(stack.size() - 1U);
      const auto next = frame.children.next();
      if (!next.has_value())
      {
        return index_result::failure(failure_of(next.error_if()));
      }
      if (!next.value_if()->has_value())
      {
        stack.pop_back();
        continue;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.150)
      stack.set(stack.size() - 1U, frame);
      const value_view child = **next.value_if();
      auto step = charge_copy(steps, step_limit);
      const auto copy =
        step.has_value() ? make_copy(child, target, storage, units) : index_result::failure(*step.error_if());
      if (!copy.has_value())
      {
        return copy;
      }
      auto attached = attach_copy(frame, child, *copy.value_if(), target, storage, units);
      if (attached.has_value())
      {
        attached = open_copy(child, *copy.value_if(), stack);
      }
      if (!attached.has_value())
      {
        return index_result::failure(*attached.error_if());
      }
    }
    return root;
  }
} // namespace adaptive_json5::detail
