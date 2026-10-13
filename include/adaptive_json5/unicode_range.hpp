#ifndef UNICODE_RANGE_HPP_
#define UNICODE_RANGE_HPP_

namespace adaptive_json5
{
  /**
   * @brief Inclusive interval in a Unicode property table.
   * @par Requirements
   * P006; J5-006.
   * @note Table entries must be sorted and nonoverlapping.
   */
  struct unicode_range
  {
      char32_t first;
      char32_t last;
  };
} // namespace adaptive_json5
#endif /* UNICODE_RANGE_HPP_ */
