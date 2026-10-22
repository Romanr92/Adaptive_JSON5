#ifndef STRING_TOKEN_HPP_
#define STRING_TOKEN_HPP_

#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Observe one warning from a successfully recognized string.
   * @par Requirements
   * P021; J5-017.
   * @param context Caller-owned observer state.
   * @param warning Warning descriptor, valid for the duration of this call.
   * @note The observer must copy the descriptor if it needs to retain it.
   * It must not throw, modify the input, or modify the source cursor.
   * It must not invalidate the storage provider or its allocations.
   * No recoverable callback failure is supported.
   */
  using string_warning_function = void (*)(void* context, const string_warning& warning) noexcept;

  /**
   * @brief Borrow an optional caller-owned warning observer.
   * @par Requirements
   * P021; J5-017.
   * @note A null notify pointer disables callbacks.
   * context may be nullptr for a stateless observer.
   * Observer state must remain valid throughout the lexer call.
   * Notifications occur in source order only after complete string success.
   */
  struct [[nodiscard]] string_warning_observer
  {
      void* context = nullptr;
      string_warning_function notify = nullptr;
  };

  /**
   * @brief Own one decoded string token and its source metadata.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-041.
   * @note value owns its decoded units independently of caller input.
   * Its storage provider state must outlive the owned value.
   * byte_offset and byte_length include both source quotation marks.
   * Warning spans refer to original input byte positions.
   * warning_count counts all raw separator warnings.
   * first_warning is present exactly when warning_count is nonzero.
   * Warning metadata is retained even when callbacks are disabled.
   */
  struct [[nodiscard]] string_token
  {
      decoded_string value{};
      std::size_t byte_offset{};
      std::size_t byte_length{};
      std::size_t warning_count{};
      std::optional<string_warning> first_warning{};
  };
} // namespace adaptive_json5

#endif /* STRING_TOKEN_HPP_ */
