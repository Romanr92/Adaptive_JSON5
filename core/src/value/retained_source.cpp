#include "adaptive_json5/value/retained_source.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstring>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Allocate exactly the input size, then copy the bytes.
   * @par Requirements
   * P039; J5-003; J5-041; J5-046.
   */
  result<retained_source> retained_source::copy(input_view input, const storage_callbacks& storage) noexcept
  {
    using source_result = result<retained_source>;

    if ((input.data == nullptr) && (input.size != 0))
    {
      return source_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    retained_source retained{};

    // Empty input has nothing to keep, so it makes no provider call.
    if (input.size == 0)
    {
      return source_result::success(std::move(retained));
    }

    auto allocation = allocate_block(storage, input.size, alignof(char));

    if (!allocation.has_value())
    {
      return source_result::failure(*allocation.error_if());
    }

    std::memcpy(allocation.value_if()->data(), input.data, input.size);
    retained.m_bytes = std::move(*allocation.value_if());

    return source_result::success(std::move(retained));
  }

  /**
   * @brief An owned block without data means nothing was retained.
   * @par Requirements
   * P039; J5-041.
   */
  bool retained_source::is_retained() const noexcept
  {
    return m_bytes.data() != nullptr;
  }

  /**
   * @brief Expose the owned bytes as a borrowed view.
   * @par Requirements
   * P039; J5-041; J5-044.
   */
  input_view retained_source::text() const noexcept
  {
    return input_view{static_cast<const char*>(m_bytes.data()), m_bytes.size()};
  }
} // namespace adaptive_json5
