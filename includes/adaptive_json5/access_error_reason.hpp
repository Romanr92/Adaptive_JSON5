#ifndef ADAPTIVE_JSON5_ACCESS_ERROR_REASON_HPP_
#define ADAPTIVE_JSON5_ACCESS_ERROR_REASON_HPP_

#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Describe why a value access or an argument was rejected.
   * @par Requirements
   * J5-031; J5-039; J5-043; issue #125.
   * @par Reasons
   * - wrong_type: The value has another kind, e.g. a member lookup on an array.
   * - absent_member: The object has no member with the requested name.
   * - duplicate_member: The object keeps the requested name more than once, so no single
   *   member can be chosen (J5-031).
   * - index_out_of_range: An index or value handle lies outside its container or storage.
   * - invalid_argument: An argument breaks the call's contract, e.g. a null pointer with a
   *   nonzero size, an invalid alignment or a default-constructed view.
   * @note The reason refines error_code::wrong_type_or_access; it never replaces the error code.
   * Failed library invariants use error_code::internal_error instead.
   */
  enum class access_error_reason : std::uint8_t
  {
    wrong_type,
    absent_member,
    duplicate_member,
    index_out_of_range,
    invalid_argument
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_ACCESS_ERROR_REASON_HPP_ */
