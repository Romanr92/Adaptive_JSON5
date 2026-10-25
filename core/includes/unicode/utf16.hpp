#ifndef UTF16_HPP_
#define UTF16_HPP_

#include "adaptive_json5/result.hpp"

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
  struct [[nodiscard]] encoded_utf16
  {
      static constexpr std::size_t c_first_unit_index = 0;
      static constexpr std::size_t c_second_unit_index = 1;
      static constexpr std::size_t c_single_unit_count = 1;
      static constexpr std::size_t c_surrogate_pair_unit_count = 2;

      char16_t units[c_surrogate_pair_unit_count]{};
      std::size_t size = c_first_unit_index;
  };

  /**
   * @brief Convert one Unicode scalar into one or two UTF-16 units.
   * @par Requirements
   * P013; J5-018.
   * @return Encoded units, or encoding error for an invalid scalar.
   * @note Surrogates are rejected as scalar inputs.
   * Escaped UTF-16 units must be preserved separately without this conversion.
   */
  [[nodiscard]] result<encoded_utf16> encode_utf16(char32_t scalar) noexcept;
} // namespace adaptive_json5

#endif // UTF16_HPP_
