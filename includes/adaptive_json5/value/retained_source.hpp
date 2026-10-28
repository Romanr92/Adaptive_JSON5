#ifndef ADAPTIVE_JSON5_RETAINED_SOURCE_HPP_
#define ADAPTIVE_JSON5_RETAINED_SOURCE_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

namespace adaptive_json5
{
  /**
   * @brief Own an optional copy of the original input bytes for diagnostics and tooling.
   * @par Requirements
   * P039; J5-040; J5-041; J5-046.
   * @note A default-constructed object retains nothing.
   * The copy is independent of the caller's input and survives moves of its owner.
   * The storage provider used by copy() must outlive this object.
   * Moving transfers the bytes and leaves the source retaining nothing; copying is unavailable.
   */
  class [[nodiscard]] retained_source
  {
    private:
      owned_block m_bytes{};

    public:
      /**
       * @brief Construct an object that retains nothing.
       * @par Requirements
       * P039; J5-041.
       * @note Performs no allocation.
       */
      retained_source() noexcept = default;

      /**
       * @brief Transfer the retained copy and leave other empty.
       * @par Requirements
       * P039; J5-041.
       * @note Performs no allocation.
       */
      retained_source(retained_source&& other) noexcept = default;

      /**
       * @brief Release the retained copy through its original provider.
       * @par Requirements
       * P039; J5-040; J5-041.
       */
      ~retained_source() noexcept = default;

      /**
       * @brief Copying is not allowed.
       * @par Requirements
       * P039; J5-041.
       * @note A copy needs storage, which can fail; request retention in parse_options instead.
       */
      retained_source(const retained_source&) = delete;

      /**
       * @brief Copy assignment is not allowed.
       * @par Requirements
       * P039; J5-041.
       * @note Same reason as the copy constructor.
       */
      retained_source& operator=(const retained_source&) = delete;

      /**
       * @brief Move assignment is not allowed.
       * @par Requirements
       * P039; J5-041.
       * @note Replacing a live copy would release it first; construct a new object instead.
       */
      retained_source& operator=(retained_source&&) = delete;

      /**
       * @brief Copy input bytes into storage owned by the returned object.
       * @par Requirements
       * P039; J5-003; J5-041; J5-046.
       * @param input Borrowed bytes to copy; need not remain alive afterwards.
       * @param storage Fallible provider charged for exactly input.size bytes.
       * @return Owning copy, or wrong_type_or_access for null data with a nonzero size,
       * or the provider's resource/allocation failure.
       * @note Empty input retains nothing and performs no provider call.
       */
      [[nodiscard]] static result<retained_source> copy(input_view input, const storage_callbacks& storage) noexcept;

      /**
       * @brief Report whether any bytes are retained.
       * @par Requirements
       * P039; J5-041.
       */
      [[nodiscard]] bool is_retained() const noexcept;

      /**
       * @brief Borrow the retained bytes.
       * @par Requirements
       * P039; J5-041; J5-044.
       * @return The retained bytes, or an empty view when nothing is retained.
       * @note The view is invalidated by moving or destroying this object.
       */
      [[nodiscard]] input_view text() const noexcept;
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_RETAINED_SOURCE_HPP_ */
