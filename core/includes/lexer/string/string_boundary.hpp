#ifndef STRING_BOUNDARY_HPP_
#define STRING_BOUNDARY_HPP_

#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "input/cursor.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Identify the delimiter that opened a string.
   * @par Requirements
   * P014; J5-014.
   */
  enum class string_quote : char32_t
  {
    single = U'\'',
    double_quote = U'"'
  };

  /**
   * @brief Classify the next source character inside a string.
   * @par Requirements
   * P014; J5-014.
   */
  enum class string_piece_kind
  {
    character,
    closing_quote,
    escape_start
  };

  /**
   * @brief Describe one string piece and its original source span.
   * @par Requirements
   * P014; J5-014.
   * @note scalar is the original source character, not a decoded escape.
   * No input storage is owned.
   */
  struct [[nodiscard]] string_piece
  {
      string_piece_kind kind;
      char32_t scalar;
      std::size_t byte_offset;
      std::size_t byte_length;
  };

  /**
   * @brief Consume a permitted opening quote if present.
   * @par Requirements
   * P014; J5-014.
   * @return The delimiter, successful nullopt before nonquote input or EOF,
   * or syntax error for a single quote in strict JSON.
   * @note Failure and no-match leave the cursor unchanged.
   * Does not skip trivia or validate nonquote input.
   */
  [[nodiscard]] result<std::optional<string_quote>> consume_string_opening(cursor& source, syntax_mode mode) noexcept;

  /**
   * @brief Consume one ordinary character, closing quote or escape introducer.
   * @par Requirements
   * P014; J5-014.
   * @param quote Delimiter returned by consume_string_opening in the same mode.
   * @return One source piece, an encoding error, or a located syntax error.
   * EOF reports syntax at the current end offset.
   * @note Call only inside an open string, outside an escape sequence.
   * Success consumes exactly one source character; failure consumes nothing.
   * escape_start consumes the backslash only: the caller must decode its
   * following escape before calling this function again.
   * Raw U+2028/U+2029 are characters; warning handling belongs to P020.
   * Whole-string rollback belongs to the later composing lexer.
   */
  [[nodiscard]] result<string_piece> consume_string_piece(cursor& source, string_quote quote,
                                                          syntax_mode mode) noexcept;
} // namespace adaptive_json5

#endif /* STRING_BOUNDARY_HPP_ */
