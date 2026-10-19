// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/unicode/utf8.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  result<std::optional<decoded_character>> decode_utf8(cursor& source) noexcept
  {
    using decode_result = result<std::optional<decoded_character>>;

    /**
     * @brief UTF-8 leading-byte boundaries and payload masks.
     * @par Requirements
     * P005; J5-009.
     */
    constexpr unsigned char c_ascii_max = 0x7F;

    constexpr unsigned char c_two_byte_lead_min = 0xC2;
    constexpr unsigned char c_two_byte_lead_max = 0xDF;
    constexpr unsigned char c_three_byte_lead_min = 0xE0;
    constexpr unsigned char c_three_byte_lead_max = 0xEF;
    constexpr unsigned char c_four_byte_lead_min = 0xF0;
    constexpr unsigned char c_four_byte_lead_max = 0xF4;

    constexpr unsigned char c_continuation_min = 0x80;
    constexpr unsigned char c_continuation_max = 0xBF;

    constexpr unsigned char c_two_byte_payload_mask = 0x1F;
    constexpr unsigned char c_three_byte_payload_mask = 0x0F;
    constexpr unsigned char c_four_byte_payload_mask = 0x07;
    constexpr unsigned char c_continuation_payload_mask = 0x3F;

    // These second-byte restrictions prevent overlong encodings,
    // encoded surrogates and values above U+10FFFF.
    constexpr unsigned char c_surrogate_sensitive_lead = 0xED;
    constexpr unsigned char c_e0_second_min = 0xA0;
    constexpr unsigned char c_ed_second_max = 0x9F;
    constexpr unsigned char c_f0_second_min = 0x90;
    constexpr unsigned char c_f4_second_max = 0x8F;

    constexpr unsigned int c_continuation_payload_bits = 6;

    constexpr std::size_t c_ascii_byte_count = 1;
    constexpr std::size_t c_two_byte_count = 2;
    constexpr std::size_t c_three_byte_count = 3;
    constexpr std::size_t c_four_byte_count = 4;
    constexpr std::size_t c_first_continuation_index = 1;

    const std::size_t start = source.offset();
    const auto first = source.peek();

    if (!first.has_value())
    {
      return decode_result::success(std::nullopt);
    }

    // Plain char may be signed; UTF-8 classification uses unsigned octets.
    const auto lead = static_cast<unsigned char>(*first);

    if (lead <= c_ascii_max)
    {
      source.advance(c_ascii_byte_count);

      return decode_result::success(
        std::optional<decoded_character>{decoded_character{static_cast<char32_t>(lead), start, c_ascii_byte_count}});
    }

    std::size_t byte_count = 0;
    char32_t scalar = 0;

    if ((lead >= c_two_byte_lead_min) && (lead <= c_two_byte_lead_max))
    {
      // Expect two bytes and retain the lead byte's five payload bits.
      byte_count = c_two_byte_count;
      scalar = static_cast<char32_t>(lead & c_two_byte_payload_mask);
    }
    else if ((lead >= c_three_byte_lead_min) && (lead <= c_three_byte_lead_max))
    {
      // Expect three bytes and retain the lead byte's four payload bits.
      byte_count = c_three_byte_count;
      scalar = static_cast<char32_t>(lead & c_three_byte_payload_mask);
    }
    else if ((lead >= c_four_byte_lead_min) && (lead <= c_four_byte_lead_max))
    {
      // Expect four bytes and retain the lead byte's three payload bits.
      byte_count = c_four_byte_count;
      scalar = static_cast<char32_t>(lead & c_four_byte_payload_mask);
    }
    else
    {
      // Reject an invalid leading byte without advancing the cursor.
      return decode_result::failure(error::at_offset(error_code::encoding, start));
    }

    // Use lookahead so incomplete or malformed input never partially advances.
    for (std::size_t index = c_first_continuation_index; index < byte_count; ++index)
    {
      const auto next = source.peek(index);

      if (!next.has_value())
      {
        // The leading byte promised more bytes than remain in the input.
        return decode_result::failure(error::at_offset(error_code::encoding, start));
      }

      // Interpret the byte as an unsigned octet, even when plain char is signed.
      const auto byte = static_cast<unsigned char>(*next);

      if ((byte < c_continuation_min) || (byte > c_continuation_max))
      {
        // A continuation must have the bit pattern 10xxxxxx (80–BF).
        return decode_result::failure(error::at_offset(error_code::encoding, start));
      }

      if (index == c_first_continuation_index)
      {
        // E0/F0 need a lower bound to prevent longer-than-necessary encodings.
        // ED needs an upper bound to exclude surrogate values U+D800–U+DFFF.
        // F4 needs an upper bound to prevent values above U+10FFFF.
        const bool invalid_second_byte = (((lead == c_three_byte_lead_min) && (byte < c_e0_second_min)) ||
                                          ((lead == c_surrogate_sensitive_lead) && (byte > c_ed_second_max)) ||
                                          ((lead == c_four_byte_lead_min) && (byte < c_f0_second_min)) ||
                                          ((lead == c_four_byte_lead_max) && (byte > c_f4_second_max)));

        if (invalid_second_byte)
        {
          // The byte has the continuation pattern, but this combination is invalid.
          return decode_result::failure(error::at_offset(error_code::encoding, start));
        }
      }

      // Make room for six more bits, remove the byte's 10 prefix, and append its payload.
      scalar = ((scalar << c_continuation_payload_bits) | static_cast<char32_t>(byte & c_continuation_payload_mask));
    }

    // Commit cursor advancement only after the complete sequence validates.
    source.advance(byte_count);

    return decode_result::success(std::optional<decoded_character>{decoded_character{scalar, start, byte_count}});
  }
} // namespace adaptive_json5
