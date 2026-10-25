#ifndef PARSER_WORK_HPP_
#define PARSER_WORK_HPP_

#include "adaptive_json5/result.hpp"
#include "memory/work_budget.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Work charged for constructing one document record.
     * @par Requirements
     * P031; P040; J5-047; J5-048; J5-049.
     * @note One unit creates the record. Record storage grows by doubling, so all
     * relocations over a whole parse move fewer than two records per record created;
     * two further units pay for that amortized relocation. The total charge therefore
     * bounds the real work, and grows linearly with the number of values instead of
     * charging the whole document size on every construction.
     */
    constexpr std::size_t c_record_construction_work = 3;

    /**
     * @brief Work charged for attaching one completed value to its open parent.
     * @par Requirements
     * P033; P040; J5-048; J5-049.
     * @note One unit for the relationship update and one for the cycle check. Open
     * containers stay detached until they close, so the check inspects only the parent.
     */
    constexpr std::size_t c_attachment_work = 2;

    /**
     * @brief Charge one completed lexical scan by the bytes it actually examined.
     * @par Requirements
     * P031; P040; J5-047; J5-048; J5-049.
     * @param work Shared work budget for the parse operation.
     * @param start_offset Original byte offset where the scan began.
     * @param end_offset Original byte offset where the scan stopped; not below start_offset.
     * @return Success or resource_limit without changing consumed work.
     * @note Charges end_offset - start_offset plus one lookahead byte. Scanners read
     * at most one byte beyond what they consume, so the total charge grows linearly
     * with the input. Call it after a successful scan and before its result changes
     * the document or cursor, so a failed charge still publishes nothing; the work
     * spent before the failure is bounded by that single scan.
     * An end_offset below start_offset is wrong_type_or_access.
     */
    [[nodiscard]] status_result charge_scanned(work_budget& work, std::size_t start_offset,
                                               std::size_t end_offset) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_WORK_HPP_ */
