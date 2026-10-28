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
     */
    void* align_within(std::size_t alignment, std::size_t bytes, void*& position, std::size_t& space) noexcept
    {
      const auto address = reinterpret_cast<std::uintptr_t>(position);
      const auto mask = static_cast<std::uintptr_t>(alignment - 1);

      // Distance up to the next multiple of alignment; zero when already aligned.
      const auto padding = static_cast<std::size_t>((static_cast<std::uintptr_t>(alignment) - (address & mask)) & mask);

      // Compare by subtraction so a large request cannot overflow.
      if ((padding > space) || (bytes > (space - padding)))
      {
        return nullptr;
      }

      position = reinterpret_cast<void*>(address + padding);
      space -= padding;

      return position;
    }
  } // namespace detail
} // namespace adaptive_json5
