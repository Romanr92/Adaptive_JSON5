#ifndef DECODED_STRING_BUILDER_HPP_
#define DECODED_STRING_BUILDER_HPP_

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/unicode/utf16.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Accumulate decoded UTF-16 units in caller-provided storage.
   * @par Requirements
   * P013; J5-018; J5-029; J5-040.
   * @note The provider state must outlive the builder and its allocations.
   * Failed appends preserve existing content and length.
   * max_units bounds capacity in units; total-memory accounting is separate.
   */
  class decoded_string_builder
  {
    private:
      storage_callbacks m_provider;
      owned_block m_storage{};
      std::size_t m_size{};
      std::size_t m_capacity{};
      std::size_t m_max_units;

      /**
       * @brief Ensure room for the requested total unit count.
       * @par Requirements
       * P013; J5-040.
       * @return Success, resource_limit, or allocation_failure.
       * @note Growth temporarily holds both old and new allocations.
       */
      status_result reserve(std::size_t required) noexcept;

      /**
       * @brief Append units from an independent readable array.
       * @par Requirements
       * P013; J5-018; J5-029.
       * @note Private callers provide a valid nonempty local array.
       * Storage is reserved for the entire append before writing any units.
       */
      status_result append_units(const char16_t* units, std::size_t count) noexcept;

    public:
      /**
       * @brief Construct an empty builder with an explicit unit limit.
       * @par Requirements
       * P013; J5-029; J5-040.
       * @note Construction allocates nothing.
       * A zero limit permits only empty content.
       */
      decoded_string_builder(const storage_callbacks& provider, std::size_t max_units) noexcept;

      // Keep one stable owner while vuilding the string.
      decoded_string_builder(const decoded_string_builder&) = delete;
      decoded_string_builder& operator=(const decoded_string_builder&) = delete;
      decoded_string_builder(decoded_string_builder&&) = delete;
      decoded_string_builder& operator=(decoded_string_builder&&) = delete;

      /**
       * @brief Release any accumulated storage.
       * @par Requirements
       * P013; J5-040.
       */
      ~decoded_string_builder() noexcept = default;

      /**
       * @brief Append one exact UTF-16 unit.
       * @par Requirements
       * P013; J5-018; J5-029.
       * @note NUL and lone surrogate units are preserved.
       */
      status_result append_unit(char16_t unit) noexcept;

      /**
       * @brief Append the UTF-16 representation of one Unicode scalar.
       * @par Requirements
       * P013; J5-018.
       * @note An invalid scalar returns an encoding error.
       * A surrogate pair is appended completely or not at all.
       */
      status_result append_scalar(char32_t scalar) noexcept;

      /**
       * @brief Transfer accumulated storage into an immutable string.
       * @par Requirements
       * P013; J5-018; J5-029; J5-040.
       * @return Completed string owning the existing allocation.
       * @note Performs no allocation or content copy and cannot fail.
       * Unused capacity remains owned by the returned string.
       * The builder becomes empty and can be reused with its original
       * provider and unit limit. Repeated finish returns an empty string.
       */
      decoded_string finish() noexcept;

      /**
       * @brief Return the number of accumulated UTF-16 units.
       * @par Requirements
       * P013; J5-029.
       */
      std::size_t size() const noexcept;
  };
} // namespace adaptive_json5

#endif // DECODED_STRING_BUILDER_HPP_
