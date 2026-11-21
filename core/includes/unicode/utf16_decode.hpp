#ifndef UTF16_DECODE_HPP_
#define UTF16_DECODE_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Hold one scalar decoded from stored UTF-16 units.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @note unit_count identifies how far the caller should advance.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.46)
  struct [[nodiscard]] decoded_utf16
  {
      std::uint32_t value{};
      std::size_t unit_count{};
  };

  /**
   * @brief Decode one scalar at a UTF-16 unit index.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @return A scalar and consumed unit count, wrong_type_or_access (index_out_of_range) for an
   * invalid index, or encoding error for an unpaired surrogate.
   * @note Does not allocate, modify the string, or advance caller state.
   * Errors have no original input byte offset: index counts decoded units.
   * @par ID SWU-UNICODE-007
   */
  [[nodiscard]] result<decoded_utf16> decode_utf16(const decoded_string& source, std::size_t index) noexcept;
} // namespace adaptive_json5

#endif // UTF16_DECODE_HPP_
