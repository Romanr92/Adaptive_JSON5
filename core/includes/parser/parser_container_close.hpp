#ifndef PARSER_CONTAINER_CLOSE_HPP_
#define PARSER_CONTAINER_CLOSE_HPP_

#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "lexer/punctuation.hpp"
#include "memory/work_budget.hpp"
#include "parser/parser_frame.hpp"
#include "parser/parser_stack.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Check that a closing token may close a container in the given grammar state.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-041; J5-043.
     * @param state Grammar state of the innermost open container.
     * @param closing Recognized right bracket or right brace.
     * @param mode Selected syntax mode.
     * @return Success, or a syntax error located at the closing token.
     * @note Shared by the counting pre-pass and close_container, so both report the same
     * errors (ADR 0003, decision 7). Performs no allocation and charges no work.
     */
    [[nodiscard]] status_result check_container_close(parser_state state, const punctuation_token& closing,
                                                      syntax_mode mode) noexcept;

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
