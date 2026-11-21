#ifndef STORAGE_HPP_
#define STORAGE_HPP_

#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Release storage through its original provider.
   * @par Requirements
   * P003; J5-040; J5-051; J5-101.
   * @note address, bytes and alignment must match an outstanding
   * successful allocation. Release must not throw or allocate.
   */
  using deallocate_function = void (*)(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept;

  /**
   * @brief Request aligned storage while preserving an explicit failure.
   * @par Requirements
   * P032; J5-003; J5-046; J5-047; J5-101.
   * @return Nonnull aligned storage on success, or an explicit failure.
   * @note Called for nonzero bytes and validated alignment.
   * Budget exhaustion reports resource_limit.
   * Provider exhaustion reports allocation_failure.
   * Failure transfers no allocation to the caller.
   * Successful storage must be releasable through the paired deallocate callback.
   * Must not throw, terminate on allocation failure or allocate error storage.
   */
  using allocate_function = result<void*> (*)(void* context, std::size_t bytes, std::size_t alignment) noexcept;

  /**
   * @brief Borrow a caller-owned storage provider.
   * @par Requirements
   * P003; P032; J5-040; J5-046; J5-047; J5-101.
   * @note Provider state must outlive every allocation using it.
   * context may be nullptr for a stateless provider.
   * Default construction supplies no usable provider.
   * A usable provider requires both allocate and deallocate.
   * Both callbacks receive the same context.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.18)
  struct [[nodiscard]] storage_callbacks
  {
      void* context = nullptr;
      // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
      // MISRA justification: The declaration and the definition use the same alias (status_result for
      // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
      // alias across translation units and reports it as different.
      // codeql::misra_deviation_next_line(DEV-013.23)
      allocate_function allocate = nullptr;
      deallocate_function deallocate = nullptr;
  };
} // namespace adaptive_json5

#endif /* STORAGE_HPP_ */
