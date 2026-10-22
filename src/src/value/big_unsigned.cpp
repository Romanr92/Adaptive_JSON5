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
    constexpr unsigned int c_limb_bits = 16;
    constexpr std::size_t c_shift_setup_work_units = 1;

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

  /**
   * @brief Compare canonical magnitudes from their most significant limbs.
   * @par Requirements
   * P038; J5-003; J5-037; J5-048.
   * @param other Borrowed right-hand magnitude.
   * @param work Shared budget charged before comparison.
   * @return Exact ordering or an explicit resource failure.
   * @note Performs no allocation or mutation.
   */
  result<magnitude_order> big_unsigned::compare(const big_unsigned& other, work_budget& work) const noexcept
  {
    using comparison_result = result<magnitude_order>;

    constexpr std::size_t c_setup_work_units = 1;

    const std::size_t traversal_units = (m_size == other.m_size) ? m_size : 0;

    const auto units = checked_add(traversal_units, c_setup_work_units);

    if (!units.has_value())
    {
      return comparison_result::failure(*units.error_if());
    }

    const auto charged = work.charge(*units.value_if());

    if (!charged.has_value())
    {
      return comparison_result::failure(*charged.error_if());
    }

    if (m_size < other.m_size)
    {
      return comparison_result::success(magnitude_order::less);
    }

    if (m_size > other.m_size)
    {
      return comparison_result::success(magnitude_order::greater);
    }

    std::size_t index = m_size;

    while (index != 0)
    {
      // Decrement before indexing to avoid unsigned wrap after the final limb.
      --index;

      const auto left = read_limb(m_storage, index);
      const auto right = read_limb(other.m_storage, index);

      if (left < right)
      {
        return comparison_result::success(magnitude_order::less);
      }

      if (left > right)
      {
        return comparison_result::success(magnitude_order::greater);
      }
    }

    return comparison_result::success(magnitude_order::equal);
  }

  /**
   * @brief Shift through replacement storage before publishing the new magnitude.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @param bit_count Exact power-of-two scaling exponent.
   * @param work Shared budget charged before limb processing.
   * @return Success or an explicit failure preserving the original value.
   * @note Allocation and size checks precede all writes to replacement storage.
   */
  status_result big_unsigned::shift_left(std::size_t bit_count, work_budget& work) noexcept
  {
    if ((bit_count == 0) || (m_size == 0))
    {
      return status_result::success();
    }

    const std::size_t whole_limbs = bit_count / c_limb_bits;

    const auto partial_bits = static_cast<unsigned int>(bit_count % c_limb_bits);

    const auto base_count = checked_add(m_size, whole_limbs);

    if (!base_count.has_value())
    {
      return status_result::failure(*base_count.error_if());
    }

    std::size_t destination_count = *base_count.value_if();

    if (partial_bits != 0)
    {
      const auto extended = checked_add(destination_count, c_additional_limb);

      if (!extended.has_value())
      {
        return status_result::failure(*extended.error_if());
      }

      destination_count = *extended.value_if();
    }

    const auto traversal_units = checked_add(m_size, destination_count);

    if (!traversal_units.has_value())
    {
      return status_result::failure(*traversal_units.error_if());
    }

    const auto units = checked_add(*traversal_units.value_if(), c_shift_setup_work_units);

    if (!units.has_value())
    {
      return status_result::failure(*units.error_if());
    }

    const auto charged = work.charge(*units.value_if());

    if (!charged.has_value())
    {
      return status_result::failure(*charged.error_if());
    }

    auto allocation = allocate_array(m_provider, destination_count, sizeof(limb_type), alignof(limb_type));

    if (!allocation.has_value())
    {
      return status_result::failure(*allocation.error_if());
    }

    auto& destination = *allocation.value_if();

    // Whole-limb shifting introduces zero limbs at the last significant end.
    for (std::size_t index{}; index < whole_limbs; ++index)
    {
      write_limb(destination, index, limb_type{});
    }

    wide_type carry{};

    for (std::size_t index{}; index < m_size; ++index)
    {
      // The partial shift is below 16 bits, so the expanded value fits wide_type.
      const auto expanded = (static_cast<wide_type>(read_limb(m_storage, index)) << partial_bits) | carry;

      write_limb(destination, whole_limbs + index, static_cast<limb_type>(expanded % c_limb_radix));

      carry = expanded / c_limb_radix;
    }

    std::size_t significant_count = *base_count.value_if();

    if (partial_bits != 0)
    {
      write_limb(destination, significant_count, static_cast<limb_type>(carry));

      if (carry != 0)
      {
        ++significant_count;
      }
    }

    // Publish only after all the fallible operations and result construction finish.
    m_storage = std::move(destination);
    m_size = significant_count;

    return status_result::success();
  }

  /**
   * @brief Subtract with borrow after validating ordering and reserving work.
   * @par Requirements
   * P038; J5-003; J5-037; J5-048.
   * @param other Borrowed subtrahend, including this object itself.
   * @param work Shared work budget.
   * @return Success or an explicit failure before mutation.
   * @note Retains allocated storage and restores canonical significant-limb count.
   */
  status_result big_unsigned::subtract(const big_unsigned& other, work_budget& work) noexcept
  {
    constexpr std::size_t c_subtraction_passes = 2;
    constexpr std::size_t c_subtraction_setup_units = 1;
    constexpr wide_type c_borrow_units = 1;

    const auto ordering = compare(other, work);

    if (!ordering.has_value())
    {
      return status_result::failure(*ordering.error_if());
    }

    if (*ordering.value_if() == magnitude_order::less)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    if (other.m_size == 0)
    {
      return status_result::success();
    }

    const auto passes = checked_multiply(m_size, c_subtraction_passes);

    if (!passes.has_value())
    {
      return status_result::failure(*passes.error_if());
    }

    const auto units = checked_add(*passes.value_if(), c_subtraction_setup_units);

    if (!units.has_value())
    {
      return status_result::failure(*units.error_if());
    }

    const auto charged = work.charge(*units.value_if());

    if (!charged.has_value())
    {
      return status_result::failure(*charged.error_if());
    }

    wide_type borrow{};

    for (std::size_t index{}; index < m_size; ++index)
    {
      const auto left = static_cast<wide_type>(read_limb(m_storage, index));

      const wide_type right =
        (index < other.m_size) ? static_cast<wide_type>(read_limb(other.m_storage, index)) : wide_type{};

      const wide_type subtrahend = right + borrow;
      wide_type difference{};

      if (left < subtrahend)
      {
        // Borrow one base-65536 unit from the next limb.
        difference = (left + c_limb_radix) - subtrahend;
        borrow = c_borrow_units;
      }
      else
      {
        difference = left - subtrahend;
        borrow = 0;
      }

      write_limb(m_storage, index, static_cast<limb_type>(difference));
    }

    // Keep comparison valid by excluding zer limbs at the significant end.
    while (m_size != 0)
    {
      const std::size_t highest_index = m_size - c_additional_limb;

      if (read_limb(m_storage, highest_index) != 0)
      {
        break;
      }

      --m_size;
    }

    return status_result::success();
  }

  /**
   * @brief Build an independent copy before replacing the destination magnitude.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @param other Borrowed source magnitude.
   * @param work Shared work budget.
   * @return Success or an explicit failure before destination mutation.
   * @note The destination retains its original storage provider.
   */
  status_result big_unsigned::assign(const big_unsigned& other, work_budget& work) noexcept
  {
    constexpr std::size_t c_copy_setup_working_units = 1;

    if (this == &other)
    {
      return status_result::success();
    }

    const auto units = checked_add(other.m_size, c_copy_setup_working_units);

    if (!units.has_value())
    {
      return status_result::failure(*units.error_if());
    }

    const auto charged = work.charge(*units.value_if());

    if (!charged.has_value())
    {
      return status_result::failure(*charged.error_if());
    }

    if (other.m_size == 0)
    {
      m_size = 0;
      return status_result::success();
    }

    // Keep the original value intact until replacement storage is ready.
    auto allocation = allocate_array(m_provider, other.m_size, sizeof(limb_type), alignof(limb_type));

    if (!allocation.has_value())
    {
      return status_result::failure(*allocation.error_if());
    }

    auto& destination = *allocation.value_if();

    for (std::size_t index{}; index < other.m_size; ++index)
    {
      write_limb(destination, index, read_limb(other.m_storage, index));
    }

    // No fallible operations remain, so publishing cannot leave a partial copy.
    m_storage = std::move(destination);
    m_size = other.m_size;

    return status_result::success();
  }
} // namespace adaptive_json5::detail
