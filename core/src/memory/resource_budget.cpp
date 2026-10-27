#include "memory/resource_budget.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Check whether an additional amount fits without overflowing.
   * @par Requirements
   * P031; J5-047; J5-048.
   * @note Subtraction is evaluated only after validating current.
   */
  status_result resource_budget::check_addition(std::size_t current, std::size_t additional, std::size_t limit) noexcept
  {
    // Subtraction avoids overflowing a speculative current + additional.
    if ((current > limit) || (additional > (limit - current)))
    {
      return status_result::failure(error::without_location(error_code::resource_limit));
    }

    return status_result::success();
  }

  /**
   * @brief Begin accounting with no resources reserved.
   * @par Requirements
   * P031; J5-047.
   * @note Every size_t limit is supported, including zero.
   * Cross-limit inequalities are not inherently invalid configurations.
   */
  resource_budget::resource_budget(const resource_limits& limits) noexcept : m_limits(limits) {}

  /**
   * @brief Check the complete supplied input size before processing.
   * @par Requirements
   * P031; J5-047.
   * @note Repeated checks do not accumulate input bytes.
   */
  status_result resource_budget::check_input(std::size_t bytes) const noexcept
  {
    return check_addition(c_no_resources, bytes, m_limits.max_input_bytes);
  }

  /**
   * @brief Check one numeric spelling and its exponent digit count.
   * @par Requirements
   * P031; J5-047; J5-048.
   * @note The caller supplies zero exponent digits when no exponent exists.
   * This operation checks lengths, not numeric grammar or exponent magnitude.
   * Call during scanning before accepting a byte that exceeds either limit.
   */
  status_result resource_budget::check_number(std::size_t bytes, std::size_t exponent_digits) const noexcept
  {
    const auto spelling = check_addition(c_no_resources, bytes, m_limits.max_number_bytes);
    if (!spelling.has_value())
    {
      return status_result::failure(*spelling.error_if());
    }

    return check_addition(c_no_resources, exponent_digits, m_limits.max_exponent_digits);
  }

  /**
   * @brief Reserve one value and optionally its object-member entry.
   * @par Requirements
   * P031; J5-047.
   * @param member True when this value is a direct object member.
   * @note The root, array elements and detached nonmember values use false.
   * Both limits are checked before either counter changes.
   */
  status_result resource_budget::reserve_value(bool member) noexcept
  {
    const auto node = check_addition(m_nodes, c_single_resource, m_limits.max_nodes);
    if (!node.has_value())
    {
      return status_result::failure(*node.error_if());
    }

    if (member)
    {
      const auto entry = check_addition(m_members, c_single_resource, m_limits.max_members);
      if (!entry.has_value())
      {
        return status_result::failure(*entry.error_if());
      }
    }

    ++m_nodes;

    if (member)
    {
      ++m_members;
    }

    return status_result::success();
  }

  /**
   * @brief Release a matching value reservation during rollback.
   * @par Requirements
   * P031; J5-047.
   * @param member Must match the original reservation.
   * @note Callers must associate releases with their own reservations.
   * Invalid counter underflow is reported without changing either counter.
   */
  status_result resource_budget::release_value(bool member) noexcept
  {
    if ((m_nodes == c_no_resources) || (member && (m_members == c_no_resources)) || (!member && (m_nodes == m_members)))
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    --m_nodes;

    if (member)
    {
      --m_members;
    }

    return status_result::success();
  }

  /**
   * @brief Reserve one additional simultaneously open container.
   * @par Requirements
   * P031; J5-047; J5-048.
   * @note Call before growing parser storage for the new nesting level.
   */
  status_result resource_budget::enter_container() noexcept
  {
    const auto entered = check_addition(m_depth, c_single_resource, m_limits.max_depth);
    if (!entered.has_value())
    {
      return status_result::failure(*entered.error_if());
    }

    ++m_depth;

    // The pre-pass sizes the frame stack from the deepest point, not the final depth.
    if (m_depth > m_peak_depth)
    {
      m_peak_depth = m_depth;
    }

    return status_result::success();
  }

  /**
   * @brief Release one open-container reservation.
   * @par Requirements
   * P031; J5-047; J5-048.
   * @note Closing or rolling back a container releases its depth.
   */
  status_result resource_budget::leave_container() noexcept
  {
    if (m_depth == c_no_resources)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    --m_depth;
    return status_result::success();
  }

  /**
   * @brief Reserve additional decoded units for one string or member name.
   * @par Requirements
   * P031; J5-047.
   * @param current_units Units already reserved for this particular string.
   * @param additional_units New units to reserve before growing its storage.
   * @note current_units must describe this string's existing reservations.
   * Checks both per-string and aggregate limits before changing accounting.
   */
  status_result resource_budget::reserve_string_units(std::size_t current_units, std::size_t additional_units) noexcept
  {
    const auto individual = check_addition(current_units, additional_units, m_limits.max_string_units);
    if (!individual.has_value())
    {
      return status_result::failure(*individual.error_if());
    }

    const auto total = check_addition(m_string_units, additional_units, m_limits.max_total_string_units);
    if (!total.has_value())
    {
      return status_result::failure(*total.error_if());
    }

    m_string_units += additional_units;
    return status_result::success();
  }

  /**
   * @brief Release decoded-unit reservations during rollback.
   * @par Requirements
   * P031; J5-047.
   * @note units must belong to reservations owned by the caller.
   * Does not release any allocation; storage ownership is separate.
   */
  status_result resource_budget::release_string_units(std::size_t units) noexcept
  {
    if (units > m_string_units)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    m_string_units -= units;
    return status_result::success();
  }

  /**
   * @brief Report the decoded-unit allowance for the next string or member name.
   * @par Requirements
   * P031; J5-047.
   * @return min(max_string_units, max_total_string_units - reserved units).
   * @note Single source for the lexer limit; reserves nothing and changes no state.
   * Returns zero if the reserved total ever exceeded the aggregate limit.
   */
  std::size_t resource_budget::string_unit_allowance() const noexcept
  {
    // Validate before subtracting so a broken invariant cannot wrap around.
    if (m_string_units > m_limits.max_total_string_units)
    {
      return c_no_resources;
    }

    const std::size_t remaining = m_limits.max_total_string_units - m_string_units;

    return (m_limits.max_string_units < remaining) ? m_limits.max_string_units : remaining;
  }

  /**
   * @brief Reserve one retained diagnostic entry.
   * @par Requirements
   * P031; J5-047.
   * @note Call before allocating or publishing the entry.
   * A minimal result/error value does not consume a diagnostic entry.
   */
  status_result resource_budget::reserve_diagnostic() noexcept
  {
    const auto diagnostic = check_addition(m_diagnostics, c_single_resource, m_limits.max_diagnostics);
    if (!diagnostic.has_value())
    {
      return status_result::failure(*diagnostic.error_if());
    }

    ++m_diagnostics;
    return status_result::success();
  }

  /**
   * @brief Release one retained or rolled-back diagnostic reservation.
   * @par Requirements
   * P031; J5-047.
   */
  status_result resource_budget::release_diagnostic() noexcept
  {
    if (m_diagnostics == c_no_resources)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    --m_diagnostics;
    return status_result::success();
  }

  /**
   * @brief Report the number of value reservations currently held.
   * @par Requirements
   * P031; J5-046;
   * J5-047.
   * @note Changes no state.
   */
  std::size_t resource_budget::reserved_nodes() const noexcept
  {
    return m_nodes;
  }

  /**
   * @brief Report the deepest nesting reserved at any time.
   * @par Requirements
   * P031; J5-046; J5-048.

   * * @note Changes no state.
   */
  std::size_t resource_budget::peak_depth() const noexcept
  {
    return m_peak_depth;
  }
} // namespace adaptive_json5
