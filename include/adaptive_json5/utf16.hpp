#ifndef UTF16_HPP_
#define UTF16_HPP_

#include "error.hpp"
#include "result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Hold the UTF-16 representation of one Unicode scalar.
   * @par Requirements
   * P013; J5-018.
   * @note Only the first size entries contain output units.
   * These units are stored inline without allocation.
   */
  struct encoded_utf16
  {
      static constexpr std::size_t c_first_unit_index = 0;
      static constexpr std::size_t c_second_unit_index = 1;
      static constexpr std::size_t c_single_unit_count = 1;
      static constexpr std::size_t c_surrogate_pair_unit_count = 2;

      char16_t units[2]{};
      std::size_t size = 0;
  };

  /**
   * @brief Convert one Unicode scalar into one or two UTF-16 units.
   * @par Requirements
   * P013; J5-018.
   * @return Encoded units, or encoding error for an invalid scalar.
   * @note Surrogates are rejected as scalar inputs.
   * Escaped UTF-16 units must be preserved separately without this conversion.
   */
  inline result<encoded_utf16> encod_utf16(char32_t scalar) noexcept
  {
    constexpr char32_t c_max_scalar = 0x10FFFF;
    constexpr char32_t c_high_surrogate_min = 0xD800;
    constexpr char32_t c_low_surrogate_max = 0xDFFF;
    constexpr char32_t c_bmp_max = 0xFFFF;
    constexpr char32_t c_supplementary_offset = 0x10000;
    constexpr char32_t c_low_surrogate_min = 0xDC00;
    constexpr char32_t c_payload_mask = 0x3FF;
    constexpr unsigned int c_surrogate_payload_bits = 10;

    // Surrogate code points are units, not valid Unicode scalers.
    if ((scalar > c_max_scalar) || (scalar >= c_high_surrogate_min) && (scalar <= c_low_surrogate_max))
    {
      return result<encoded_utf16>::failure(error::without_location(error_code::encoding));
    }

    encoded_utf16 output{};

    if (scalar <= c_bmp_max)
    {
      output.units[encoded_utf16::c_first_unit_index] = static_cast<char16_t>(scalar);
      output.size = encoded_utf16::c_single_unit_count;
    }
    else
    {
      // remove the BMP range so the remaining value fits into two ten-bit payloads.
      const char32_t payload = scalar - c_supplementary_offset;

      output.units[encoded_utf16::c_first_unit_index] =
        static_cast<char16_t>(c_high_surrogate_min + (payload >> c_surrogate_payload_bits));
      output.units[encoded_utf16::c_second_unit_index] =
        static_cast<char16_t>(c_low_surrogate_min + (payload & c_payload_mask));
      output.size = encoded_utf16::c_surrogate_pair_unit_count;
    }

    return result<encoded_utf16>::success(output);
  }
} // namespace adaptive_json5

#endif // UTF16_HPP_
