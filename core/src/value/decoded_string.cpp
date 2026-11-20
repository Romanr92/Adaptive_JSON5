#include "adaptive_json5/value/decoded_string.hpp"

#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "common/checked_size.hpp"
#include "memory/decoded_string_builder.hpp"
#include "memory/utf16_storage.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace adaptive_json5
{
  /**
   * @brief Adopt storage containing copied unit representations.
   * @par Requirements
   * P013; J5-040.
   * @par ID SWU-VALUE-017
   */
  decoded_string::decoded_string(owned_block&& storage, std::size_t size) noexcept
      : m_storage(std::move(storage)), m_size(size)
  {
  }

  /**
   * @brief Transfer ownership and leave the source empty.
   * @par Requirements
   * P013; J5-040.
   * @par ID SWU-VALUE-018
   */
  decoded_string::decoded_string(decoded_string&& other) noexcept
      : m_storage(std::move(other.m_storage)), m_size(other.m_size)
  {
    // The moved-from string must not advertise units it no longer owns.
    other.m_size = 0;
  }

  /**
   * @brief Copy decoded units into independent caller-provided storage.
   * @par Requirements
   * P013; J5-018; J5-029; J5-040.
   * @param units Readable array of count units; may be nullptr when count is zero.
   * @param count Number of UTF-16 units, not bytes or Unicode scalars.
   * @return Owned string or an explicit access, size or allocation error.
   * @note Empty input succeeds without requiring a storage provider.
   * Nonempty input must remain readable for this call only.
   * @par ID SWU-VALUE-019
   */
  result<decoded_string> decoded_string::copy_units(const storage_callbacks& storage, const std::uint16_t* units,
                                                    std::size_t count) noexcept
  {
    if (count == 0)
    {
      return result<decoded_string>::success(decoded_string{});
    }

    if (units == nullptr)
    {
      return result<decoded_string>::failure(error::access_failure(access_error_reason::invalid_argument));
    }

    // The builder allocates exactly count units and stores them in the layout unit_at() reads.
    decoded_string_builder builder{storage, count};
    const auto reserved = builder.reserve_exact(count);
    if (!reserved.has_value())
    {
      return result<decoded_string>::failure(*reserved.error_if());
    }

    const std::uint16_t* next = units;
    std::size_t remaining = count;
    while (remaining != std::size_t{})
    {
      const auto appended = builder.append_unit(*next);
      if (!appended.has_value())
      {
        return result<decoded_string>::failure(*appended.error_if());
      }

      ++next;
      --remaining;
    }

    return result<decoded_string>::success(builder.finish());
  }

  /**
   * @brief Return the number of stored UTF-16 units.
   * @par Requirements
   * P013; J5-029.
   * @par ID SWU-VALUE-020
   */
  std::size_t decoded_string::size() const noexcept
  {
    return m_size;
  }

  /**
   * @brief Read one decoded unit without changing the string.
   * @par Requirements
   * P013; J5-018; J5-029.
   * @return Exact unit, or wrong_type_or_access (index_out_of_range) for an invalid index.
   * @note Surrogate units are returned unchanged.
   * @par ID SWU-VALUE-021
   */
  result<std::uint16_t> decoded_string::unit_at(std::size_t index) const noexcept
  {
    if (index >= m_size)
    {
      return result<std::uint16_t>::failure(error::access_failure(access_error_reason::index_out_of_range));
    }

    // The index check bounds the read to a stored UTF-16 unit. Read its bytes
    // without relying on the storage to contain a typed UTF-16 object.
    const auto* bytes = static_cast<const std::byte*>(m_storage.data());
    // Successful allocation already proved every in-range unit offset fits.
    const auto offset = checked_multiply(index, detail::c_utf16_unit_bytes, resource_limit_reason::string);
    if (!offset.has_value())
    {
      return result<std::uint16_t>::failure(error::internal_fault());
    }

    return result<std::uint16_t>::success(detail::load_utf16_unit(bytes + *offset.value_if()));
  }
} // namespace adaptive_json5
