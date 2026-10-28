#ifndef DECODED_STRING_HPP_
#define DECODED_STRING_HPP_

#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Declare the builder that transfers completed string storage.
   * @par Requirements
   * P013; J5-040.
   */
  class decoded_string_builder;

  /**
   * @brief Own an immutable sequence of decoded UTF-16 units.
   * @par Requirements
   * P013; J5-018; J5-029; J5-040.
   * @note Embedded NUL and lone surrogate units are preserved.
   * No terminating NUL is stored or required.
   * The storage provider state must outlive this object.
   */
  class [[nodiscard]] decoded_string
  {
    private:
      // Only the builder may adopt completed storage without copying.
      friend class decoded_string_builder;

      owned_block m_storage{};
      std::size_t m_size = 0;

      /**
       * @brief Adopt storage containing copied unit representations.
       * @par Requirements
       * P013; J5-040.
       */
      decoded_string(owned_block&& storage, std::size_t size) noexcept;

    public:
      /**
       * @brief Construct an empty string without allocating.
       * @par Requirements
       * P013; J5-029.
       */
      decoded_string() noexcept = default;

      /**
       * @brief Transfer ownership and leave the source empty.
       * @par Requirements
       * P013; J5-040.
       */
      decoded_string(decoded_string&& other) noexcept;

      /**
       * @brief Release owned storage through its original provider.
       * @par Requirements
       * P013; J5-040.
       */
      ~decoded_string() noexcept = default;

      /**
       * @brief Copying is not allowed.
       * @par Requirements
       * P021; J5-040.
       * @note Two strings would own the same units; use copy_units() for an independent,
       * fallible copy.
       */
      decoded_string(const decoded_string&) = delete;

      /**
       * @brief Copy assignment is not allowed.
       * @par Requirements
       * P021; J5-040.
       * @note Same reason as the copy constructor.
       */
      decoded_string& operator=(const decoded_string&) = delete;

      /**
       * @brief Move assignment is not allowed.
       * @par Requirements
       * P021; J5-040.
       * @note Replacing a live string would release its units first; construct a new
       * string instead.
       */
      decoded_string& operator=(decoded_string&&) = delete;

      /**
       * @brief Copy decoded units into independent caller-provided storage.
       * @par Requirements
       * P013; J5-018; J5-029; J5-040.
       * @param units Readable array of count units; may be nullptr when count is zero.
       * @param count Number of UTF-16 units, not bytes or Unicode scalars.
       * @return Owned string or an explicit access, size or allocation error.
       * @note Empty input succeeds without requiring a storage provider.
       * Nonempty input must remain readable for this call only.
       */
      [[nodiscard]] static result<decoded_string> copy_units(const storage_callbacks& storage, const char16_t* units,
                                                             std::size_t count) noexcept;

      /**
       * @brief Return the number of stored UTF-16 units.
       * @par Requirements
       * P013; J5-029.
       */
      [[nodiscard]] std::size_t size() const noexcept;

      /**
       * @brief Read one decoded unit without changing the string.
       * @par Requirements
       * P013; J5-018; J5-029.
       * @return Exact unit, or wrong_type_or_access for an invalid index.
       * @note Surrogate units are returned unchanged.
       */
      [[nodiscard]] result<char16_t> unit_at(std::size_t index) const noexcept;
  };
} // namespace adaptive_json5

#endif // DECODED_STRING_HPP_
