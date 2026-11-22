#include "schema/pod_vector.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>

namespace adaptive_json5::detail
{
  namespace
  {
    constexpr std::size_t c_first_capacity = 8U;
  } // namespace

  /**
   * @brief Return the capacity after growing from current, or nullopt when doubling would overflow.
   * @par Requirements
   * P044; J5-046.
   * @par ID SWU-SCHEMA-189
   */
  std::optional<std::size_t> next_capacity(std::size_t current, std::size_t element_size) noexcept
  {
    if (current == 0U)
    {
      return c_first_capacity;
    }
    const std::size_t largest = std::numeric_limits<std::size_t>::max() / element_size;
    if (current > (largest / 2U))
    {
      return std::nullopt;
    }
    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: current is at most largest / 2 (checked above), so doubling it cannot wrap.
    // codeql::misra_deviation_next_line(DEV-101.79)
    return current * 2U;
  }

  /**
   * @brief Copy bytes between two blocks; does nothing for zero bytes (null blocks allowed then).
   * @par Requirements
   * P044; J5-046.
   * @par ID SWU-SCHEMA-190
   */
  void copy_bytes(void* destination, const void* source, std::size_t bytes) noexcept
  {
    if (bytes != 0U)
    {
      // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
      // MISRA justification: pod_vector copies trivially copyable elements (static_assert) between blocks of at
      // least bytes bytes, as in the parser (DEV-023).
      // codeql::misra_deviation_next_line(DEV-110.1)
      static_cast<void>(std::memcpy(destination, source, bytes));
    }
  }
} // namespace adaptive_json5::detail
