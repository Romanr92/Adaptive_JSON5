#include "memory/pointer_alignment.hpp"

#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Align a position inside a buffer, like std::align, without <memory>.
     * @par Requirements
     * J5-042; J5-046; J5-101.
     * @par ID SWU-MEMORY-055
     */
    void* align_within(std::size_t alignment, std::size_t bytes, void*& position, std::size_t& space) noexcept
    {
      // MISRA rule: RULE-8-2-5 cpp/misra/reinterpret-cast-shall-not-be-used
      // MISRA rule: RULE-8-2-7 cpp/misra/no-pointer-to-integral-cast
      // MISRA justification: Use the numeric address to compute alignment padding. Check that both the
      // padding and the requested object fit before advancing the position.
      // codeql::misra_deviation_next_line(DEV-003.01)
      const auto address = reinterpret_cast<std::uintptr_t>(position);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Callers pass a validated nonzero power-of-two alignment, so alignment - 1 is the
      // alignment mask.
      // codeql::misra_deviation_next_line(DEV-004.117)
      const auto mask = static_cast<std::uintptr_t>(alignment - 1);

      // Distance up to the next multiple of alignment; zero when already aligned.
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The subtraction is intentionally modular; the result is masked to below alignment,
      // which is the padding to the next aligned address.
      // codeql::misra_deviation_next_line(DEV-004.116)
      const auto padding = static_cast<std::size_t>((static_cast<std::uintptr_t>(alignment) - (address & mask)) & mask);

      // Compare by subtraction so a large request cannot overflow.
      if ((padding > space) || (bytes > (space - padding)))
      {
        return nullptr;
      }

      // Advance the original pointer instead of converting the integer back, so provenance is kept.
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Converts the untyped storage pointer to the byte or object type it was created as.
      // This is a static_cast from void*, not an integer-to-pointer conversion.
      // codeql::misra_deviation_next_line(DEV-002.25)
      position = static_cast<std::byte*>(position) + padding;
      space -= padding;

      return position;
    }
  } // namespace detail
} // namespace adaptive_json5
