#include "adaptive_json5/detail/value/big_unsigned.hpp"

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace adaptive_json5::detail
{
  namespace
  {
    using limb_type = std::uint_least16_t;
    using wide_type = std::uint_least32_t;

    constexpr wide_type c_limb_radix = 65536;
    constexpr unsigned int c_decimal_radix = 10;
    constexpr unsigned int c_hexadecimal_radix = 16;
    constexpr std::size_t c_limb_passes = 2;
    constexpr std::size_t c_tail_work_units = 1;

    constexpr std::size_t c_additional_limb = 1;

    /**
     * @brief Read one initialized limb from owned byte storage.
     * @par Requirements
     * P038; J5-037; J5-046.
     * @param storage Block containing the initialized limb.
     * @param index Existing limb index.
     * @return Stored limb value.
     * @note Caller guarantees an in-bounds initialized limb.
     * Byte copying avoids constructing typed objects inside raw storage.
     */
    [[nodiscard]] limb_type read_limb(const owned_block& storage, std::size_t index) noexcept
    {
      limb_type value{};
      const auto* bytes = static_cast<const unsigned char*>(storage.data());

      std::memcpy(&value, bytes + (index * sizeof(limb_type)), sizeof(value));

      return value;
    }

    /**
     * @brief Store one limb in an allocated byte region.
     * @par Requirements
     * P038; J5-037; J5-046.
     * @param storage Block containing the destination slot.
     * @param index Allocated limb-slot index.
     * @param value Limb value below the limb radix.
     * @note Caller guarantees an in-bounds slot.
     */
    void write_limb(owned_block& storage, std::size_t index, limb_type value) noexcept
    {
      auto* bytes = static_cast<unsigned char*>(storage.data());

      std::memcpy(bytes + (index * sizeof(limb_type)), &value, sizeof(value));
    }
  } // namespace

  /**
   * @brief Construct an allocation-free exact zero.
   * @par Requirements
   * P038; J5-046.
   * @param provider Borrowed fallible storage provider.
   */
  big_unsigned::big_unsigned(const storage_callbacks& provider) noexcept : m_provider(provider) {}

  /**
   * @brief Report whether no nonzero limbs are stored.
   * @par Requirements
   * P038; J5-037.
   */
  bool big_unsigned::is_zero() const noexcept
  {
    return m_size == 0;
  }

  /**
   * @brief Append a numeric digit using bounded exact limb arithmetic.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @param digit Numeric digit value below radix.
   * @param radix Decimal or hexadecimal spelling base.
   * @param work Shared work budget.
   * @return Success or an explicit failure preserving the original value.
   * @note Allocation and budget checks precede mutation.
   */
  status_result big_unsigned::append_digit(unsigned int digit, unsigned int radix, work_budget& work) noexcept
  {
    if (((radix != c_decimal_radix) && (radix != c_hexadecimal_radix)) || (digit >= radix))
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    const auto passes = checked_multiply(m_size, c_limb_passes);

    if (!passes.has_value())
    {
      return status_result::failure(*passes.error_if());
    }

    const auto units = checked_add(*passes.value_if(), c_tail_work_units);

    if (!units.has_value())
    {
      return status_result::failure(*units.error_if());
    }

    const auto charged = work.charge(*units.value_if());

    if (!charged.has_value())
    {
      return status_result::failure(*charged.error_if());
    }

    // Determine whether growth is needed without modifying the current integer.
    wide_type carry = digit;

    for (std::size_t index{}; index < m_size; ++index)
    {
      const auto expanded = (static_cast<wide_type>(read_limb(m_storage, index)) * radix) + carry;

      carry = expanded / c_limb_radix;
    }

    const bool grows = (carry != 0);
    owned_block replacement{};

    if (grows)
    {
      const auto count = checked_add(m_size, c_additional_limb);

      if (!count.has_value())
      {
        return status_result::failure(*count.error_if());
      }

      auto allocation = allocate_array(m_provider, *count.value_if(), sizeof(limb_type), alignof(limb_type));

      if (!allocation.has_value())
      {
        return status_result::failure(*allocation.error_if());
      }

      replacement = std::move(*allocation.value_if());
    }

    // No fallible operations remain after choosing the destination storage.
    owned_block& destination = grows ? replacement : m_storage;
    carry = digit;

    for (std::size_t index{}; index < m_size; ++index)
    {
      const auto expanded = (static_cast<wide_type>(read_limb(m_storage, index)) * radix) + carry;

      write_limb(destination, index, static_cast<limb_type>(expanded % c_limb_radix));

      carry = expanded / c_limb_radix;
    }

    if (grows)
    {
      write_limb(destination, m_size, static_cast<limb_type>(carry));
      m_storage = std::move(replacement);
      ++m_size;
    }

    return status_result::success();
  }
} // namespace adaptive_json5::detail
