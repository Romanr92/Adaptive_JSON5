#ifndef INTERNAL_LINK_HPP_
#define INTERNAL_LINK_HPP_

#include "adaptive_json5/error.hpp"

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Report a failure met while following a link the library itself recorded.
     * @par Requirements
     * J5-039; J5-043; issue #125.
     * @param failure Failure returned while resolving the recorded index.
     * @return internal_error when failure is wrong_type_or_access, because a dangling or
     * mistyped internal link can only come from damaged records; failure unchanged otherwise.
     * @note Use only where the index comes from a record or from the parser, never for an index
     * a caller passed in, which keeps its access reason. Work, resource and allocation failures
     * pass through. Performs no allocation.
     * @par ID SWU-COMMON-003
     */
    [[nodiscard]] constexpr error as_internal_fault(const error& failure) noexcept
    {
      // Following a link the library itself recorded: an access failure means damaged records.
      return (failure.code() == error_code::wrong_type_or_access) ? error::internal_fault() : failure;
    }
  } // namespace detail
} // namespace adaptive_json5

#endif /* INTERNAL_LINK_HPP_ */
