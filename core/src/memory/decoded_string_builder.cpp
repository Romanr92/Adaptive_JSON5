#include "memory/decoded_string_builder.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "common/checked_size.hpp"
#include "unicode/utf16.hpp"

#include <cstddef>
#include <cstring>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Append units from an independent readable array.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @note Private callers provide a valid nonempty local array.
   * The entire append must fit the reservation before any unit is written.
   */
  status_result decoded_string_builder::append_units(const char16_t* units, std::size_t count) noexcept
  {
    const auto new_size = checked_add(m_size, count);
    if (!new_size.has_value())
    {
      return status_result::failure(*new_size.error_if());
    }

    // Growing here would mean the lexer's counting pass and decoding pass disagree.
    if (*new_size.value_if() > m_capacity)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    auto* bytes = static_cast<unsigned char*>(m_storage.data());

    // The capacity check proved that the old offset and complete range fit.
    std::memcpy(bytes + (m_size * sizeof(char16_t)), units, (count * sizeof(char16_t)));
    m_size = *new_size.value_if();

    return status_result::success();
  }

  /**
   * @brief Construct an empty builder with an explicit unit limit.
   * @par Requirements
   * P013; J5-029; J5-040.
   * @note Construction allocates nothing.
   * A zero limit permits only empty content.
   */
  decoded_string_builder::decoded_string_builder(const storage_callbacks& provider, std::size_t max_units) noexcept
      : m_provider(provider), m_max_units(max_units)
  {
  }

  /**
   * @brief Reserve storage for exactly the given number of units.
   * @par Requirements
   * P013; J5-029; J5-040; J5-046.
   * @param units Exact decoded size counted by the caller's first pass.
   * @return Success, resource_limit above the unit limit, allocation_failure, or
   * wrong_type_or_access when content or a reservation already exists.
   * @note Makes at most one allocation; zero units allocate nothing.
   */
  status_result decoded_string_builder::reserve_exact(std::size_t units) noexcept
  {
    // One reservation per string: a second one would mean the lexer's passes are out of step.
    if ((m_size != std::size_t{}) || (m_capacity != std::size_t{}))
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    if (units > m_max_units)
    {
      return status_result::failure(error::without_location(error_code::resource_limit));
    }

    // An empty string owns no storage, so it never needs the provider.
    if (units == std::size_t{})
    {
      return status_result::success();
    }

    auto allocation = allocate_array(m_provider, units, sizeof(char16_t), alignof(char16_t));

    if (!allocation.has_value())
    {
      return status_result::failure(*allocation.error_if());
    }

    m_storage = std::move(*allocation.value_if());
    m_capacity = units;

    return status_result::success();
  }

  /**
   * @brief Append one exact UTF-16 unit.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @note NUL and lone surrogate units are preserved.
   */
  status_result decoded_string_builder::append_unit(char16_t unit) noexcept
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
  status_result decoded_string_builder::append_scalar(char32_t scalar) noexcept
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
   * The returned string owns exactly the reserved storage.
   * The builder becomes empty and can be reused with its original
   * provider and unit limit. Repeated finish returns an empty string.
   */
  decoded_string decoded_string_builder::finish() noexcept
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
  std::size_t decoded_string_builder::size() const noexcept
  {
    return m_size;
  }
} // namespace adaptive_json5
