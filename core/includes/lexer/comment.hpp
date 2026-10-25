#ifndef ADAPTIVE_JSON5_COMMENT_HPP_
#define ADAPTIVE_JSON5_COMMENT_HPP_

#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Original byte position and length of a recognized comment.
   * @par Requirements
   * P009; P010; J5-012.
   * @note Describes input bytes without copying or owning them.
   * A line-comment span includes // but excludes its line terminator.
   */
  struct [[nodiscard]] comment_span
  {
      std::size_t byte_offset;
      std::size_t byte_length;
  };

  /**
   * @brief Consume a // comment if one begins at the current position.
   * @par Requirements
   * P009; J5-012.
   * @param source Cursor over borrowed UTF-8 input.
   * @return A comment span, successful std::nullopt when no comment starts
   * here, or an explicit encoding error.
   * @note Leaves the terminating line break unread.
   * No match or encoding failure leaves source unchanged.
   * Requires no allocation and throws no exceptions.
   */
  [[nodiscard]] result<std::optional<comment_span>> consume_line_comment(cursor& source) noexcept;

  /**
   * @brief Consume a block comment through its first closing delimiter.
   * @par Requirements
   * P010; J5-012.
   * @param source Cursor over borrowed UTF-8 input.
   * @return A comment span, successful std::nullopt when no block comment
   * begins here, or an explicit encoding or syntax error.
   * @note Includes both delimiters and does not nest comments.
   * An unterminated comment reports a syntax error at its opening slash.
   * No match or failure leaves source unchanged.
   * Requires no allocation and throws no exceptions.
   * This recognizer does not enforce strict JSON mode.
   */
  [[nodiscard]] result<std::optional<comment_span>> consume_block_comment(cursor& source) noexcept;

} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_COMMENT_HPP_ */
