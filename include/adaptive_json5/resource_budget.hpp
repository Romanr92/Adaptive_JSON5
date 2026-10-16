#ifndef RESOURCE_BUDGET_HPP_
#define RESOURCE_BUDGET_HPP_

#include "error.hpp"
#include "resource_limits.hpp"
#include "result.hpp"

#include <_mingw_stat64.h>
#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Track resource reservations for one operation.
   * @par Requirements
   * P031; J5-047; J5-048.
   * @note Copies the limits so later caller changes cannot alter this budget.
   * Failed checks leave every counter unchanged.
   * Successful reservations must precede the corresponding operation.
   * Callers release reservations when associated construction is rolled back.
   * This object is not shared concurrently without external synchronization.
   * Owned allocation bytes and work are accounted separately.
   */
  class resource_budget
  {
    private:
      resource_limits m_limits;
      std::size_t m_nodes{};
      std::size_t m_members{};
      std::size_t m_depth{};
      std::size_t m_string_diagnostics{};
      std::size_t m_diagnostics{};

      /**
       * @brief Check whether an additional amount fits without overflowing.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note Subtraction is evaluated only after validating current.
       */
      static status_result check_addition(std::size_t current, std::size_t additional, std::size_t limit) noexcept
      {
        // Subtraction avoids overflowing a speculative current + additional.
        if ((current > limit) || (additional > (limit - current)))
        {
          return status_result::failure(error::without_location(error_code::resource_limit));
        }

        return status_result::success();
      }

    public:
  };
} // namespace adaptive_json5

#endif /* RESOURCE_BUDGET_HPP_ */
