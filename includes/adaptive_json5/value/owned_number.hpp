#ifndef OWNED_NUMBER_HPP_
#define OWNED_NUMBER_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/number_kind.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    struct number_access;
  } // namespace detail

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
  class [[nodiscard]] owned_number
  {
    private:
      // Internal parser access for spellings it already validated; not a supported public API.
      friend struct detail::number_access;

      owned_block m_storage;
      number_kind m_kind;

      /**
       * @brief Adopt validated numeric text and its spelling family.
       * @par Requirements
       * P029; J5-034; J5-040.
       * @note Storage ownership transfers without another allocation.
       */
      owned_number(owned_block&& storage, number_kind kind) noexcept;

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
      [[nodiscard]] static result<owned_number> copy_spelling(input_view text, syntax_mode mode,
                                                              const storage_callbacks& storage,
                                                              std::size_t max_bytes) noexcept;

      /**
       * @brief Borrow the exact original numeric spelling.
       * @par Requirements
       * P029; J5-034; J5-038; J5-040.
       * @return Immutable bytes without a terminating NUL.
       * @note Empty after ownership has been moved away.
       * The view becomes invalid when its owning allocation is released.
       */
      [[nodiscard]] input_view spelling() const noexcept;

      /**
       * @brief Return the retained numeric spelling family.
       * @par Requirements
       * P029; J5-034; J5-038.
       * @note Finite decimal spellings remain decimal regardless of
       * whether a later machine-number conversion would overflow.
       * A moved-from object's kind is metadata only.
       */
      [[nodiscard]] number_kind kind() const noexcept;
  };
} // namespace adaptive_json5

#endif /* OWNED_NUMBER_HPP_ */
