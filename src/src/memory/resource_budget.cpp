// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/memory/resource_budget.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  status_result resource_budget::check_addition(std::size_t current, std::size_t additional, std::size_t limit) noexcept
  {
    // Subtraction avoids overflowing a speculative current + additional.
    if ((current > limit) || (additional > (limit - current)))
    {
      return status_result::failure(error::without_location(error_code::resource_limit));
    }

    return status_result::success();
  }

  resource_budget::resource_budget(const resource_limits& limits) noexcept : m_limits(limits) {}

  status_result resource_budget::check_input(std::size_t bytes) const noexcept
  {
    return check_addition(c_no_resources, bytes, m_limits.max_input_bytes);
  }

  status_result resource_budget::check_number(std::size_t bytes, std::size_t exponent_digits) const noexcept
  {
    const auto spelling = check_addition(c_no_resources, bytes, m_limits.max_number_bytes);
    if (!spelling.has_value())
    {
      return status_result::failure(*spelling.error_if());
    }

    return check_addition(c_no_resources, exponent_digits, m_limits.max_exponent_digits);
  }

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

  status_result resource_budget::enter_container() noexcept
  {
    const auto entered = check_addition(m_depth, c_single_resource, m_limits.max_depth);
    if (!entered.has_value())
    {
      return status_result::failure(*entered.error_if());
    }

    ++m_depth;
    return status_result::success();
  }

  status_result resource_budget::leave_container() noexcept
  {
    if (m_depth == c_no_resources)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    --m_depth;
    return status_result::success();
  }

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

  status_result resource_budget::release_string_units(std::size_t units) noexcept
  {
    if (units > m_string_units)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    m_string_units -= units;
    return status_result::success();
  }

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

  status_result resource_budget::release_diagnostic() noexcept
  {
    if (m_diagnostics == c_no_resources)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    --m_diagnostics;
    return status_result::success();
  }
} // namespace adaptive_json5
