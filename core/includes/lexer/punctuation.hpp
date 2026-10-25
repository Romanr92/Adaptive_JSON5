#ifndef ADAPTIVE_JSON5_PUNCTUATION_HPP_
#define ADAPTIVE_JSON5_PUNCTUATION_HPP_

#include "input/cursor.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Structural punctuation recognized in both supported syntax modes.
   * @par Requirements
   * P012; J5-013.
   */
  enum class punctuation_kind
  {
    left_brace,
    right_brace,
    left_bracket,
    right_bracket,
    colon,
    comma
  };

  /**
   * @brief One structural punctuation token and its original byte span.
   * @par Requirements
   * P012; J5-013.
   * @note Contains no ownership of the input.
   */
  struct [[nodiscard]] punctuation_token
  {
      punctuation_kind kind;
      std::size_t byte_offset;
      std::size_t byte_length;
  };

  /**
   * @brief Consume one structural punctuation character if present.
   * @par Requirements
   * P012; J5-013.
   * @param source Cursor positioned outside strings and comments.
   * @return A token, or std::nullopt at EOF or before nonpunctuation input.
   * @note No match leaves source unchanged.
   * Each recognized punctuator occupies exactly one byte.
   * Does not skip trivia or validate nonpunctuation input.
   * Requires no allocation and throws no exceptions.
   */
  [[nodiscard]] std::optional<punctuation_token> consume_punctuation(cursor& source) noexcept;
} // namespace adaptive_json5

#endif // ADAPTIVE_JSON5_PUNCTUATION_HPP_
