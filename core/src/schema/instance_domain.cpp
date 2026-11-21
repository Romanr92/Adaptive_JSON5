#include "schema/instance_domain.hpp"

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/schema_work.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace adaptive_json5::detail
{
  namespace
  {
    using status = result<void, schema_error>;

    /**
     * @brief One open container: its child iterator and whether its children are members.
     * @par Requirements
     * P043; J5-048.
     */
    struct stack_frame
    {
        value_iterator children;
        bool object;
    };

    static_assert(std::is_trivially_destructible_v<stack_frame>,
                  "Stack frames are released without running destructors");

    constexpr std::uint16_t c_high_surrogate_first = 0xD800U;
    constexpr std::uint16_t c_high_surrogate_last = 0xDBFFU;
    constexpr std::uint16_t c_low_surrogate_first = 0xDC00U;
    constexpr std::uint16_t c_low_surrogate_last = 0xDFFFU;

    /**
     * @brief Translate a document access failure into a schema error.
     * @par Requirements
     * P043; J5-043.
     * @par ID SWU-SCHEMA-021
     */
    status access_failure(const error* failure) noexcept
    {
      if (failure == nullptr)
      {
        return status::failure(schema_error::of(schema_error_code::internal_error));
      }
      return status::failure(schema_error::from_storage(*failure));
    }

    /**
     * @brief Report whether a decoded UTF-16 string holds an unpaired surrogate.
     * @par Requirements
     * P043; J5-061; ADR 0006 decision 6.
     * @return The verdict, or the access failure of a unit read.
     * @par ID SWU-SCHEMA-022
     */
    result<bool> has_unpaired_surrogate(const decoded_string& text) noexcept
    {
      std::size_t position = 0U;
      while (position < text.size())
      {
        const auto unit = text.unit_at(position);
        if (!unit.has_value())
        {
          return result<bool>::failure(*unit.error_if());
        }
        const std::uint16_t value = *unit.value_if();
        if ((value >= c_low_surrogate_first) && (value <= c_low_surrogate_last))
        {
          return result<bool>::success(true);
        }
        if ((value >= c_high_surrogate_first) && (value <= c_high_surrogate_last))
        {
          if ((position + 1U) >= text.size())
          {
            return result<bool>::success(true);
          }
          const auto next = text.unit_at(position + 1U);
          if (!next.has_value())
          {
            return result<bool>::failure(*next.error_if());
          }
          const std::uint16_t low = *next.value_if();
          if ((low < c_low_surrogate_first) || (low > c_low_surrogate_last))
          {
            return result<bool>::success(true);
          }
          position += 2U;
        }
        else
        {
          ++position;
        }
      }
      return result<bool>::success(false);
    }

    /**
     * @brief Return a value's source span, or nullopt when unknown.
     * @par Requirements
     * P043; J5-044; J5-064.
     * @par ID SWU-SCHEMA-023
     */
    std::optional<source_span> value_span(const value_view& value) noexcept
    {
      const auto span = value.source_span();
      return span.has_value() ? *span.value_if() : std::nullopt;
    }

    /**
     * @brief Return a member's name source span, or nullopt when unknown.
     * @par Requirements
     * P043; J5-044; J5-064.
     * @par ID SWU-SCHEMA-024
     */
    std::optional<source_span> name_span(const value_view& member) noexcept
    {
      const auto span = member.member_name_source_span();
      return span.has_value() ? *span.value_if() : std::nullopt;
    }

    /**
     * @brief Reject a decoded string with an unpaired surrogate at range.
     * @par Requirements
     * P043; J5-061; ADR 0006 decision 6.
     * @par ID SWU-SCHEMA-025
     */
    status check_text(const decoded_string& text, std::optional<source_span> range) noexcept
    {
      const auto unpaired = has_unpaired_surrogate(text);
      if (!unpaired.has_value())
      {
        return access_failure(unpaired.error_if());
      }
      if (*unpaired.value_if())
      {
        return status::failure(schema_error::unsupported(unsupported_instance_reason::unpaired_surrogate, range));
      }
      return status::success();
    }

    /**
     * @brief Charge one step and check one value's own content (not its children).
     * @par Requirements
     * P043; J5-059; J5-061; ADR 0006 decision 6.
     * @param value Value to check.
     * @param in_object True when value is an object member, whose name is checked first.
     * @param work Step accounting.
     * @return success, unsupported_instance, or an operational failure.
     * @par ID SWU-SCHEMA-026
     */
    status check_value(const value_view& value, bool in_object, schema_work& work) noexcept
    {
      const auto stepped = work.step();
      if (!stepped.has_value())
      {
        return stepped;
      }

      if (in_object)
      {
        const auto name = value.member_name();
        if (!name.has_value())
        {
          return access_failure(name.error_if());
        }
        const auto name_checked = check_text(**name.value_if(), name_span(value));
        if (!name_checked.has_value())
        {
          return name_checked;
        }
      }

      const auto kind = value.kind();
      if (!kind.has_value())
      {
        return access_failure(kind.error_if());
      }

      switch (*kind.value_if())
      {
      case value_kind::string:
      {
        const auto text = value.as_string();
        if (!text.has_value())
        {
          return access_failure(text.error_if());
        }
        return check_text(**text.value_if(), value_span(value));
      }
      case value_kind::number:
      {
        const auto number = value.as_number();
        if (!number.has_value())
        {
          return access_failure(number.error_if());
        }
        const number_kind spelling = (*number.value_if())->kind();
        if ((spelling == number_kind::infinity) || (spelling == number_kind::nan))
        {
          return status::failure(
            schema_error::unsupported(unsupported_instance_reason::non_finite_number, value_span(value)));
        }
        return status::success();
      }
      case value_kind::null:
      case value_kind::boolean:
      case value_kind::array:
      case value_kind::object:
        return status::success();
      default:
        return status::failure(schema_error::of(schema_error_code::internal_error));
      }
    }

    /**
     * @brief Reject an object that repeats a decoded member name.
     * @par Requirements
     * P043; J5-061; ADR 0006 decision 6.
     * @return success, unsupported_instance (duplicate_name) at the first repeated name in
     * source order, or an operational failure. One step per name comparison.
     * @par ID SWU-SCHEMA-027
     */
    status check_unique_names(const value_view& object, schema_work& work) noexcept
    {
      auto outer = object.children();
      if (!outer.has_value())
      {
        return access_failure(outer.error_if());
      }
      for (;;)
      {
        const auto first = outer.value_if()->next();
        if (!first.has_value())
        {
          return access_failure(first.error_if());
        }
        if (!first.value_if()->has_value())
        {
          return status::success();
        }
        const value_view& first_member = **first.value_if();
        const auto first_name = first_member.member_name();
        if (!first_name.has_value())
        {
          return access_failure(first_name.error_if());
        }

        value_iterator later = *outer.value_if();
        for (;;)
        {
          const auto second = later.next();
          if (!second.has_value())
          {
            return access_failure(second.error_if());
          }
          if (!second.value_if()->has_value())
          {
            break;
          }
          const auto stepped = work.step();
          if (!stepped.has_value())
          {
            return stepped;
          }
          const value_view& second_member = **second.value_if();
          const auto second_name = second_member.member_name();
          if (!second_name.has_value())
          {
            return access_failure(second_name.error_if());
          }
          const auto equal = decoded_name_equal(**first_name.value_if(), **second_name.value_if());
          if (!equal.has_value())
          {
            return access_failure(equal.error_if());
          }
          if (*equal.value_if())
          {
            return status::failure(
              schema_error::unsupported(unsupported_instance_reason::duplicate_name, name_span(second_member)));
          }
        }
      }
    }

    /**
     * @brief Fixed-capacity container stack in caller work storage.
     * @par Requirements
     * P043; J5-048; J5-065.
     * @note Frames are trivially destructible; the block is released by owned_block.
     */
    class frame_stack
    {
      private:
        owned_block m_block{};
        std::size_t m_capacity{};
        std::size_t m_size{};

      public:
        /**
         * @brief Allocate room for capacity frames.
         * @par Requirements
         * P043; J5-046; J5-065.
         * @par ID SWU-SCHEMA-028
         */
        [[nodiscard]] status reserve(const storage_callbacks& storage, std::size_t capacity) noexcept
        {
          auto block = allocate_array(storage, capacity, sizeof(stack_frame), alignof(stack_frame));
          if (!block.has_value())
          {
            return status::failure(schema_error::from_storage(*block.error_if()));
          }
          m_block = std::move(*block.value_if());
          m_capacity = capacity;
          return status::success();
        }

        /**
         * @brief Push a frame; the caller checked the depth limit against capacity.
         * @par Requirements
         * P043; J5-048.
         * @par ID SWU-SCHEMA-029
         */
        [[nodiscard]] status push(const stack_frame& frame) noexcept
        {
          if (m_size >= m_capacity)
          {
            return status::failure(schema_error::of(schema_error_code::internal_error));
          }
          void* const slot = static_cast<unsigned char*>(m_block.data()) + (m_size * sizeof(stack_frame));
          static_cast<void>(::new (slot) stack_frame(frame));
          ++m_size;
          return status::success();
        }

        /**
         * @brief Borrow the top frame; requires a nonempty stack.
         * @par Requirements
         * P043; J5-048.
         * @par ID SWU-SCHEMA-030
         */
        [[nodiscard]] stack_frame& top() noexcept
        {
          void* const slot = static_cast<unsigned char*>(m_block.data()) + ((m_size - 1U) * sizeof(stack_frame));
          return *std::launder(static_cast<stack_frame*>(slot));
        }

        /**
         * @brief Remove the top frame; requires a nonempty stack.
         * @par Requirements
         * P043; J5-048.
         * @par ID SWU-SCHEMA-031
         */
        void pop() noexcept
        {
          --m_size;
        }

        /**
         * @brief Return the number of frames, which equals the current nesting depth.
         * @par Requirements
         * P043; J5-048.
         * @par ID SWU-SCHEMA-032
         */
        [[nodiscard]] std::size_t size() const noexcept
        {
          return m_size;
        }
    };

    /**
     * @brief Check a container's names if it is an object and push its children.
     * @par Requirements
     * P043; J5-048; J5-061.
     * @return success (also for scalars, which push nothing), resource_limit (max_depth),
     * unsupported_instance (duplicate_name) or an operational failure.
     * @par ID SWU-SCHEMA-033
     */
    status enter(const value_view& value, frame_stack& stack, schema_work& work) noexcept
    {
      const auto kind = value.kind();
      if (!kind.has_value())
      {
        return access_failure(kind.error_if());
      }
      const bool object = *kind.value_if() == value_kind::object;
      if (!object && (*kind.value_if() != value_kind::array))
      {
        return status::success();
      }
      if (stack.size() >= work.budget().max_depth)
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_depth));
      }
      if (object)
      {
        const auto unique = check_unique_names(value, work);
        if (!unique.has_value())
        {
          return unique;
        }
      }
      const auto children = value.children();
      if (!children.has_value())
      {
        return access_failure(children.error_if());
      }
      return stack.push(stack_frame{*children.value_if(), object});
    }

    /**
     * @brief Report whether a value is an array or object.
     * @par Requirements
     * P043; J5-048.
     * @par ID SWU-SCHEMA-034
     */
    result<bool> is_container(const value_view& value) noexcept
    {
      const auto kind = value.kind();
      if (!kind.has_value())
      {
        return result<bool>::failure(*kind.error_if());
      }
      return result<bool>::success((*kind.value_if() == value_kind::array) || (*kind.value_if() == value_kind::object));
    }
  } // namespace

  /**
   * @brief Check that an instance is a JSON-compatible value before any assertion runs.
   * @par Requirements
   * P043; J5-048; J5-059; J5-061; J5-065; ADR 0006 decision 6.
   * @par ID SWU-SCHEMA-020
   */
  result<void, schema_error> check_instance_domain(const document& instance, schema_work& work,
                                                   const storage_callbacks& work_storage) noexcept
  {
    const auto root = instance.root();
    if (!root.has_value())
    {
      return access_failure(root.error_if());
    }
    const value_view& root_value = *root.value_if();

    const auto root_checked = check_value(root_value, false, work);
    if (!root_checked.has_value())
    {
      return root_checked;
    }
    const auto container = is_container(root_value);
    if (!container.has_value())
    {
      return access_failure(container.error_if());
    }
    if (!*container.value_if())
    {
      return status::success();
    }
    if (work.budget().max_depth == 0U)
    {
      return status::failure(schema_error::limit(schema_limit_reason::max_depth));
    }

    frame_stack stack{};
    const auto reserved = stack.reserve(work_storage, std::min(work.budget().max_depth, instance.size()));
    if (!reserved.has_value())
    {
      return reserved;
    }
    const auto entered = enter(root_value, stack, work);
    if (!entered.has_value())
    {
      return entered;
    }

    while (stack.size() != 0U)
    {
      stack_frame& frame = stack.top();
      const bool in_object = frame.object;
      const auto next = frame.children.next();
      if (!next.has_value())
      {
        return access_failure(next.error_if());
      }
      if (!next.value_if()->has_value())
      {
        stack.pop();
        continue;
      }
      const value_view& child = **next.value_if();
      const auto checked = check_value(child, in_object, work);
      if (!checked.has_value())
      {
        return checked;
      }
      const auto child_entered = enter(child, stack, work);
      if (!child_entered.has_value())
      {
        return child_entered;
      }
    }
    return status::success();
  }
} // namespace adaptive_json5::detail
