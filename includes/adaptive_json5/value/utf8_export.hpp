#ifndef UTF8_EXPORT_HPP_
#define UTF8_EXPORT_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Calculate the checked UTF-8 size of a decoded string.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @return Required bytes, encoding error for unpaired surrogates,
   * or resource_limit if the byte count cannot be represented.
   * @note No terminating NUL is included. Performs no allocation.
   */
  [[nodiscard]] result<std::size_t> utf8_size(const decoded_string& source) noexcept;

  /**
   * @brief Export a decoded string into caller-provided UTF-8 storage.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @param destination Writable byte array of capacity bytes.
   * May be nullptr when the required output size is zero.
   * @return Bytes written, encoding error for unpaired surrogates,
   * resource_limit for insufficient capacity or size overflow,
   * or wrong_type_or_access for a null nonempty destination.
   * @note No allocation or terminating NUL. Embedded NUL is preserved.
   * Validation and capacity failures leave the destination unchanged.
   * Source and destination storage must not overlap.
   * The source must remain unchanged throughout this call.
   */
  [[nodiscard]] result<std::size_t> export_utf8(const decoded_string& source, unsigned char* destination,
                                                std::size_t capacity) noexcept;
} // namespace adaptive_json5

#endif /* UTF8_EXPORT_HPP_ */
