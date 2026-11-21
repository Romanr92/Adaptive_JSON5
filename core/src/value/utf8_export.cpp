#include "adaptive_json5/value/utf8_export.hpp"

#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "common/checked_size.hpp"
#include "unicode/utf16_decode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Return the char that holds one UTF-8 byte value.
     * @par Requirements
     * P013; J5-029.
     * @param value Byte value, 0 to 0xFF.
     * @return The char with that bit pattern.
     * @note A lookup in a table of character literals, so no integer is converted to char
     * (MISRA C++:2023 Rule 7.0.3) and values above 0x7F need no implementation-defined
     * conversion to a possibly signed char.
     * @par ID SWU-VALUE-184
     */
    [[nodiscard]] char utf8_char(std::uint32_t value) noexcept
    {
      static constexpr std::array<char, 256U> c_byte_chars{
        {'\x00', '\x01', '\x02', '\x03', '\x04', '\x05', '\x06', '\x07', '\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D',
         '\x0E', '\x0F', '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19', '\x1A', '\x1B',
         '\x1C', '\x1D', '\x1E', '\x1F', '\x20', '\x21', '\x22', '\x23', '\x24', '\x25', '\x26', '\x27', '\x28', '\x29',
         '\x2A', '\x2B', '\x2C', '\x2D', '\x2E', '\x2F', '\x30', '\x31', '\x32', '\x33', '\x34', '\x35', '\x36', '\x37',
         '\x38', '\x39', '\x3A', '\x3B', '\x3C', '\x3D', '\x3E', '\x3F', '\x40', '\x41', '\x42', '\x43', '\x44', '\x45',
         '\x46', '\x47', '\x48', '\x49', '\x4A', '\x4B', '\x4C', '\x4D', '\x4E', '\x4F', '\x50', '\x51', '\x52', '\x53',
         '\x54', '\x55', '\x56', '\x57', '\x58', '\x59', '\x5A', '\x5B', '\x5C', '\x5D', '\x5E', '\x5F', '\x60', '\x61',
         '\x62', '\x63', '\x64', '\x65', '\x66', '\x67', '\x68', '\x69', '\x6A', '\x6B', '\x6C', '\x6D', '\x6E', '\x6F',
         '\x70', '\x71', '\x72', '\x73', '\x74', '\x75', '\x76', '\x77', '\x78', '\x79', '\x7A', '\x7B', '\x7C', '\x7D',
         '\x7E', '\x7F', '\x80', '\x81', '\x82', '\x83', '\x84', '\x85', '\x86', '\x87', '\x88', '\x89', '\x8A', '\x8B',
         '\x8C', '\x8D', '\x8E', '\x8F', '\x90', '\x91', '\x92', '\x93', '\x94', '\x95', '\x96', '\x97', '\x98', '\x99',
         '\x9A', '\x9B', '\x9C', '\x9D', '\x9E', '\x9F', '\xA0', '\xA1', '\xA2', '\xA3', '\xA4', '\xA5', '\xA6', '\xA7',
         '\xA8', '\xA9', '\xAA', '\xAB', '\xAC', '\xAD', '\xAE', '\xAF', '\xB0', '\xB1', '\xB2', '\xB3', '\xB4', '\xB5',
         '\xB6', '\xB7', '\xB8', '\xB9', '\xBA', '\xBB', '\xBC', '\xBD', '\xBE', '\xBF', '\xC0', '\xC1', '\xC2', '\xC3',
         '\xC4', '\xC5', '\xC6', '\xC7', '\xC8', '\xC9', '\xCA', '\xCB', '\xCC', '\xCD', '\xCE', '\xCF', '\xD0', '\xD1',
         '\xD2', '\xD3', '\xD4', '\xD5', '\xD6', '\xD7', '\xD8', '\xD9', '\xDA', '\xDB', '\xDC', '\xDD', '\xDE', '\xDF',
         '\xE0', '\xE1', '\xE2', '\xE3', '\xE4', '\xE5', '\xE6', '\xE7', '\xE8', '\xE9', '\xEA', '\xEB', '\xEC', '\xED',
         '\xEE', '\xEF', '\xF0', '\xF1', '\xF2', '\xF3', '\xF4', '\xF5', '\xF6', '\xF7', '\xF8', '\xF9', '\xFA', '\xFB',
         '\xFC', '\xFD', '\xFE', '\xFF'}};

      return c_byte_chars[static_cast<std::size_t>(value)];
    }
  } // namespace

  /**
   * @brief Calculate the checked UTF-8 size of a decoded string.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @return Required bytes, encoding error for unpaired surrogates,
   * or resource_limit (output_capacity) if the byte count cannot be represented.
   * @note No terminating NUL is included. Performs no allocation.
   * @par ID SWU-VALUE-103
   */
  result<std::size_t> utf8_size(const decoded_string& source) noexcept
  {
    constexpr std::uint32_t c_ascii_max = 0x7F;
    constexpr std::uint32_t c_two_byte_max = 0x7FF;
    constexpr std::uint32_t c_three_byte_max = 0xFFFF;
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

      const std::uint32_t scalar = decoded.value_if()->value;
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
      else
      {
        // Larger scalars keep the four-byte count set above.
      }

      const auto next_total = checked_add(total, byte_count, resource_limit_reason::output_capacity);
      if (!next_total.has_value())
      {
        return result<std::size_t>::failure(*next_total.error_if());
      }

      total = *next_total.value_if();

      // Successful decoding guarantees this complete unit sequence is in range.
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: unit_count comes from decoding a sequence that lies inside the remaining units, so
      // index stays at most the unit count.
      // codeql::misra_deviation_next_line(DEV-004.147)
      index += decoded.value_if()->unit_count;
    }

    return result<std::size_t>::success(total);
  }

  /**
   * @brief Encode one validated scalar into the caller's prechecked UTF-8 output range.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @par ID SWU-VALUE-219
   */
  static void write_utf8_scalar(char*& next, std::uint32_t scalar) noexcept
  {
    constexpr std::uint32_t c_ascii_max = 0x7F;
    constexpr std::uint32_t c_two_byte_max = 0x7FF;
    constexpr std::uint32_t c_three_byte_max = 0xFFFF;
    constexpr std::uint32_t c_two_byte_prefix = 0xC0;
    constexpr std::uint32_t c_three_byte_prefix = 0xE0;
    constexpr std::uint32_t c_four_byte_prefix = 0xF0;
    constexpr std::uint32_t c_continuation_prefix = 0x80;
    constexpr std::uint32_t c_continuation_mask = 0x3F;
    constexpr std::uint32_t c_one_payload_shift = 6;
    constexpr std::uint32_t c_two_payload_shift = 12;
    constexpr std::uint32_t c_three_payload_shift = 18;
    if (scalar <= c_ascii_max)
    {
      *next++ = utf8_char(scalar);
    }
    else
    {
      if (scalar <= c_two_byte_max)
      {
        *next++ = utf8_char(c_two_byte_prefix | (scalar >> c_one_payload_shift));
      }
      else
      {
        if (scalar <= c_three_byte_max)
        {
          *next++ = utf8_char(c_three_byte_prefix | (scalar >> c_two_payload_shift));
        }
        else
        {
          *next++ = utf8_char(c_four_byte_prefix | (scalar >> c_three_payload_shift));
          *next++ = utf8_char(c_continuation_prefix | ((scalar >> c_two_payload_shift) & c_continuation_mask));
        }

        *next++ = utf8_char(c_continuation_prefix | ((scalar >> c_one_payload_shift) & c_continuation_mask));
      }

      *next++ = utf8_char(c_continuation_prefix | (scalar & c_continuation_mask));
    }
  }

  /**
   * @brief Export a decoded string into caller-provided UTF-8 storage.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @param destination Writable char array of capacity bytes.
   * May be nullptr when the required output size is zero.
   * @return Bytes written, encoding error for unpaired surrogates,
   * resource_limit (output_capacity) for insufficient capacity or size overflow,
   * or wrong_type_or_access (invalid_argument) for a null nonempty destination.
   * @note No allocation or terminating NUL. Embedded NUL is preserved.
   * Validation and capacity failures leave the destination unchanged.
   * Source and destination storage must not overlap.
   * The source must remain unchanged throughout this call.
   * @par ID SWU-VALUE-104
   */
  result<std::size_t> export_utf8(const decoded_string& source, char* destination, std::size_t capacity) noexcept
  {
    // Validate all units before exposing any partially written output.
    const auto required = utf8_size(source);
    if (!required.has_value())
    {
      return result<std::size_t>::failure(*required.error_if());
    }

    if (*required.value_if() > capacity)
    {
      return result<std::size_t>::failure(error::limit_without_location(resource_limit_reason::output_capacity));
    }

    if (*required.value_if() == std::size_t{})
    {
      return result<std::size_t>::success(std::size_t{});
    }

    if (destination == nullptr)
    {
      return result<std::size_t>::failure(error::access_failure(access_error_reason::invalid_argument));
    }

    std::size_t index{};
    char* next = destination;

    while (index < source.size())
    {
      const auto decoded = decode_utf16(source, index);
      if (!decoded.has_value())
      {
        return result<std::size_t>::failure(*decoded.error_if());
      }

      const std::uint32_t scalar = decoded.value_if()->value;

      write_utf8_scalar(next, scalar);

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: unit_count comes from decoding a sequence that lies inside the remaining units, so
      // index stays at most the unit count.
      // codeql::misra_deviation_next_line(DEV-004.146)
      index += decoded.value_if()->unit_count;
    }

    return result<std::size_t>::success(*required.value_if());
  }
} // namespace adaptive_json5
