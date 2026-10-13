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
} // namespace adaptive_json5

#endif /* INPUT_HPP_ */
