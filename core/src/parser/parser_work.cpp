#include "parser/parser_work.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"
#include "lexer/trivia.hpp"
#include "memory/work_budget.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
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
    result<limited_scan> begin_limited_scan(input_view input, std::size_t start_offset,
                                            const work_budget& work) noexcept
    {
      if (((input.data == nullptr) && (input.size != 0)) || (start_offset > input.size))
      {
        return result<limited_scan>::failure(error::without_location(error_code::wrong_type_or_access));
      }

      const std::size_t input_left = input.size - start_offset;
      const std::size_t visible = (input_left < work.remaining()) ? input_left : work.remaining();

      // Start the view at the input's first byte so the cursor keeps original offsets.
      cursor source{input_view{input.data, start_offset + visible}};
      source.advance(start_offset);

      return result<limited_scan>::success(
        limited_scan{source, start_offset, start_offset + visible, visible < input_left});
    }

    /**
     * @brief Charge a successful or no-match scan, or fail when it reached the cut.
     * @par Requirements
     * P040; J5-049.
     * @param scan Scan started by begin_limited_scan.
     * @param end_offset Original offset where the scan stopped; start_offset for no-match.
     * @param work Shared work budget for the parse operation.
     * @return Success after charging end_offset - start_offset, resource_limit at
     * view_end when the scan stopped within c_cut_margin_bytes of a cut, or wrong_type_or_access for a reversed range.
     */
    status_result finish_limited_scan(const limited_scan& scan, std::size_t end_offset, work_budget& work) noexcept
    {
      // A cursor never moves backwards, so a reversed range is an internal error.
      if ((end_offset < scan.start_offset) || (end_offset > scan.view_end))
      {
        return status_result::failure(error::without_location(error_code::wrong_type_or_access));
      }

      // The lexer saw the cut as end of input, so its result may be a shortened token.
      if (scan.cut && ((end_offset + c_cut_margin_bytes) >= scan.view_end))
      {
        return status_result::failure(error::at_offset(error_code::resource_limit, scan.view_end));
      }

      return work.charge(end_offset - scan.start_offset);
    }

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
    error limited_scan_error(const limited_scan& scan, const error& failure) noexcept
    {
      if (!scan.cut)
      {
        return failure;
      }

      // A token that ran up to the cut may be reported at its start, so check its range end as well.
      const bool range_near_cut =
        failure.source_range.has_value() &&
        ((failure.source_range->byte_offset + failure.source_range->byte_length + c_cut_margin_bytes) >= scan.view_end);

      // Adding the offset instead of subtracting from view_end cannot underflow.
      const bool near_cut =
        (!failure.has_byte_offset) || ((failure.byte_offset + c_cut_margin_bytes) >= scan.view_end) || range_near_cut;

      return near_cut ? error::at_offset(error_code::resource_limit, scan.view_end) : failure;
    }

    /**
     * @brief Skip trivia within the remaining work budget and charge the bytes skipped.
     * @par Requirements
     * P040; J5-010; J5-011; J5-049.
     * @param source Long-lived cursor over input; advanced only after the scan is charged.
     * @param input Complete borrowed input underlying source.
     * @param mode Selected syntax.
     * @param work Shared work budget for the parse operation.
     * @return Success, a trivia error, or resource_limit when the budget ends inside the trivia.
     */
    status_result skip_trivia_within_budget(cursor& source, input_view input, syntax_mode mode,
                                            work_budget& work) noexcept
    {
      const auto started = begin_limited_scan(input, source.offset(), work);

      if (!started.has_value())
      {
        return status_result::failure(*started.error_if());
      }

      limited_scan scan = *started.value_if();
      const auto trivia = skip_trivia(scan.source, mode);

      if (!trivia.has_value())
      {
        // An unterminated comment at the cut may only mean the budget hid its end.
        return status_result::failure(limited_scan_error(scan, *trivia.error_if()));
      }

      const auto charged = finish_limited_scan(scan, scan.source.offset(), work);

      if (!charged.has_value())
      {
        return status_result::failure(*charged.error_if());
      }

      // Move the long-lived cursor; copying scan.source would keep its shortened view.
      source.advance(scan.source.offset() - source.offset());

      return status_result::success();
    }
  } // namespace detail
} // namespace adaptive_json5
