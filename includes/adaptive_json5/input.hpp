#ifndef INPUT_HPP_
#define INPUT_HPP_

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Borrowed, immutable input bytes with an explicit length.
   * @par Requirements
   * J5-004.
   * @note No terminating NUL is required. Embedded zero bytes count
   * toward size and do not terminate the input.
   * The caller retains ownership and keeps the bytes alive and
   * unchanged while an operation reads them.
   * A nonzero size requires data to address at least size readable bytes.
   */
  struct input_view
  {
      const char* data = nullptr;
      std::size_t size = 0;
  };

  /**
   * @brief Describe a range in the original input using byte offsets.
   * @par Requirements
   * P032; J5-041; J5-044.
   * @note byte_offset is zero-based and byte_length counts bytes.
   * The range includes its first byte and excludes its end position.
   * Contains no pointer and does not retain ownership of source text.
   * Construction does not validate the range against an input.
   * Consumers must validate bounds before accessing source bytes.
   */
  struct source_span
  {
      std::size_t byte_offset{};
      std::size_t byte_length{};
  };
} // namespace adaptive_json5

#endif /* INPUT_HPP_ */
