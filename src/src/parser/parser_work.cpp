#include "adaptive_json5/detail/parser/parser_work.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Charge one completed lexical scan by the bytes it actually examined.
     * @par Requirements
     * P031; P040; J5-047; J5-048; J5-049.
     * @param work Shared work budget for the parse operation.
     * @param start_offset Original byte offset where the scan began.
     * @param end_offset Original byte offset where the scan stopped; not below start_offset.
     * @return Success or resource_limit without changing consumed work.
     * @note Charges the consumed bytes plus one lookahead byte.
     */
    status_result charge_scanned(work_budget& work, std::size_t start_offset, std::size_t end_offset) noexcept
    {
      // A cursor never moves backwards, so a reversed range is an internal error.
      if (end_offset < start_offset)
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      const auto units = checked_add(end_offset - start_offset, 1);

      if (!units.has_value())
      {
        return status_result::failure(*units.error_if());
      }

      return work.charge(*units.value_if());
    }
  } // namespace detail
} // namespace adaptive_json5
