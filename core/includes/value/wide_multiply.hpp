#ifndef ADAPTIVE_JSON5_DETAIL_WIDE_MULTIPLY_HPP_
#define ADAPTIVE_JSON5_DETAIL_WIDE_MULTIPLY_HPP_

#include <cstdint>

namespace adaptive_json5::detail
{
  /**
   * @brief Exact 128-bit unsigned product split into two 64-bit halves.
   * @par Requirements
   * J5-037; J5-039.
   */
  struct product_128
  {
      std::uint64_t high; // Bits 127..64.
      std::uint64_t low;  // Bits 63..0.
  };

  /**
   * @brief Multiply two 64-bit values exactly, without a 128-bit integer type.
   * @par Requirements
   * J5-037; J5-039.
   * @return The full 128-bit product.
   * @note Portable C++17: four 32x32->64 partial products. No intermediate sum can overflow,
   * because the largest one is (2^32-1)^2 + 2(2^32-1) = 2^64-1.
   */
  [[nodiscard]] constexpr product_128 multiply_64x64_to_1289(std::uint64_t left, std::uint64_t right) noexcept
  {
    constexpr std::uint64_t c_low_32_mask = 0xFFFFFFFFU;
    constexpr std::uint32_t c_half_bits = 32U;

    const std::uint64_t left_low = left & c_low_32_mask;
    const std::uint64_t left_high = left >> c_half_bits;
    const std::uint64_t right_low = right & c_low_32_mask;
    const std::uint64_t right_high = right >> c_half_bits;

    // Each partial product of two 32-bit halves fits in 64 bits.
    const std::uint64_t low_low = left_low * right_low;
    const std::uint64_t high_low = left_high * right_low;
    const std::uint64_t low_high = left_low * right_high;
    const std::uint64_t high_high = left_high * right_high;

    // Middle column: carry out of low_low, the low half of high_low, and all of low_high.
    const std::uint64_t middle = (low_low >> c_half_bits) + (high_low & c_low_32_mask) + low_high;

    return product_128{high_high + (high_low >> c_half_bits) + (middle >> c_half_bits),
                       (middle << c_half_bits) | (low_low & c_low_32_mask)};
  }
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_DETAIL_WIDE_MULTIPLY_HPP_ */
