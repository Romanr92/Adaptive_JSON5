#include "adaptive_json5/value/utf8_export.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "common/checked_size.hpp"
#include "unicode/utf16_decode.hpp"

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
  result<std::size_t> utf8_size(const decoded_string& source) noexcept
  {
    constexpr char32_t c_ascii_max = 0x7F;
    constexpr char32_t c_two_byte_max = 0x7FF;
    constexpr char32_t c_three_byte_max = 0xFFFF;
    constexpr std::size_t c_one_byte_count = 1;
    constexpr std::size_t c_two_byte_count = 2;
    constexpr std::size_t c_three_byte_count = 3;
    constexpr std::size_t c_four_byte_count = 4;

    std::size_t index{};
    std::size_t total{};

    while (index < source.size())
    {
      const auto decoded = decode_utf16(source, index);
      if (!decoded.has_value())
      {
        return result<std::size_t>::failure(*decoded.error_if());
      }

      const char32_t scalar = decoded.value_if()->value;
      std::size_t byte_count = c_four_byte_count;

      if (scalar <= c_ascii_max)
      {
        byte_count = c_one_byte_count;
      }
      else if (scalar <= c_two_byte_max)
      {
        byte_count = c_two_byte_count;
      }
      else if (scalar <= c_three_byte_max)
      {
        byte_count = c_three_byte_count;
      }

      const auto next_total = checked_add(total, byte_count);
      if (!next_total.has_value())
      {
        return result<std::size_t>::failure(*next_total.error_if());
      }

      total = *next_total.value_if();

      // Successful decoding guarantees this complete unit sequence is in range.
      index += decoded.value_if()->unit_count;
    }

    return result<std::size_t>::success(total);
  }

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
  result<std::size_t> export_utf8(const decoded_string& source, unsigned char* destination,
                                  std::size_t capacity) noexcept
  {
    constexpr char32_t c_ascii_max = 0x7F;
    constexpr char32_t c_two_byte_max = 0x7FF;
    constexpr char32_t c_three_byte_max = 0xFFFF;
    constexpr char32_t c_two_byte_prefix = 0xC0;
    constexpr char32_t c_three_byte_prefix = 0xE0;
    constexpr char32_t c_four_byte_prefix = 0xF0;
    constexpr char32_t c_continuation_prefix = 0x80;
    constexpr char32_t c_continuation_mask = 0x3F;
    constexpr unsigned int c_one_payload_shift = 6;
    constexpr unsigned int c_two_payload_shift = 12;
    constexpr unsigned int c_three_payload_shift = 18;

    // Validate all units before exposing any partially written output.
    const auto required = utf8_size(source);
    if (!required.has_value())
    {
      return result<std::size_t>::failure(*required.error_if());
    }

    if (*required.value_if() > capacity)
    {
      return result<std::size_t>::failure(error::without_location(error_code::resource_limit));
    }

    if (*required.value_if() == std::size_t{})
    {
      return result<std::size_t>::success(std::size_t{});
    }

    if (destination == nullptr)
    {
      return result<std::size_t>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    std::size_t index{};
    unsigned char* next = destination;

    while (index < source.size())
    {
      const auto decoded = decode_utf16(source, index);
      if (!decoded.has_value())
      {
        return result<std::size_t>::failure(*decoded.error_if());
      }

      const char32_t scalar = decoded.value_if()->value;

      if (scalar <= c_ascii_max)
      {
        *next++ = static_cast<unsigned char>(scalar);
      }
      else
      {
        if (scalar <= c_two_byte_max)
        {
          *next++ = static_cast<unsigned char>(c_two_byte_prefix | (scalar >> c_one_payload_shift));
        }
        else
        {
          if (scalar <= c_three_byte_max)
          {
            *next++ = static_cast<unsigned char>(c_three_byte_prefix | (scalar >> c_two_payload_shift));
          }
          else
          {
            *next++ = static_cast<unsigned char>(c_four_byte_prefix | (scalar >> c_three_payload_shift));
            *next++ = static_cast<unsigned char>(c_continuation_prefix |
                                                 ((scalar >> c_two_payload_shift) & c_continuation_mask));
          }

          *next++ =
            static_cast<unsigned char>(c_continuation_prefix | ((scalar >> c_one_payload_shift) & c_continuation_mask));
        }

        *next++ = static_cast<unsigned char>(c_continuation_prefix | (scalar & c_continuation_mask));
      }

      index += decoded.value_if()->unit_count;
    }

    return result<std::size_t>::success(*required.value_if());
  }
} // namespace adaptive_json5
