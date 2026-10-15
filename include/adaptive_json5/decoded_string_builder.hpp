#ifndef DECODED_STRING_BUILDER_HPP_
#define DECODED_STRING_BUILDER_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/storage.hpp"
#include "checked_size.hpp"
#include "decoded_string.hpp"
#include "owned_block.hpp"
#include "utf16.hpp"

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
      status_result reserve(std::size_t required) noexcept
      {
        if (required > m_max_units)
        {
          return status_result::failure(error::without_location(error_code::resource_limit));
        }

        if (required <= m_capacity)
        {
          return status_result::success();
        }

        std::size_t new_capacity = required;

        // Geometric growth avoids reallocation for every appended character.
        const auto doubled = checked_add(m_capacity, m_capacity);
        if (doubled.has_value())
        {
          const std::size_t bounded_capacity = (*doubled.value_if() > m_max_units) ? m_max_units : *doubled.value_if();

          if (bounded_capacity > new_capacity)
          {
            new_capacity = bounded_capacity;
          }
        }

        auto allocation = allocate_array(m_provider, new_capacity, sizeof(char16_t), alignof(char16_t));

        if (!allocation.has_value())
        {
          return status_result::failure(*allocation.error_if());
        }

        // Empty storage may have a null pointer, so omit the empty copy.
        if (m_size != std::size_t{})
        {
          std::memcpy(allocation.value_if()->data(), m_storage.data(), m_size * sizeof(char16_t));
        }

        // Replace the old woner only after allocation and coy succeeded.
        m_storage = std::move(*allocation.value_if());
        m_capacity = new_capacity;

        return status_result::success();
      }

      /**
       * @brief Append units from an independent readable array.
       * @par Requirements
       * P013; J5-018; J5-029.
       * @note Private callers provide a valid nonempty local array.
       * Storage is reserved for the entire append before writing any units.
       */
      status_result append_units(const char16_t* units, std::size_t count) noexcept
      {
        const auto new_size = checked_add(m_size, count);
        if (!new_size.has_value())
        {
          return status_result::failure(*new_size.error_if());
        }

        const auto reserved = reserve(*new_size.value_if());
        if (!reserved.has_value())
        {
          return status_result::failure(*reserved.error_if());
        }

        auto* bytes = static_cast<unsigned char*>(m_storage.data());

        // Reserve proved that the old offset and complete range fit.
        std::memcpy(bytes + (m_size * sizeof(char16_t)), units, (count * sizeof(char16_t)));
        m_size = *new_size.value_if();

        return status_result::success();
      }

    public:
      /**
       * @brief Construct an empty builder with an explicit unit limit.
       * @par Requirements
       * P013; J5-029; J5-040.
       * @note Construction allocates nothing.
       * A zero limit permits only empty content.
       */
      decoded_string_builder(const storage_callbacks& provider, std::size_t max_units) noexcept
          : m_provider(provider), m_max_units(max_units)
      {
      }

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
      status_result append_unit(char16_t unit) noexcept
      {
        return append_units(&unit, encoded_utf16::c_single_unit_count);
      }

      /**
       * @brief Append the UTF-16 representation of one Unicode scalar.
       * @par Requirements
       * P013; J5-018.
       * @note An invalid scalar returns an encoding error.
       * A surrogate pair is appended completely or not at all.
       */
      status_result append_scalar(char32_t scalar) noexcept
      {
        const auto encoded = encode_utf16(scalar);
        if (!encoded.has_value())
        {
          return status_result::failure(*encoded.error_if());
        }

        return append_units(encoded.value_if()->units, encoded.value_if()->size);
      }

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
      decoded_string finish() noexcept
      {
        decoded_string output{std::move(m_storage), m_size};

        // The builder no longwe owns storage or has capacity to append into.
        m_size = std::size_t{};
        m_capacity = std::size_t{};

        return output;
      }

      /**
       * @brief Return the number of accumulated UTF-16 units.
       * @par Requirements
       * P013; J5-029.
       */
      std::size_t size() const noexcept
      {
        return m_size;
      }
  };
} // namespace adaptive_json5

#endif // DECODED_STRING_BUILDER_HPP_
