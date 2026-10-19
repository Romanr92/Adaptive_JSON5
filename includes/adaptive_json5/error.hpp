#ifndef ADAPTIVE_JSON5_ERROR_HPP_
#define ADAPTIVE_JSON5_ERROR_HPP_

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Stable categories for explicit library failures.
   * @par Requirements
   * P002; J5-043.
   */
  enum class error_code
  {
    syntax = 1,
    encoding = 2,
    duplicate_policy = 3,
    wrong_type_or_access = 4,
    numeric_conversion = 5,
    resource_limit = 6,
    allocation_failure = 7
  };

  /**
   * @brief Minimal error information stored without allocation.
   * @par Requirements
   * P002; J5-003; J5-043; J5-045.
   * @note byte_offset is zero-based in the original input.
   * When has_byte_offset is false, byte_offset is unused and set to zero.
   */
  struct error
  {
      error_code code;
      std::size_t byte_offset;
      bool has_byte_offset;

      static constexpr error at_offset(error_code code, std::size_t byte_offset) noexcept
      {
        return error{code, byte_offset, true};
      }

      static constexpr error without_location(error_code code) noexcept
      {
        return error{code, 0, false};
      }
  };

} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_ERROR_HPP_ */
