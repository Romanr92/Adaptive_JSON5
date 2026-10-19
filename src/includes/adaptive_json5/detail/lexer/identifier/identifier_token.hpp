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
  struct identifier_token
  {
      decoded_string value{};
      std::size_t byte_offset{};
      std::size_t byte_length{};
      bool contains_escape{};
  };
} // namespace adaptive_json5

#endif /* IDENTIFIER_TOKEN_HPP_ */
