// SPDX-License-Identifier: Apache-2.0
#ifndef ADAPTIVE_JSON5_NUMBER_KIND_HPP_
#define ADAPTIVE_JSON5_NUMBER_KIND_HPP_
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

} // namespace adaptive_json5
#endif
