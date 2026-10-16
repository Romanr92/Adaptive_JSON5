#ifndef VALUE_KIND_HPP_
#define VALUE_KIND_HPP_

namespace adaptive_json5
{
  /**
   * @brief Identify a value's JSON/JSON5 data type without coercion.
   * @par Requirements
   * P030; J5-028; J5-039.
   * @note Boolean values remain distinct from numeric values.
   * Decimal, hexadecimal, Infinity and NaN all have kind number;
   * owned_number retains their numeric spelling family separately.
   * Null is a value, not an indication of an absent object member.
   */
  enum class value_kind
  {
    null,
    boolean,
    string,
    number,
    array,
    object
  };
} // namespace adaptive_json5

#endif /* VALUE_KIND_HPP_ */
