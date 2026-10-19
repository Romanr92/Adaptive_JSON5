#ifndef PARSER_DUPLICATE_HPP_
#define PARSER_DUPLICATE_HPP_

#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

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
                                        source_span repeated_span, work_budget& work) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_DUPLICATE_HPP_ */
