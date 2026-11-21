#ifndef ADAPTIVE_JSON5_INTERNAL_UNICODE_UNICODE_RANGE_HPP_
#define ADAPTIVE_JSON5_INTERNAL_UNICODE_UNICODE_RANGE_HPP_

#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Inclusive interval in a Unicode property table.
   * @par Requirements
   * P006; J5-006.
   * @note Table entries must be sorted and nonoverlapping.
   */
  struct [[nodiscard]] unicode_range
  {
      std::uint32_t first;
      std::uint32_t last;
  };
} // namespace adaptive_json5
#endif /* ADAPTIVE_JSON5_INTERNAL_UNICODE_UNICODE_RANGE_HPP_ */
