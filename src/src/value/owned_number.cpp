// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/value/owned_number.hpp"

#include "adaptive_json5/detail/input/cursor.hpp"
#include "adaptive_json5/detail/lexer/number/number_lexer.hpp"
#include "adaptive_json5/detail/lexer/number/number_token.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Adopt validated numeric text and its spelling family.
   * @par Requirements
   * P029; J5-034; J5-040.
   * @note Storage ownership transfers without another allocation.
   */
  owned_number::owned_number(owned_block&& storage, number_kind kind) noexcept
      : m_storage(std::move(storage)), m_kind(kind)
  {
  }

  /**
   * @brief Validate and copy exactly one numeric spelling.
   * @par Requirements
   * P029; J5-034; J5-038; J5-040; J5-046.
   * @param text Borrowed numeric bytes, with no surrounding trivia.
   * @param mode Syntax used to validate the spelling.
   * @param storage Caller-owned fallible storage provider.
   * @param max_bytes Maximum owned spelling length in bytes.
   * @return An owned number or an explicit error.
   * @note Checks the component byte limit before scanning or allocation.
   * Located validation errors are relative to text, not a containing document.
   * Allocation failures preserve their error code without a source location.
   * Failure does not mutate input or retain a partial allocation.
   * This limit does not replace P031 accounting/work/exponent guards.
   */
  result<owned_number> owned_number::copy_spelling(input_view text, syntax_mode mode, const storage_callbacks& storage,
                                                   std::size_t max_bytes) noexcept
  {
    constexpr std::size_t c_spelling_start_offset = 0;
    using number_result = result<owned_number>;

    if ((text.data == nullptr) && (text.size != 0))
    {
      return number_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    if (text.size > max_bytes)
    {
      return number_result::failure(error::at_offset(error_code::resource_limit, c_spelling_start_offset));
    }

    cursor source{text};
    const auto recognized = consume_number(source, mode);
    if (!recognized.has_value())
    {
      return number_result::failure(*recognized.error_if());
    }

    if (!recognized.value_if()->has_value())
    {
      return number_result::failure(error::at_offset(error_code::syntax, c_spelling_start_offset));
    }

    // Reject surrounding trivia or additional tokens instead of copying only a prefix.
    if (!source.is_at_end())
    {
      return number_result::failure(error::at_offset(error_code::syntax, source.offset()));
    }

    auto allocation = allocate_block(storage, text.size, alignof(char));
    if (!allocation.has_value())
    {
      // Allocation errors have no location relative to the numeric spelling.
      return number_result::failure(error::without_location(allocation.error_if()->code));
    }

    std::memcpy(allocation.value_if()->data(), text.data, text.size);

    const number_kind kind = (**recognized.value_if()).kind;
    return number_result::success(owned_number{std::move(*allocation.value_if()), kind});
  }

  /**
   * @brief Borrow the exact original numeric spelling.
   * @par Requirements
   * P029; J5-034; J5-038; J5-040.
   * @return Immutable bytes without a terminating NUL.
   * @note Empty after ownership has been moved away.
   * The view becomes invalid when its owning allocation is released.
   */
  input_view owned_number::spelling() const noexcept
  {
    return input_view{static_cast<const char*>(m_storage.data()), m_storage.size()};
  }

  /**
   * @brief Return the retained numeric spelling family.
   * @par Requirements
   * P029; J5-034; J5-038.
   * @note Finite decimal spellings remain decimal regardless of
   * whether a later machine-number conversion would overflow.
   * A moved-from object's kind is metadata only.
   */
  number_kind owned_number::kind() const noexcept
  {
    return m_kind;
  }
} // namespace adaptive_json5
