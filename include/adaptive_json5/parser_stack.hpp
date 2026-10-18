#ifndef PARSER_STACK_HPP_
#define PARSER_STACK_HPP_

#include "checked_size.hpp"
#include "error.hpp"
#include "owned_block.hpp"
#include "parser_frame.hpp"
#include "result.hpp"
#include "storage.hpp"
#include "work_budget.hpp"

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Own a bounded stack of open-container frames.
     * @par Requirements
     * P033; J5-003; J5-046; J5-048; J5-049.
     * @note Provider state must outlive this stack.
     * Use accounted callbacks to include frame storage in owned-byte accounting.
     * The caller separately reserves and releases resource-budget depth.
     * Growth invalidates frame pointers.
     * Cleanup is iterative and requires no allocation or work charge.
     */
    class parser_stack
    {
      private:
      public:
    }
  } // namespace detail
} // namespace adaptive_json5

#endif /* PARSER_STACK_HPP_ */
