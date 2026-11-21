#include "unicode/utf16_decode.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Decode one scalar at a UTF-16 unit index.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @return A scalar and consumed unit count, wrong_type_or_access (index_out_of_range) for an
   * invalid index, or encoding error for an unpaired surrogate.
   * @note Does not allocate, modify the string, or advance caller state.
   * Errors have no original input byte offset: index counts decoded units.
   * @par ID SWU-UNICODE-007
   */
  result<decoded_utf16> decode_utf16(const decoded_string& source, std::size_t index) noexcept
  {
    constexpr std::uint32_t c_high_surrogate_min = 0xD800;
    constexpr std::uint32_t c_high_surrogate_max = 0xDBFF;
    constexpr std::uint32_t c_low_surrogate_min = 0xDC00;
    constexpr std::uint32_t c_low_surrogate_max = 0xDFFF;
    constexpr std::uint32_t c_high_surrogate_base = 0xD7C0U;
    constexpr std::uint32_t c_surrogate_payload_bits = 10;
    constexpr std::size_t c_single_unit_count = 1;
    constexpr std::size_t c_surrogate_pair_unit_count = 2;

    const auto first = source.unit_at(index);

    if (!first.has_value())
    {
      return result<decoded_utf16>::failure(*first.error_if());
    }

    const auto leading = static_cast<std::uint32_t>(*first.value_if());

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

    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: index is a valid unit index below the unit count, so adding one is at most the unit
    // count.
    // codeql::misra_deviation_next_line(DEV-004.143)
    const auto second = source.unit_at(index + c_single_unit_count);
    if (!second.has_value())
    {
      return result<decoded_utf16>::failure(*second.error_if());
    }

    const auto trailing = static_cast<std::uint32_t>(*second.value_if());
    if ((trailing < c_low_surrogate_min) || (trailing > c_low_surrogate_max))
    {
      return result<decoded_utf16>::failure(error::without_location(error_code::encoding));
    }

    // Combine the two ten-bit payloads. Subtracting c_high_surrogate_base (0xD800 - 0x40)
    // instead of 0xD800 adds the 0x10000 offset, and the parts do not overlap, so | joins them.
    const std::uint32_t scalar =
      ((leading - c_high_surrogate_base) << c_surrogate_payload_bits) | (trailing - c_low_surrogate_min);

    return result<decoded_utf16>::success(decoded_utf16{scalar, c_surrogate_pair_unit_count});
  }
} // namespace adaptive_json5
