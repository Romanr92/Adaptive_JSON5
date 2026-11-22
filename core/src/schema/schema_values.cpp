#include "schema/schema_values.hpp"

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/pod_vector.hpp"
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
        for (std::size_t index = 0U; equal && (index < entry.first.size()); ++index)
        {
          const auto unit = name.unit_at(index);
          equal = unit.has_value() && (*unit.value_if() == static_cast<std::uint16_t>(entry.first[index]));
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
      for (std::size_t index = 0U; index < text.size(); ++index)
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
      }
      auto copied = decoded_string::copy_units(storage, units.writable(), units.size());
      if (!copied.has_value())
      {
        return text_result::failure(failure_of(copied.error_if()));
      }
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
      const auto attached = document_access::append_member(target, frame.target, std::move(*copied.value_if()), copy);
      return attached.has_value() ? status::success() : status::failure(failure_of(attached.error_if()));
    }
  } // namespace

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
      if ((bit == 0U) || ((mask & bit) != 0U))
      {
        return mask_result::failure(shape_at(**element.value_if()));
      }
      mask = static_cast<std::uint8_t>(mask | bit);
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
