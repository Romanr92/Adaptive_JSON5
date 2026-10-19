#ifndef PARSER_CONTAINER_OPEN_HPP_
#define PARSER_CONTAINER_OPEN_HPP_

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/lexer/punctuation.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/detail/parser/parser_frame.hpp"
#include "adaptive_json5/detail/parser/parser_stack.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Prepare a frame and create one detached empty container.
     * @par Requirements
     * P033; J5-013; J5-046; J5-048; J5-049.
     * @param destination Private document under construction.
     * @param frames Stack receiving the new open-container frame.
     * @param opening Recognized one-byte left bracket or left brace.
     * @param work Shared work budget for this parse operation.
     * @return New container index or an explicit failure.
     * @note The caller must reserve depth and value/member resources first.
     * On failure, the caller must release those reservations.
     * Depth remains reserved until closure or rollback.
     * Value/member reservations remain while their records exist.
     * Does not advance a cursor, attach the container or establish a root.
     * Failure preserves document records and existing frame contents.
     * Stack capacity and successfully charged work may change on failure.
     * Frame growth invalidates previously obtained frame pointers.
     * Record construction charges destination.size() plus one.
     * Frame pushing additionally follows parser_stack's charging rules.
     */
    result<value_index> open_container(document& destination, parser_stack& frames, const punctuation_token& opening,
                                       work_budget& work) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_CONTAINER_OPEN_HPP_ */
