#ifndef PARSER_ATTACHMENT_HPP_
#define PARSER_ATTACHMENT_HPP_

#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Attach a completed value and retain its pending member-name span.
     * @par Requirements
     * P033; P034; P035; P036; J5-013; J5-030; J5-031; J5-032;
     * J5-041; J5-048; J5-049; J5-051.
     * @param destination Document owning both frame.container and child.
     * @param frame Parent frame currently expecting a value.
     * @param child Detached completed value belonging to destination.
     * @param work Shared work budget for the parse operation.
     * @param input_size Original input size used to validate member-name spans.
     * @return Success or an explicit state, work, range or attachment failure.
     * @note Performs no allocation.
     * Failure preserves relationships, frame state, pending name and name offsets.
     * Successful work charges are not refunded after attachment failure.
     * Charges c_attachment_work before relationship inspection; frame.container must
     * still be detached, as it is for every open container in the engine.
     * Success transfers an object's pending name and records its original span.
     * Does not establish a root, change the cursor or reserve resources.
     * The caller handles node/member/string accounting and duplicate policy.
     */
    [[nodiscard]] status_result attach_parsed_value(document& destination, parser_frame& frame, value_index child,
                                                    work_budget& work, std::size_t input_size) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_ATTACHMENT_HPP_ */
