#ifndef IDENTIFIER_TOKEN_HPP_
#define IDENTIFIER_TOKEN_HPP_

#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Own one decoded identifier name and its original source span.
   * @par Requirements
   * P022; P023; J5-019; J5-020; J5-041.
   * @note value owns decoded UTF-16 units independently of caller input.
   * Its storage provider state must outlive the owned value.
   * byte_offset and byte_length describe the original source bytes,
   * including any Unicode escape spellings.
   * contains_escape records whether any character used an escape.
   * This token does not determine whether the name is a value literal.
   */
  struct [[nodiscard]] identifier_token
  {
      decoded_string value{};
      std::size_t byte_offset{};
      std::size_t byte_length{};
      bool contains_escape{};
  };

  /**
   * @brief Describe one validated identifier without owning its decoded units.
   * @par Requirements
   * P022; P023; J5-019; J5-020; J5-046; J5-047.
   * @note byte_offset and byte_length describe the original source bytes,
   * including any Unicode escape spellings.
   * contains_escape records whether any character used an escape.
   * Owns nothing, so it can be produced and discarded without a storage provider.
   */
  struct [[nodiscard]] identifier_measure
  {
      std::size_t units{};
      std::size_t byte_offset{};
      std::size_t byte_length{};
      bool contains_escape{};
  };
} // namespace adaptive_json5

#endif /* IDENTIFIER_TOKEN_HPP_ */
