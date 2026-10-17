#ifndef OWNED_NUMBER_HPP_
#define OWNED_NUMBER_HPP_

#include "adaptive_json5/cursor.hpp"
#include "adaptive_json5/options.hpp"
#include "error.hpp"
#include "input.hpp"
#include "number_lexer.hpp"
#include "number_token.hpp"
#include "owned_block.hpp"
#include "result.hpp"
#include "storage.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Own one validated numeric spelling without numeric conversion.
   * @par Requirements
   * P029; J5-034; J5-038; J5-040.
   * @note Original bytes preserve signs, negative zero and exact numeric text.
   * The storage provider state must outlive the owned allocation.
   * No terminating NUL is stored.
   * Numeric conversion and exact-value operations are separate.
   * A moved-from object has empty spelling and retains its kind metadata.
   * Empty spelling does not represent a numeric value.
   */
  class owned_number
  {
    private:
      owned_block m_storage;
      number_kind m_kind;

      /**
       * @brief Adopt validated numeric text and its spelling family.
       * @par Requirements
       * P029; J5-034; J5-040.
       * @note Storage ownership transfers without another allocation.
       */
      owned_number(owned_block&& storage, number_kind kind) noexcept : m_storage(std::move(storage)), m_kind(kind) {}

    public:
      /**
       * @brief Transfer owned spelling without copying its bytes.
       * @par Requirements
       * P029; J5-040.
       * @note The source becomes empty; views into transferred storage
       * remain valid while the destination owns that storage.
       */
      owned_number(owned_number&& other) noexcept = default;

      /**
       * @brief Release numeric text through its original storage provider.
       * @par Requirements
       * P029; J5-040.
       * @note Performs no allocation and does not throw.
       */
      ~owned_number() noexcept = default;

      // Independent copies require an explicit fallible storage operation.
      owned_number(const owned_number&) = delete;
      owned_number& operator=(const owned_number&) = delete;
      owned_number& operator=(owned_number&&) = delete;

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
      static result<owned_number> copy_spelling(input_view text, syntax_mode mode, const storage_callbacks& storage,
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
      input_view spelling() const noexcept
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
      number_kind kind() const noexcept
      {
        return m_kind;
      }
  };
} // namespace adaptive_json5

#endif /* OWNED_NUMBER_HPP_ */
