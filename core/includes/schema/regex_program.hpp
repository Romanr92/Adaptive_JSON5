#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_REGEX_PROGRAM_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_REGEX_PROGRAM_HPP_

#include <cstddef>
#include <cstdint>

namespace adaptive_json5::detail
{
  /**
   * @brief Instruction kinds of a compiled pattern (Pike VM program).
   * @par Requirements
   * P049; J5-060; J5-065.
   */
  enum class regex_op : std::uint8_t
  {
    literal,           // value: the code point to match.
    any,               // Any code point except a line terminator.
    char_class,        // first/second: range table slice; negated inverts the class.
    split,             // Continue at first and at second (first preferred).
    jump,              // Continue at first.
    line_start,        // ^: only at the start of the text (no multiline flag in JSON Schema).
    line_end,          // $: only at the end of the text.
    word_boundary,     // \b
    not_word_boundary, // \B
    match              // The pattern matched.
  };

  /**
   * @brief One instruction; jump targets are absolute indices in the instruction table.
   * @par Requirements
   * P049; J5-060.
   */
  struct regex_inst
  {
      regex_op op{regex_op::match};
      bool negated{};
      std::uint32_t value{};
      std::size_t first{};
      std::size_t second{};
  };

  /**
   * @brief An inclusive code point range of a character class.
   * @par Requirements
   * P049; J5-060.
   */
  struct regex_range
  {
      std::uint32_t low{};
      std::uint32_t high{};
  };

  /**
   * @brief Read access to the instruction and range tables of compiled patterns.
   * @par Requirements
   * P049; J5-060.
   */
  struct regex_tables
  {
      const regex_inst* code{};
      const regex_range* ranges{};
  };
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_REGEX_PROGRAM_HPP_ */
