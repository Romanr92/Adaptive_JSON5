#ifndef ADAPTIVE_JSON5_STRING_WARNING_TYPES_HPP_
#define ADAPTIVE_JSON5_STRING_WARNING_TYPES_HPP_
#include <cstddef>
namespace adaptive_json5
{
  /**
   * @brief Identify advisory conditions in valid string source.
   * @par Requirements
   * P020; P041; J5-017; J5-018.
   * @note raw_line_separator and raw_paragraph_separator identify raw U+2028
   * and U+2029 in JSON5 strings only. unpaired_surrogate_escape identifies a
   * Unicode escape whose decoded surrogate has no adjacent partner, in either
   * syntax mode. It covers the complete escape and does not change decoded units.
   */
  enum class string_warning_code
  {
    raw_line_separator,       // Raw U+2028 in a JSON5 string.
    raw_paragraph_separator,  // Raw U+2029 in a JSON5 string.
    unpaired_surrogate_escape // A \u escape decodes to an unpaired surrogate, in either mode.
  };

  /**
   * @brief Describe a string warning using its original source span.
   * @par Requirements
   * P020; J5-017.
   * @note Stored inline without allocation or ownership of input bytes.
   * Offsets and lengths count original UTF-8 bytes.
   */
  struct [[nodiscard]] string_warning
  {
      string_warning_code code;
      std::size_t byte_offset;
      std::size_t byte_length;
  };

} // namespace adaptive_json5
#endif
