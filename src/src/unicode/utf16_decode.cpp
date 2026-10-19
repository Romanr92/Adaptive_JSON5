// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/unicode/utf16_decode.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>

namespace adaptive_json5
{
  result<decoded_utf16> decode_utf16(const decoded_string& source, std::size_t index) noexcept
  {
    constexpr char32_t c_high_surrogate_min = 0xD800;
    constexpr char32_t c_high_surrogate_max = 0xDBFF;
    constexpr char32_t c_low_surrogate_min = 0xDC00;
    constexpr char32_t c_low_surrogate_max = 0xDFFF;
    constexpr char32_t c_supplementary_offset = 0x10000;
    constexpr unsigned int c_surrogate_payload_bits = 10;
    constexpr std::size_t c_single_unit_count = 1;
    constexpr std::size_t c_surrogate_pair_unit_count = 2;

    const auto first = source.unit_at(index);

    if (!first.has_value())
    {
      return result<decoded_utf16>::failure(*first.error_if());
    }

    const char32_t leading = *first.value_if();

    // A low surrogate cannot begin a scalar.
    if ((leading >= c_low_surrogate_min) && (leading <= c_low_surrogate_max))
    {
      return result<decoded_utf16>::failure(error::without_location(error_code::encoding));
    }

    if ((leading < c_high_surrogate_min) || (leading > c_high_surrogate_max))
    {
      return result<decoded_utf16>::success(decoded_utf16{leading, c_single_unit_count});
    }

    // The successful first read proves index < size, making subtraction safe.
    if ((source.size() - index) < c_surrogate_pair_unit_count)
    {
      return result<decoded_utf16>::failure(error::without_location(error_code::encoding));
    }

    const auto second = source.unit_at(index + c_single_unit_count);
    if (!second.has_value())
    {
      return result<decoded_utf16>::failure(*second.error_if());
    }

    const char32_t trailing = *second.value_if();
    if ((trailing < c_low_surrogate_min) || (trailing > c_low_surrogate_max))
    {
      return result<decoded_utf16>::failure(error::without_location(error_code::encoding));
    }

    // Combine the two ten-bit payloads, restoring the supplementary offset.
    const char32_t scalar = c_supplementary_offset + ((leading - c_high_surrogate_min) << c_surrogate_payload_bits) +
                            (trailing - c_low_surrogate_min);

    return result<decoded_utf16>::success(decoded_utf16{scalar, c_surrogate_pair_unit_count});
  }
} // namespace adaptive_json5
