#include "lexer/identifier/identifier_classification.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "lexer/identifier/identifier_token.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Compare decoded content with an exact UTF-16 spelling.
     * @par Requirements
     * P024; J5-023.
     * @note Performs no allocation, normalization or case folding.
     * Checked-access failures are propagated explicitly.
     * @par ID SWU-LEXER-005
     */
    result<bool> identifier_spelling_matches(const decoded_string& value, const std::uint16_t* spelling,
                                             std::size_t length) noexcept
    {
      if (value.size() != length)
      {
        return result<bool>::success(false);
      }

      // Compare from the last unit down; the order does not matter for equality.
      std::size_t index = length;
      while (index != std::size_t{})
      {
        --index;
        const auto unit = value.unit_at(index);
        if (!unit.has_value())
        {
          return result<bool>::failure(*unit.error_if());
        }

        if (static_cast<std::uint32_t>(*unit.value_if()) != static_cast<std::uint32_t>(spelling[index]))
        {
          return result<bool>::success(false);
        }
      }

      return result<bool>::success(true);
    }
  } // namespace detail

  /**
   * @brief Classify an identifier as a member name or primitive literal.
   * @par Requirements
   * P024; J5-019; J5-023.
   * @param token Complete token produced by identifier recognition.
   * @param context Whether a member name or value is expected.
   * @param mode Selected input syntax.
   * @return Classification or an explicit error.
   * @note Strict JSON rejects unquoted member names at the token start.
   * Value literals require exact lowercase spelling without escapes.
   * Other value spellings return unrecognized_value, not a string value.
   * Input, token ownership and cursor position are unchanged.
   * Performs no allocation.
   * @par ID SWU-LEXER-006
   */
  result<identifier_classification> classify_identifier(const identifier_token& token, identifier_context context,
                                                        syntax_mode mode) noexcept
  {
    using classification_result = result<identifier_classification>;

    if (context == identifier_context::member_name)
    {
      if (mode == syntax_mode::strict_json)
      {
        return classification_result::failure(
          error::syntax_at(token.byte_offset, syntax_error_reason::not_allowed_in_strict_json));
      }

      return classification_result::success(identifier_classification::member_name);
    }

    // Decoded equality does not make an escaped name a literal spelling.
    if (token.contains_escape)
    {
      return classification_result::success(identifier_classification::unrecognized_value);
    }

    // UTF-16 code units of the three literal spellings.
    static constexpr std::array<std::uint16_t, 4U> c_null{0x6EU, 0x75U, 0x6CU, 0x6CU};         // "null"
    static constexpr std::array<std::uint16_t, 4U> c_true{0x74U, 0x72U, 0x75U, 0x65U};         // "true"
    static constexpr std::array<std::uint16_t, 5U> c_false{0x66U, 0x61U, 0x6CU, 0x73U, 0x65U}; // "false"

    // Check the three literal spellings in turn.
    const auto is_null = detail::identifier_spelling_matches(token.value, c_null.data(), c_null.size());
    if (!is_null.has_value())
    {
      return classification_result::failure(*is_null.error_if());
    }

    if (*is_null.value_if())
    {
      return classification_result::success(identifier_classification::null_literal);
    }

    const auto is_true = detail::identifier_spelling_matches(token.value, c_true.data(), c_true.size());
    if (!is_true.has_value())
    {
      return classification_result::failure(*is_true.error_if());
    }

    if (*is_true.value_if())
    {
      return classification_result::success(identifier_classification::true_literal);
    }

    const auto is_false = detail::identifier_spelling_matches(token.value, c_false.data(), c_false.size());
    if (!is_false.has_value())
    {
      return classification_result::failure(*is_false.error_if());
    }

    if (*is_false.value_if())
    {
      return classification_result::success(identifier_classification::false_literal);
    }

    return classification_result::success(identifier_classification::unrecognized_value);
  }
} // namespace adaptive_json5
