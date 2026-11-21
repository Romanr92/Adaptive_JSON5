#include "value/number_access.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    namespace
    {
      /**
       * @brief Classify a validated spelling's family from its first bytes only.
       * @par Requirements
       * P029; J5-034.
       * @param text Non-empty numeric spelling.
       * @return The family the full lexer would report for a valid spelling.
       * @note Constant time: looks at an optional sign and at most the next two bytes.
       * @par ID SWU-VALUE-078
       */
      number_kind leading_kind(input_view text) noexcept
      {
        std::size_t position{};

        if ((text.data[position] == '+') || (text.data[position] == '-'))
        {
          ++position;
        }

        if (position >= text.size)
        {
          return number_kind::decimal;
        }

        const char first = text.data[position];

        if (first == 'I')
        {
          return number_kind::infinity;
        }

        if (first == 'N')
        {
          return number_kind::nan;
        }

        const bool hexadecimal_prefix = (first == '0') && ((position + 1) < text.size) &&
                                        ((text.data[position + 1] == 'x') || (text.data[position + 1] == 'X'));

        return hexadecimal_prefix ? number_kind::hexadecimal : number_kind::decimal;
      }
    } // namespace

    /**
     * @brief Copy a spelling the number lexer already validated, without lexing it again.
     * @par Requirements
     * P029; P032; J5-034; J5-046.
     * @param text Spelling recognized by the number lexer, exactly one complete number.
     * @param kind Spelling family the lexer reported for text.
     * @param storage Caller-owned fallible storage provider.
     * @param max_bytes Maximum owned spelling length in bytes.
     * @return An owned number or the same errors as owned_number::copy_spelling.
     * @par ID SWU-VALUE-079
     */
    result<owned_number> number_access::copy_validated_spelling(input_view text, number_kind kind,
                                                                const storage_callbacks& storage,
                                                                std::size_t max_bytes) noexcept
    {
      constexpr std::size_t c_spelling_start_offset = 0;
      using number_result = result<owned_number>;

      // An empty spelling cannot be a number the lexer recognized.
      if ((text.data == nullptr) || (text.size == 0))
      {
        return number_result::failure(error::internal_fault());
      }

      // Same limit check and location as copy_spelling, so the parser reports the same error.
      if (text.size > max_bytes)
      {
        return number_result::failure(error::limit_at(c_spelling_start_offset, resource_limit_reason::number_bytes));
      }

      // A cheap consistency check keeps rejecting mismatched token metadata, as the old re-lex did.
      if (leading_kind(text) != kind)
      {
        return number_result::failure(error::internal_fault());
      }

      auto allocation = allocate_block(storage, text.size, alignof(char));

      if (!allocation.has_value())
      {
        // Allocation errors have no location relative to the numeric spelling; unlocated()
        // keeps their code and reason, e.g. the owned_bytes limit.
        return number_result::failure(allocation.error_if()->unlocated());
      }

      // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
      // MISRA justification: std::memcpy copies bytes between non-overlapping buffers whose sizes were checked
      // when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object
      // representation to or from untyped storage without alignment or aliasing assumptions.
      // codeql::misra_deviation_next_line(DEV-023.05)
      static_cast<void>(std::memcpy(allocation.value_if()->data(), text.data, text.size));

      // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
      // MISRA justification: The object is moved exactly once in this expression and is not read again
      // afterwards; the reported accesses are the operand expressions that form this same move (for example
      // value_if() on a result that still holds its value).
      // codeql::misra_deviation_next_line(DEV-024.10)
      return number_result::success(owned_number{std::move(*allocation.value_if()), kind});
    }
  } // namespace detail
} // namespace adaptive_json5
