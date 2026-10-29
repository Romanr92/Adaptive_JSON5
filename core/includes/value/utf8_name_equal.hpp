#ifndef UTF8_NAME_EQUAL_HPP_
#define UTF8_NAME_EQUAL_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <string_view>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Compare a decoded member name with a UTF-8 name, without allocating.
     * @par Requirements
     * P030; J5-031; J5-033; J5-046.
     * @param name Decoded member name (UTF-16 units) stored in the document.
     * @param utf8 Name given by the caller, in UTF-8.
     * @return True when both spell exactly the same characters, false otherwise, or an
     * encoding error when the UTF-8 bytes reached before a difference are not valid.
     * @note Exact comparison: no normalization and no case folding, as reject_duplicates
     * uses. Stops at the first difference. Uses no storage.
     */
    [[nodiscard]] result<bool> utf8_name_equal(const decoded_string& name, std::string_view utf8) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* UTF8_NAME_EQUAL_HPP_ */
