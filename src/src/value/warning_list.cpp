#include "adaptive_json5/value/warning_list.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    // Entries are copied as raw bytes, which is only valid for trivially copyable items.
    static_assert(std::is_trivially_copyable_v<string_warning>, "warning entries are copied bytewise");

    constexpr std::size_t c_growth_factor = 2;

    /**
     * @brief Read one stored entry from raw owned storage.
     * @par Requirements
     * P039; J5-046.
     * @param storage Block holding at least index + 1 entries.
     * @param index Initialized entry position.
     * @return Copy of the stored entry.
     * @note Byte copying avoids constructing objects inside raw storage.
     */
    [[nodiscard]] string_warning read_entry(const owned_block& storage, std::size_t index) noexcept
    {
      string_warning entry{};
      const auto* bytes = static_cast<const unsigned char*>(storage.data());

      std::memcpy(&entry, bytes + (index * sizeof(string_warning)), sizeof(entry));

      return entry;
    }

    /**
     * @brief Store one entry into raw owned storage.
     * @par Requirements
     * P039; J5-046.
     * @param storage Block with room for at least index + 1 entries.
     * @param index Entry position to write.
     * @param entry Value to store.
     */
    void write_entry(owned_block& storage, std::size_t index, const string_warning& entry) noexcept
    {
      auto* bytes = static_cast<unsigned char*>(storage.data());

      std::memcpy(bytes + (index * sizeof(string_warning)), &entry, sizeof(entry));
    }
  } // namespace

  /**
   * @brief Borrow the provider without allocating.
   * @par Requirements
   * P039; J5-046.
   */
  warning_list::warning_list(const storage_callbacks& provider) noexcept : m_provider(provider) {}

  /**
   * @brief Take over the storage and counts, then empty the source.
   * @par Requirements
   * P039; J5-040.
   */
  warning_list::warning_list(warning_list&& other) noexcept
      : m_provider(other.m_provider), m_storage(std::move(other.m_storage)), m_size(other.m_size),
        m_capacity(other.m_capacity)
  {
    // The moved-from list must not report entries whose storage it no longer owns.
    other.m_size = 0;
    other.m_capacity = 0;
  }

  /**
   * @brief Grow into a replacement block and publish it only after copying succeeds.
   * @par Requirements
   * P039; J5-003; J5-046.
   */
  status_result warning_list::reserve(std::size_t additional) noexcept
  {
    const auto required = checked_add(m_size, additional);

    if (!required.has_value())
    {
      return status_result::failure(*required.error_if());
    }

    if (*required.value_if() <= m_capacity)
    {
      return status_result::success();
    }

    std::size_t capacity = *required.value_if();

    // Doubling keeps total copying linear; if doubling overflows, the exact requirement still suffices.
    const auto doubled = checked_multiply(m_capacity, c_growth_factor);

    if (doubled.has_value() && (*doubled.value_if() > capacity))
    {
      capacity = *doubled.value_if();
    }

    auto allocation = allocate_array(m_provider, capacity, sizeof(string_warning), alignof(string_warning));

    if (!allocation.has_value())
    {
      return status_result::failure(*allocation.error_if());
    }

    owned_block& replacement = *allocation.value_if();

    for (std::size_t index{}; index < m_size; ++index)
    {
      write_entry(replacement, index, read_entry(m_storage, index));
    }

    // Publishing last means any earlier failure left the existing entries untouched.
    m_storage = std::move(replacement);
    m_capacity = capacity;

    return status_result::success();
  }

  /**
   * @brief Write into reserved space without allocating.
   * @par Requirements
   * P039; J5-003; J5-017.
   */
  status_result warning_list::append_reserved(const string_warning& warning) noexcept
  {
    if (m_size >= m_capacity)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    write_entry(m_storage, m_size, warning);
    ++m_size;

    return status_result::success();
  }

  /**
   * @brief Report the retained entry count.
   * @par Requirements
   * P039; J5-017.
   */
  std::size_t warning_list::size() const noexcept
  {
    return m_size;
  }

  /**
   * @brief Return a copy of one checked entry.
   * @par Requirements
   * P039; J5-003; J5-017; J5-044.
   */
  result<string_warning> warning_list::at(std::size_t index) const noexcept
  {
    if (index >= m_size)
    {
      return result<string_warning>::failure(error::without_location(error_code::wrong_type_or_access));
    }

    return result<string_warning>::success(read_entry(m_storage, index));
  }
} // namespace adaptive_json5
