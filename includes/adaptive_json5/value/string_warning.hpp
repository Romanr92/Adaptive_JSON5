#ifndef ADAPTIVE_JSON5_STRING_WARNING_TYPES_HPP_
#define ADAPTIVE_JSON5_STRING_WARNING_TYPES_HPP_
#include <cstddef>
namespace adaptive_json5
{
  /**
   * @brief Identify advisory conditions in valid string source.
   * @par Requirements
   * P020; J5-017.
   */
  enum class string_warning_code
  {
    raw_line_separator,
    raw_paragraph_separator
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
