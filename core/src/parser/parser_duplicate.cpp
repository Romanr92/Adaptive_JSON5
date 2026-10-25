#include "parser/parser_duplicate.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_name_equal.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "common/checked_size.hpp"

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Reject a decoded name matching an existing member of one object.
     * @par Requirements
     * P036; J5-032; J5-033; J5-048; J5-049.
     * @param values Document containing previously completed members.
     * @param object Object whose direct members are inspected.
     * @param name Newly decoded name, not yet attached.
     * @param repeated_span Valid original source span of the new name token.
     * @param work Shared parser work budget.
     * @return Success when unique, duplicate failure with both locations,
     * or an explicit access/resource failure.
     * @note Requires parser-produced name spans on matching existing members.
     * Compares exact UTF-16 units without normalization or case folding.
     * Inspects only this object's direct members, in source order.
     * Changes no document, name or cursor state and performs no allocation.
     * Successfully charged work is not refunded on later failure.
     * The caller decides whether the selected policy requires this check.
     */
    status_result reject_duplicate_name(const document& values, value_index object, const decoded_string& name,
                                        source_span repeated_span, work_budget& work) noexcept
    {
      const auto inspection = work.charge(1);

      if (!inspection.has_value())
      {
        return status_result::failure(*inspection.error_if());
      }

      const auto parent = values.view(object);

      if (!parent.has_value())
      {
        return status_result::failure(*parent.error_if());
      }

      const auto kind = parent.value_if()->kind();

      if (!kind.has_value())
      {
        return status_result::failure(*kind.error_if());
      }

      // generic iteration also accepts arrays; duplicate-name checks required objects.
      if (*kind.value_if() != value_kind::object)
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      auto iterator = values.iterate(object);

      if (!iterator.has_value())
      {
        return status_result::failure(*iterator.error_if());
      }

      while (true)
      {
        // Charge before advancing, including the final end-of-iteration check.
        const auto step = work.charge(1);

        if (!step.has_value())
        {
          return status_result::failure(*step.error_if());
        }

        const auto next = iterator.value_if()->next();

        if (!next.has_value())
        {
          return status_result::failure(*next.error_if());
        }

        const auto& member = *next.value_if();

        if (!member.has_value())
        {
          return status_result::success();
        }

        const auto existing = member->member_name();

        if (!existing.has_value())
        {
          return status_result::failure(*existing.error_if());
        }

        const auto& previous_name = **existing.value_if();

        if (previous_name.size() != name.size())
        {
          continue;
        }

        // Equal length comparison can read one unit from each name per position.
        const auto comparison_units = checked_multiply(name.size(), 2);

        if (!comparison_units.has_value())
        {
          return status_result::failure(*comparison_units.error_if());
        }

        const auto comparison = work.charge(*comparison_units.value_if());

        if (!comparison.has_value())
        {
          return status_result::failure(*comparison.error_if());
        }

        const auto equal = decoded_name_equal(previous_name, name);

        if (!equal.has_value())
        {
          return status_result::failure(*equal.error_if());
        }

        if (!*equal.value_if())
        {
          continue;
        }

        const auto first_span = member->member_name_source_span();

        if (!first_span.has_value())
        {
          return status_result::failure(*first_span.error_if());
        }

        // Missing provenance must not produce a fabricated first occurrence.
        if (!first_span.value_if()->has_value())
        {
          return status_result::failure(error::without_location(error_code::wrong_type_or_access));
        }

        return status_result::failure(error::duplicate_member(**first_span.value_if(), repeated_span));
      }
    }
  } // namespace detail
} // namespace adaptive_json5
