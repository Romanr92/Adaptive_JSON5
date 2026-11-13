#ifndef RESOURCE_BUDGET_HPP_
#define RESOURCE_BUDGET_HPP_

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
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
  class [[nodiscard]] resource_budget
  {
    private:
      static constexpr std::size_t c_no_resources = 0;
      static constexpr std::size_t c_single_resource = 1;

      resource_limits m_limits;
      std::size_t m_nodes{};
      std::size_t m_members{};
      std::size_t m_depth{};
      std::size_t m_peak_depth{};
      std::size_t m_string_units{};
      std::size_t m_diagnostics{};

      /**
       * @brief Check whether an additional amount fits without overflowing.
       * @par Requirements
       * P031; J5-047; J5-048; issue #125.
       * @param current Amount already reserved.
       * @param additional Amount to add.
       * @param limit Inclusive maximum for current + additional.
       * @param reason Limit being checked; every caller names its own resource_limits field.
       * @return Success, or resource_limit carrying reason without a location; the engine
       * adds the location of the offending token.
       * @note Subtraction is evaluated only after validating current.
       * @par ID SWU-MEMORY-056
       */
      [[nodiscard]] static status_result check_addition(std::size_t current, std::size_t additional, std::size_t limit,
                                                        resource_limit_reason reason) noexcept;

    public:
      /**
       * @brief Begin accounting with no resources reserved.
       * @par Requirements
       * P031; J5-047.
       * @note Every size_t limit is supported, including zero.
       * Cross-limit inequalities are not inherently invalid configurations.
       * @par ID SWU-MEMORY-057
       */
      explicit resource_budget(const resource_limits& limits) noexcept;

      /**
       * @brief Check the complete supplied input size before processing.
       * @par Requirements
       * P031; J5-047.
       * @note Repeated checks do not accumulate input bytes.
       * @par ID SWU-MEMORY-058
       */
      [[nodiscard]] status_result check_input(std::size_t bytes) const noexcept;

      /**
       * @brief Check one numeric spelling and its exponent digit count.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note The caller supplies zero exponent digits when no exponent exists.
       * This operation checks lengths, not numeric grammar or exponent magnitude.
       * Call during scanning before accepting a byte that exceeds either limit.
       * @par ID SWU-MEMORY-059
       */
      [[nodiscard]] status_result check_number(std::size_t bytes, std::size_t exponent_digits) const noexcept;

      /**
       * @brief Reserve one value and optionally its object-member entry.
       * @par Requirements
       * P031; J5-047.
       * @param member True when this value is a direct object member.
       * @note The root, array elements and detached nonmember values use false.
       * Both limits are checked before either counter changes.
       * @par ID SWU-MEMORY-060
       */
      [[nodiscard]] status_result reserve_value(bool member) noexcept;

      /**
       * @brief Release a matching value reservation during rollback.
       * @par Requirements
       * P031; J5-047.
       * @param member Must match the original reservation.
       * @note Callers must associate releases with their own reservations.
       * Invalid counter underflow is reported without changing either counter.
       * @par ID SWU-MEMORY-061
       */
      [[nodiscard]] status_result release_value(bool member) noexcept;

      /**
       * @brief Reserve one additional simultaneously open container.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note Call before growing parser storage for the new nesting level.
       * @par ID SWU-MEMORY-062
       */
      [[nodiscard]] status_result enter_container() noexcept;

      /**
       * @brief Release one open-container reservation.
       * @par Requirements
       * P031; J5-047; J5-048.
       * @note Closing or rolling back a container releases its depth.
       * @par ID SWU-MEMORY-063
       */
      [[nodiscard]] status_result leave_container() noexcept;

      /**
       * @brief Reserve additional decoded units for one string or member name.
       * @par Requirements
       * P031; J5-047.
       * @param current_units Units already reserved for this particular string.
       * @param additional_units New units to reserve before growing its storage.
       * @note current_units must describe this string's existing reservations.
       * Checks both per-string and aggregate limits before changing accounting.
       * @par ID SWU-MEMORY-064
       */
      [[nodiscard]] status_result reserve_string_units(std::size_t current_units,
                                                       std::size_t additional_units) noexcept;

      /**
       * @brief Release decoded-unit reservations during rollback.
       * @par Requirements
       * P031; J5-047.
       * @note units must belong to reservations owned by the caller.
       * Does not release any allocation; storage ownership is separate.
       * @par ID SWU-MEMORY-065
       */
      [[nodiscard]] status_result release_string_units(std::size_t units) noexcept;

      /**
       * @brief Report the decoded-unit allowance for the next string or member name.
       * @par Requirements
       * P031; J5-047.
       * @return min(max_string_units, max_total_string_units - reserved units).
       * @note Single source for the lexer limit; reserves nothing and changes no state.
       * Returns zero if the reserved total ever exceeded the aggregate limit.
       * @par ID SWU-MEMORY-066
       */
      [[nodiscard]] std::size_t string_unit_allowance() const noexcept;

      /**
       * @brief Report how many more diagnostic entries fit.
       * @par Requirements
       * P031; J5-017; J5-047; issue #125.
       * @return max_diagnostics - reserved diagnostic entries.
       * @note Reserves nothing and changes no state. The counting pass passes it to the
       * lexers, which mark the first warning that does not fit.
       * Returns zero if the reserved count ever exceeded the limit.
       * @par ID SWU-MEMORY-077
       */
      [[nodiscard]] std::size_t diagnostic_allowance() const noexcept;

      /**
       * @brief Reserve one retained diagnostic entry.
       * @par Requirements
       * P031; J5-047.
       * @note Call before allocating or publishing the entry.
       * A minimal result/error value does not consume a diagnostic entry.
       * @par ID SWU-MEMORY-067
       */
      [[nodiscard]] status_result reserve_diagnostic() noexcept;

      /**
       * @brief Release one retained or rolled-back diagnostic reservation.
       * @par Requirements
       * P031; J5-047.
       * @par ID SWU-MEMORY-068
       */
      [[nodiscard]] status_result release_diagnostic() noexcept;

      /**
       * @brief Report the number of value reservations currently held.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note After a successful counting pre-pass this is the exact record count to reserve
       * (ADR 0003, decision 7). Changes no state.
       * @par ID SWU-MEMORY-069
       */
      [[nodiscard]] std::size_t reserved_nodes() const noexcept;

      /**
       * @brief Report the deepest nesting reserved at any time.
       * @par Requirements
       * P031; J5-046; J5-048.
       * @note Not reduced by leave_container(), so after a successful counting pre-pass it is
       * the exact parser frame count to reserve (ADR 0003, decision 7). Changes no state.
       * @par ID SWU-MEMORY-070
       */
      [[nodiscard]] std::size_t peak_depth() const noexcept;
  };
} // namespace adaptive_json5

#endif /* RESOURCE_BUDGET_HPP_ */
