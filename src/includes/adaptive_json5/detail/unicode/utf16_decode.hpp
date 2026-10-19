#ifndef UTF16_DECODE_HPP_
#define UTF16_DECODE_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Hold one scalar decoded from stored UTF-16 units.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @note unit_count identifies how far the caller should advance.
   */
  struct decoded_utf16
  {
      char32_t value{};
      std::size_t unit_count{};
  };

  /**
   * @brief Decode one scalar at a UTF-16 unit index.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @return A scalar and consumed unit count, wrong_type_or_access for an
   * invalid index, or encoding error for an unpaired surrogate.
   * @note Does not allocate, modify the string, or advance caller state.
   * Errors have no original input byte offset: index counts decoded units.
   */
  result<decoded_utf16> decode_utf16(const decoded_string& source, std::size_t index) noexcept;
} // namespace adaptive_json5

#endif // UTF16_DECODE_HPP_
