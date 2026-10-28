#ifndef NUMBER_ACCESS_HPP_
#define NUMBER_ACCESS_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Internal parser operations that create owned numbers from validated spellings.
     * @par Requirements
     * P029; P032; J5-034; J5-046; J5-049.
     * @note Not a supported public API; never installed.
     */
    struct number_access
    {
        /**
         * @brief Copy a spelling the number lexer already validated, without lexing it again.
         * @par Requirements
         * P029; P032; J5-034; J5-046.
         * @param text Spelling recognized by the number lexer, exactly one complete number.
         * @param kind Spelling family the lexer reported for text.
         * @param storage Caller-owned fallible storage provider.
         * @param max_bytes Maximum owned spelling length in bytes.
         * @return An owned number, or the same limit/allocation/access errors as
         * owned_number::copy_spelling, located relative to text.
         * @note Skips only copy_spelling's validation pass (ADR 0003, decision 10, E); the
         * caller guarantees text came from the lexer. Untrusted text must use copy_spelling.
         * A kind that does not match the spelling's leading bytes fails with wrong_type_or_access.
         */
        [[nodiscard]] static result<owned_number> copy_validated_spelling(input_view text, number_kind kind,
                                                                          const storage_callbacks& storage,
                                                                          std::size_t max_bytes) noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* NUMBER_ACCESS_HPP_ */
