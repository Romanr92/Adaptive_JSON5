#ifndef STRING_LEXER_HPP_
#define STRING_LEXER_HPP_

#include "cursor.hpp"
#include "decoded_string_builder.hpp"
#include "error.hpp"
#include "options.hpp"
#include "result.hpp"
#include "storage.hpp"
#include "string_boundary.hpp"
#include "string_escape.hpp"
#include "string_token.hpp"
#include "string_warning.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Deliver warnings from an already validated string body.
     * @par Requirements
     * P021; J5-017.
     * @param source Copy of the cursor immediately after the opening quote.
     * @note Requires the same unchanged input validated by the first pass.
     * The observer must satisfy string_warning_observer's lifetime contract.
     * Performs no allocation and does not modify the caller's cursor.
     */
  } // namespace detail
} // namespace adaptive_json5

#endif /* STRING_LEXER_HPP_ */
