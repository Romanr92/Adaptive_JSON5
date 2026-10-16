#ifndef NUMBER_TOKEN_HPP_
#define NUMBER_TOKEN_HPP_

#include <cmath>
#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Identify the recognized numeric spelling family.
   * @par Requirements
   * P025; P026; P027; J5-021; J5-022.
   * @note Classification does not perform numeric conversion.
   */
  enum class number_kind
  {
    decimal,
    hexadecimal,
    infinity,
    nan
  };

  /**
   * @brief Describe a recognized numeric token in original input bytes.
   * @par Requirements
   * P025; P026; P027; J5-021; J5-022; J5-041.
   * @note Contains source metadata only and owns no input text.
   * byte_offset and byte_length include any leading sign.
   * Reading the original spelling requires the original input.
   * Owned numeric spelling and exact-value representation belong to P029.
   * A decimal token is not required to fit a machine numeric type.
   */
  struct number_token
  {
      number_kind kind = number_kind::decimal;
      std::size_t byte_offset{};
      std::size_t byte_length{};
  };
} // namespace adaptive_json5

#endif /* NUMBER_TOKEN_HPP_ */
