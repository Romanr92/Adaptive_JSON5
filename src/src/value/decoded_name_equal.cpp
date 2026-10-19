// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/value/decoded_name_equal.hpp"

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>

namespace adaptive_json5
{
  result<bool> decoded_name_equal(const decoded_string& left, const decoded_string& right) noexcept
  {
    if (left.size() != right.size())
    {
      return result<bool>::success(false);
    }

    for (std::size_t index{}; index < left.size(); ++index)
    {
      const auto left_unit = left.unit_at(index);

      if (!left_unit.has_value())
      {
        return result<bool>::failure(*left_unit.error_if());
      }

      const auto right_unit = right.unit_at(index);

      if (!right_unit.has_value())
      {
        return result<bool>::failure(*right_unit.error_if());
      }

      // Compare decoded units without transforming their content.
      if (*left_unit.value_if() != *right_unit.value_if())
      {
        return result<bool>::success(false);
      }
    }

    return result<bool>::success(true);
  }
} // namespace adaptive_json5
