#ifndef PARSER_WORK_HPP_
#define PARSER_WORK_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
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
     * @note One unit creates the record. Records are reserved exactly and never relocated
     * (ADR 0003, decision 7), so no relocation share is charged (step 8, E1). The charge grows
     * linearly with the number of values instead of charging the whole document size on every
     * construction.
     */
    constexpr std::size_t c_record_construction_work = 1;

    /**
     * @brief Work charged for attaching one completed value to its open parent.
     * @par Requirements
     * P033; P040; J5-048; J5-049.
     * @note One unit for the relationship update and one for the cycle check. Open
     * containers stay detached until they close, so the check inspects only the parent.
     */
    constexpr std::size_t c_attachment_work = 2;

    /**
     * @brief Bytes before a cut within which a scan may have looked at the cut.
     * @par Requirements
     * P040; J5-049.
     * @note A successful scan peeks at most two bytes past its end (a comment opener),
     * and decode_utf8 locates a truncated character at its first byte, at most three
     * bytes before the cut.
     */
    constexpr std::size_t c_cut_margin_bytes = 3;

    /**
     * @brief One lexical scan over the bytes the remaining work budget can pay for.
     * @par Requirements
     * P040; J5-049.
     * @note source reports original input offsets. Do not assign source to a
     * long-lived cursor; its view ends at view_end.
     */
    struct [[nodiscard]] limited_scan
    {
        cursor source;            // Scan cursor positioned at start_offset.
        std::size_t start_offset; // Original offset where the scan begins.
        std::size_t view_end;     // Exclusive original offset where visible bytes end.
        bool cut;                 // True when real input continues after view_end.
    };

    /**
     * @brief Start a scan limited to min(remaining input, remaining work) bytes.
     * @par Requirements
     * P040; J5-049.
     * @param input Complete borrowed input.
     * @param start_offset Original offset where the scan begins.
     * @param work Shared work budget; not charged here.
     * @return The limited scan, or wrong_type_or_access for an invalid input or offset.
     * @note Performs no allocation and reads no input bytes.
     */
    [[nodiscard]] result<limited_scan> begin_limited_scan(input_view input, std::size_t start_offset,
                                                          const work_budget& work) noexcept;

    /**
     * @brief Charge a successful or no-match scan, or fail when it reached the cut.
     * @par Requirements
     * P040; J5-049.
     * @param scan Scan started by begin_limited_scan.
     * @param end_offset Original offset where the scan stopped; start_offset for no-match.
     * @param work Shared work budget for the parse operation.
     * @return Success after charging end_offset - start_offset, resource_limit at
     * view_end when the scan stopped within c_cut_margin_bytes of a cut, or wrong_type_or_access for a reversed range.
     * @note Call before the scan's result changes the document or cursor.
     */
    [[nodiscard]] status_result finish_limited_scan(const limited_scan& scan, std::size_t end_offset,
                                                    work_budget& work) noexcept;

    /**
     * @brief Map a lexer failure that may have been caused by the cut.
     * @par Requirements
     * P040; J5-049.
     * @param scan Scan started by begin_limited_scan.
     * @param failure Error reported by the lexer.
     * @return resource_limit at view_end when the failure lies within
     * c_cut_margin_bytes of a cut, has a source range ending there, or has no location;
     * otherwise failure unchanged.
     */
    [[nodiscard]] error limited_scan_error(const limited_scan& scan, const error& failure) noexcept;

    /**
     * @brief Skip trivia within the remaining work budget and charge the bytes skipped.
     * @par Requirements
     * P040; J5-010; J5-011; J5-049.
     * @param source Long-lived cursor over input; advanced only after the scan is charged.
     * @param input Complete borrowed input underlying source.
     * @param mode Selected syntax.
     * @param work Shared work budget for the parse operation.
     * @return Success, a trivia error, or resource_limit when the budget ends inside the trivia.
     * @note Shared by the container engine and the primitive parser so both charge trivia alike.
     */
    [[nodiscard]] status_result skip_trivia_within_budget(cursor& source, input_view input, syntax_mode mode,
                                                          work_budget& work) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_WORK_HPP_ */
