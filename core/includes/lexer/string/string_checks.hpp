#ifndef STRING_CHECKS_HPP_
#define STRING_CHECKS_HPP_

#include "adaptive_json5/options.hpp"

namespace adaptive_json5
{
  /**
   * @brief Secure-mode checks applied while walking one string body.
   * @par Requirements
   * CSR-07; issue #145.
   * @note All false by default, which keeps standard behavior. Performs no allocation.
   * A unit U+0000 is checked as a zero byte first and as a control character only when
   * reject_zero_bytes is false, so it gets security_reason::zero_byte when both are set.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.27)
  struct string_checks
  {
      bool reject_zero_bytes{};         // U+0000, raw or escaped.
      bool reject_control_characters{}; // U+0000..U+001F, raw or escaped; names only.
      bool reject_unpaired_surrogate{}; // Instead of the unpaired_surrogate_escape warning.
  };

  /**
   * @brief Select the checks for a string value.
   * @par Requirements
   * CSR-07; issue #145.
   * @param options Caller's parse options.
   * @return Zero-byte and surrogate checks from options; control characters are not checked
   * in values.
   * @par ID SWU-LEXER-052
   */
  [[nodiscard]] constexpr string_checks value_string_checks(const parse_options& options) noexcept
  {
    return string_checks{options.zero_bytes == zero_byte_policy::reject, false,
                         options.unpaired_surrogates == unpaired_surrogate_policy::reject};
  }

  /**
   * @brief Select the checks for a quoted member name.
   * @par Requirements
   * CSR-07; issue #145.
   * @param options Caller's parse options.
   * @return The value checks plus the member-name control-character check.
   * @note Identifier names need no checks: their grammar already excludes these characters.
   * @par ID SWU-LEXER-053
   */
  [[nodiscard]] constexpr string_checks name_string_checks(const parse_options& options) noexcept
  {
    string_checks checks = value_string_checks(options);
    checks.reject_control_characters = (options.name_controls == name_control_policy::reject);
    return checks;
  }
} // namespace adaptive_json5

#endif /* STRING_CHECKS_HPP_ */
