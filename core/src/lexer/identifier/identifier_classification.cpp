#include "lexer/identifier/identifier_classification.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "lexer/identifier/identifier_token.hpp"

#include <cstddef>
#include <string_view>

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
     */
    result<bool> identifier_spelling_matches(const decoded_string& value, std::u16string_view spelling) noexcept
    {
      if (value.size() != spelling.size())
      {
        return result<bool>::success(false);
      }

      for (std::size_t index{}; index < spelling.size(); ++index)
      {
        const auto unit = value.unit_at(index);
        if (!unit.has_value())
        {
          return result<bool>::failure(*unit.error_if());
        }

        if (*unit.value_if() != spelling[index])
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
   */
  result<identifier_classification> classify_identifier(const identifier_token& token, identifier_context context,
                                                        syntax_mode mode) noexcept
  {
    using classification_result = result<identifier_classification>;

    if (context == identifier_context::member_name)
    {
      if (mode == syntax_mode::strict_json)
      {
        return classification_result::failure(error::at_offset(error_code::syntax, token.byte_offset));
      }

      return classification_result::success(identifier_classification::member_name);
    }

    // Decoded equality does not make an escaped name a literal spelling.
    if (token.contains_escape)
    {
      return classification_result::success(identifier_classification::unrecognized_value);
    }

    constexpr std::u16string_view c_spellings[] = {u"null", u"true", u"false"};
    constexpr identifier_classification c_classifications[] = {identifier_classification::null_literal,
                                                               identifier_classification::true_literal,
                                                               identifier_classification::false_literal};

    for (std::size_t index{}; index < sizeof(c_spellings) / sizeof(c_spellings[0]); ++index)
    {
      const auto matches = detail::identifier_spelling_matches(token.value, c_spellings[index]);
      if (!matches.has_value())
      {
        return classification_result::failure(*matches.error_if());
      }

      if (*matches.value_if())
      {
        return classification_result::success(c_classifications[index]);
      }
    }

    return classification_result::success(identifier_classification::unrecognized_value);
  }
} // namespace adaptive_json5
