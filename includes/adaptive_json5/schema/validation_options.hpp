#ifndef ADAPTIVE_JSON5_SCHEMA_VALIDATION_OPTIONS_HPP_
#define ADAPTIVE_JSON5_SCHEMA_VALIDATION_OPTIONS_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/schema/schema_budget.hpp"

#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief How many assertion failures a validation gathers.
   * @par Requirements
   * P043; J5-063; ADR 0006 decision 12.
   * @note fail_fast stops at the first assertion failure. collect_all keeps up to
   * validation_budget::max_errors diagnostics and marks the report truncated beyond that.
   */
  enum class evaluation_mode : std::uint8_t
  {
    fail_fast,
    collect_all
  };

  /**
   * @brief Ask whether a running validation should stop.
   * @par Requirements
   * P043; J5-049; ADR 0006 decision 7.
   * @return True to stop; validation then returns schema_error_code::cancelled.
   * @note Polled once per evaluation step. Must not throw and must not touch the compiled
   * schema or the work context of the validation that polls it (integrator obligation).
   */
  using cancellation_poll = bool (*)(void* context) noexcept;

  /**
   * @brief Optional caller cancellation callback.
   * @par Requirements
   * P043; J5-049; ADR 0006 decision 7.
   * @note A null poll means no cancellation polling; budgets still bound the work.
   */
  struct [[nodiscard]] cancellation_callback
  {
      cancellation_poll poll = nullptr;
      void* context = nullptr;
  };

  /**
   * @brief Per-validation settings and work storage.
   * @par Requirements
   * P043; J5-050; J5-063; J5-065; ADR 0006 decisions 4, 5, 7 and 12.
   * @note Each validation uses its own work storage, released before the call returns.
   * Concurrent validations of one compiled schema are safe when each uses its own
   * work storage; sharing one between threads is unsupported.
   */
  struct [[nodiscard]] validation_options
  {
      storage_callbacks work_storage{};   // Frame stack and pre-pass; released before validate() returns.
      storage_callbacks report_storage{}; // Diagnostics; owned by the returned report.
      validation_budget budget{};
      evaluation_mode mode = evaluation_mode::fail_fast;
      cancellation_callback cancellation{};
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_VALIDATION_OPTIONS_HPP_ */
