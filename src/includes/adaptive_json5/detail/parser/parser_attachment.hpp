#ifndef PARSER_ATTACHMENT_HPP_
#define PARSER_ATTACHMENT_HPP_

#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Attach a completed value to its waiting parent frame.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-030; J5-031; J5-048; J5-049; J5-051.
     * @param destination Document owning both frame.container and child.
     * @param frame Parent frame currently expecting a value.
     * @param child Detached completed value belonging to destination.
     * @param work Shared work budget for the parse operation.
     * @return Success or an explicit state, work or attachment failure.
     * @note Performs no allocation.
     * Failure preserves document relationships, frame state and pending name.
     * Successful work charges are not refunded after attachment failure.
     * Charges destination.size() plus one before relationship inspection.
     * Success transfers an object member's pending name and expects a separator.
     * Does not establish a root, change the cursor or reserve resources.
     * The caller handles node/member/string accounting and duplicate policy.
     */
    status_result attach_parsed_value(document& destination, parser_frame& frame, value_index child,
                                      work_budget& work) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_ATTACHMENT_HPP_ */
