#ifndef RESOURCE_BUDGET_HPP_
#define RESOURCE_BUDGET_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/result.hpp"

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
      static constexpr std::size_t c_no_resources = 0;
      static constexpr std::size_t c_single_resource = 1;

      resource_limits m_limits;
      std::size_t m_nodes{};
      std::size_t m_members{};
      std::size_t m_depth{};
      std::size_t m_string_units{};
      std::size_t m_diagnostics{};

      /**
       * @brief Check whether an additional amount fits without overflowing.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note Subtraction is evaluated only after validating current.
       */
      static status_result check_addition(std::size_t current, std::size_t additional, std::size_t limit) noexcept;

    public:
      /**
       * @brief Begin accounting with no resources reserved.
       * @par Requirements
       * P031; J5-047.
       * @note Every size_t limit is supported, including zero.
       * Cross-limit inequalities are not inherently invalid configurations.
       */
      explicit resource_budget(const resource_limits& limits) noexcept;

      /**
       * @brief Check the complete supplied input size before processing.
       * @par Requirements
       * P031; J5-047.
       * @note Repeated checks do not accumulate input bytes.
       */
      status_result check_input(std::size_t bytes) const noexcept;

      /**
       * @brief Check one numeric spelling and its exponent digit count.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note The caller supplies zero exponent digits when no exponent exists.
       * This operation checks lengths, not numeric grammar or exponent magnitude.
       * Call during scanning before accepting a byte that exceeds either limit.
       */
      status_result check_number(std::size_t bytes, std::size_t exponent_digits) const noexcept;

      /**
       * @brief Reserve one value and optionally its object-member entry.
       * @par Requirements
       * P031; J5-047.
       * @param member True when this value is a direct object member.
       * @note The root, array elements and detached nonmember values use false.
       * Both limits are checked before either counter changes.
       */
      status_result reserve_value(bool member) noexcept;

      /**
       * @brief Release a matching value reservation during rollback.
       * @par Requirements
       * P031; J5-047.
       * @param member Must match the original reservation.
       * @note Callers must associate releases with their own reservations.
       * Invalid counter underflow is reported without changing either counter.
       */
      status_result release_value(bool member) noexcept;

      /**
       * @brief Reserve one additional simultaneously open container.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note Call before growing parser storage for the new nesting level.
       */
      status_result enter_container() noexcept;

      /**
       * @brief Release one open-container reservation.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note Closing or rolling back a container releases its depth.
       */
      status_result leave_container() noexcept;

      /**
       * @brief Reserve additional decoded units for one string or member name.
       * @par Requirements
       * P031; J5-047.
       * @param current_units Units already reserved for this particular string.
       * @param additional_units New units to reserve before growing its storage.
       * @note current_units must describe this string's existing reservations.
       * Checks both per-string and aggregate limits before changing accounting.
       */
      status_result reserve_string_units(std::size_t current_units, std::size_t additional_units) noexcept;

      /**
       * @brief Release decoded-unit reservations during rollback.
       * @par Requirements
       * P031; J5-047.
       * @note units must belong to reservations owned by the caller.
       * Does not release any allocation; storage ownership is separate.
       */
      status_result release_string_units(std::size_t units) noexcept;

      /**
       * @brief Reserve one retained diagnostic entry.
       * @par Requirements
       * P031; J5-047.
       * @note Call before allocating or publishing the entry.
       * A minimal result/error value does not consume a diagnostic entry.
       */
      status_result reserve_diagnostic() noexcept;

      /**
       * @brief Release one retained or rolled-back diagnostic reservation.
       * @par Requirements
       * P031; J5-047.
       */
      status_result release_diagnostic() noexcept;
  };
} // namespace adaptive_json5

#endif /* RESOURCE_BUDGET_HPP_ */
