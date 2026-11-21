#ifndef ADAPTIVE_JSON5_INTERNAL_PARSER_PARSER_FRAME_HPP_
#define ADAPTIVE_JSON5_INTERNAL_PARSER_PARSER_FRAME_HPP_

#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Identify the next grammar operation for one open container.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-027.
     * @note Separate initial and post-comma states allow the parser
     * to distinguish empty containers from trailing commas.
     */
    enum class parser_state : std::uint8_t
    {
      array_first_or_end,
      array_value_after_comma,
      array_comma_or_end,
      object_first_name_or_end,
      object_name_after_comma,
      object_colon,
      object_value,
      object_comma_or_end
    };

    /**
     * @brief Retain the parsing state of one open array or object.
     * @par Requirements
     * P033; P034; P035; J5-013; J5-048.
     * @note The parser must initialize container and state before use.
     * Container indices stay valid for the whole parse.
     * pending_name owns a decoded object name until member attachment.
     * This type neither allocates frame storage nor enforces budgets.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
    // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
    // implicit members that are never odr-used are not in its database.
    // codeql::misra_deviation_next_line(DEV-020.41)
    struct [[nodiscard]] parser_frame
    {
        value_index container{};
        parser_state state = parser_state::array_first_or_end;
        std::size_t opening_offset{};
        std::optional<decoded_string> pending_name{};
        std::size_t name_offset{};
        std::size_t name_length{};
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_INTERNAL_PARSER_PARSER_FRAME_HPP_ */
