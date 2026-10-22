#ifndef DECODED_NAME_EQUAL_HPP_
#define DECODED_NAME_EQUAL_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

namespace adaptive_json5
{
  /**
   * @brief Compare decoded member names by their exact UTF-16 units.
   * @par Requirements
   * P030; J5-031; J5-033; J5-039.
   * @param left First decoded name.
   * @param right Second decoded name.
   * @return Exact equality or an explicit unit-access failure.
   * @note Performs no allocation, normalization or case folding.
   * Embedded NUL and surrogate units participate unchanged.
   * Work is linear in the number of units examined.
   */
  [[nodiscard]] result<bool> decoded_name_equal(const decoded_string& left, const decoded_string& right) noexcept;
} // namespace adaptive_json5

#endif /* DECODED_NAME_EQUAL_HPP_ */
