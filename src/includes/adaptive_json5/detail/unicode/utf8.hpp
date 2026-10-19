#ifndef UTF8_HPP_
#define UTF8_HPP_

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief One decoded Unicode scalar and its original byte span.
   * @par Requirements
   * P005; J5-009.
   * @note Contains no ownership of input storage.
   */
  struct decoded_character
  {
      char32_t value;
      std::size_t byte_offset;
      std::size_t byte_length;
  };

  /**
   * @brief Decode one UTF-8 scalar from the current cursor position.
   * @par Requirements
   * P005; J5-009.
   * @param source Cursor over valid borrowed input storage.
   * @return A decoded character, successful std::nullopt at EOF,
   * or an explicit encoding error.
   * @note Requires no allocation. Encoding failure reports the sequence's
   * starting byte offset and leaves the cursor unchanged.
   * BOM, NUL and noncharacters are decoded without applying grammar policy.
   */
  result<std::optional<decoded_character>> decode_utf8(cursor& source) noexcept;
} // namespace adaptive_json5

#endif /* UTF8_HPP_ */
