#ifndef PARSER_CONTAINER_CLOSE_HPP_
#define PARSER_CONTAINER_CLOSE_HPP_

#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_stack.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Complete and remove the innermost open-container frame.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-041; J5-048; J5-049.
     * @param destination Document owning the frame's container.
     * @param frames Stack of valid open-container frames.
     * @param closing Recognized one-byte right bracket or right brace.
     * @param mode Selected syntax mode.
     * @param input_size Original input size in bytes.
     * @param work Shared work budget for this parse operation.
     * @return Completed container index or an explicit failure.
     * @note The caller must maintain valid frame/container correspondence.
     * Failure preserves the stack and existing source-span metadata.
     * Successful closure records the span and removes exactly one frame.
     * Does not advance a cursor, attach the value or establish a root.
     * The caller releases depth after success; value/member reservations remain.
     * Performs no allocation and charges one work unit after validation.
     * Successful work charges are not refunded after later failure.
     */
    [[nodiscard]] result<value_index> close_container(document& destination, parser_stack& frames,
                                                      const punctuation_token& closing, syntax_mode mode,
                                                      std::size_t input_size, work_budget& work) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_CONTAINER_CLOSE_HPP_ */
