#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_EXACT_NUMBER_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_EXACT_NUMBER_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/owned_number.hpp"

namespace adaptive_json5::detail
{
  /**
   * @brief Report whether a finite number is strictly greater than zero.
   * @par Requirements
   * P048; J5-054; J5-059.
   * @return The verdict, or internal_error for a non-finite or malformed spelling.
   * @par ID SWU-SCHEMA-216
   */
  [[nodiscard]] result<bool, schema_error> is_positive(const owned_number& number) noexcept;

  /**
   * @brief Report whether value is an integer multiple of divisor, exactly (no rounding).
   * @par Requirements
   * P048; J5-057; J5-059; Draft-07 Validation section 6.2.1.
   * @param divisor A number greater than zero.
   * @param scratch Storage for the big integers, released before returning.
   * @return The verdict, resource_limit when the scaled numbers would need more work than
   * the parser's default work limit, or a storage failure.
   * @note Both numbers are read as an integer significand times a power of ten (hexadecimal
   * as an integer), scaled to a common exponent and compared through an exact remainder.
   * The sign is ignored. Zero is a multiple of every divisor.
   * @par ID SWU-SCHEMA-217
   */
  [[nodiscard]] result<bool, schema_error> is_multiple_of(const owned_number& value, const owned_number& divisor,
                                                          const storage_callbacks& scratch) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_EXACT_NUMBER_HPP_ */
