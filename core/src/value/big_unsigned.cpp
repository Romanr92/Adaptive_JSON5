#include "value/big_unsigned.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "common/checked_size.hpp"

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
    constexpr unsigned int c_decimal_radix_2 = 100;
    constexpr unsigned int c_decimal_radix_3 = 1000;
    constexpr unsigned int c_decimal_radix_4 = 10000;
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
   * @brief Upper bound on the bits of an integer below 10^digits.
   * @par Requirements
   * P038; J5-037; J5-046.
   * @param digits Number of decimal digits, or the exponent of a power of ten.
   * @return ceil(digits * 10 / 3) bits, or resource_limit on overflow.
   */
  result<std::size_t> decimal_digit_bits(std::size_t digits) noexcept
  {
    constexpr std::size_t c_bits_numerator = 10;
    constexpr std::size_t c_bits_denominator = 3;

    const auto scaled = checked_multiply(digits, c_bits_numerator);

    if (!scaled.has_value())
    {
      return result<std::size_t>::failure(*scaled.error_if());
    }

    // Adding denominator - 1 before dividing rounds up.
    const auto rounded = checked_add(*scaled.value_if(), c_bits_denominator - 1);

    if (!rounded.has_value())
    {
      return result<std::size_t>::failure(*rounded.error_if());
    }

    return result<std::size_t>::success(*rounded.value_if() / c_bits_denominator);
  }

  /**
   * @brief Convert a bit bound to the limbs to reserve, with one limb of margin.
   * @par Requirements
   * P038; J5-037; J5-046.
   * @param bits Upper bound on significant bits.
   * @return ceil(bits / 16) + 1 limbs, or resource_limit on overflow.
   */
  result<std::size_t> limbs_for_bits(std::size_t bits) noexcept
  {
    const auto rounded = checked_add(bits, std::size_t{c_limb_bits} - 1);

    if (!rounded.has_value())
    {
      return result<std::size_t>::failure(*rounded.error_if());
    }

    // The extra limb absorbs the +1 bit of a power of ten and any rounding in the callers' bounds.
    return checked_add(*rounded.value_if() / c_limb_bits, c_additional_limb);
  }

  /**
   * @brief Construct an allocation-free exact zero.
   * @par Requirements
   * P038; J5-046.
   * @param provider Borrowed fallible storage provider.
   */
  big_unsigned::big_unsigned(const storage_callbacks& provider) noexcept : m_provider(provider) {}

  /**
   * @brief Reserve limb storage for an upper bound before loading any digits.
   * @par Requirements
   * P038; J5-037; J5-046.
   * @param limbs Upper bound on the limbs this integer will hold.
   * @return Success, a provider failure, or wrong_type_or_access when nonzero or already reserved.
   */
  status_result big_unsigned::reserve(std::size_t limbs) noexcept
  {
    // One reservation per integer, made while it is still zero, so nothing needs copying.
    if ((m_size != 0) || (m_capacity != 0))
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    if (limbs == 0)
    {
      return status_result::success();
    }

    auto allocation = allocate_array(m_provider, limbs, sizeof(limb_type), alignof(limb_type));

    if (!allocation.has_value())
    {
      return status_result::failure(*allocation.error_if());
    }

    m_storage = std::move(*allocation.value_if());
    m_capacity = limbs;

    return status_result::success();
  }

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
   * @brief Count bits below the highest set bit of the top limb.
   * @par Requirements
   * P038; J5-003; J5-037.
   * @return Exact significant bit count or an explicit size failure.
   * @note Limb storage bounds bytes, not bits, so the count uses checked arithmetic.
   */
  result<std::size_t> big_unsigned::bit_length() const noexcept
  {
    using length_result = result<std::size_t>;

    if (m_size == 0)
    {
      return length_result::success(0);
    }

    limb_type top = read_limb(m_storage, m_size - c_additional_limb);
    std::size_t top_bits{};

    // Canonical values have a nonzero top limb, so this loop runs at least once.
    while (top != 0)
    {
      ++top_bits;
      top = static_cast<limb_type>(top >> 1U);
    }

    const auto lower_bits = checked_multiply(m_size - c_additional_limb, std::size_t{c_limb_bits});

    if (!lower_bits.has_value())
    {
      return length_result::failure(*lower_bits.error_if());
    }

    const auto total_bits = checked_add(*lower_bits.value_if(), top_bits);

    if (!total_bits.has_value())
    {
      return length_result::failure(*total_bits.error_if());
    }

    return length_result::success(*total_bits.value_if());
  }

  /**
   * @brief Append a digit or decimal chunk using bounded exact limb arithmetic.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @param digit Numeric digit or decimal chunk value below radix.
   * @param radix Supported multiplier: 10, 100, 1000, 10000 or 16.
   * @param work Shared work budget.
   * @return Success or an explicit failure preserving the original value.
   * @note Budget and capacity checks precede mutation; performs no allocation.
   */
  status_result big_unsigned::append_digit(unsigned int digit, unsigned int radix, work_budget& work) noexcept
  {
    const bool supported_radix = (radix == c_decimal_radix) || (radix == c_decimal_radix_2) ||
                                 (radix == c_decimal_radix_3) || (radix == c_decimal_radix_4) ||
                                 (radix == c_hexadecimal_radix);

    if (!supported_radix || (digit >= radix))
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

    // The reservation is an upper bound, so needing more means the bound was wrong.
    if (grows && (m_size >= m_capacity))
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    // No fallible operations remain, so the integer is updated in place.
    carry = digit;

    for (std::size_t index{}; index < m_size; ++index)
    {
      const auto expanded = (static_cast<wide_type>(read_limb(m_storage, index)) * radix) + carry;

      write_limb(m_storage, index, static_cast<limb_type>(expanded % c_limb_radix));

      carry = expanded / c_limb_radix;
    }

    if (grows)
    {
      write_limb(m_storage, m_size, static_cast<limb_type>(carry));
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

    if (destination_count > m_capacity)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    std::size_t significant_count = *base_count.value_if();

    if (partial_bits == 0)
    {
      // Move limbs upwards starting at the top, so no source limb is overwritten before it is read.
      for (std::size_t index = m_size; index != 0;)
      {
        --index;
        write_limb(m_storage, index + whole_limbs, read_limb(m_storage, index));
      }
    }
    else
    {
      const unsigned int carry_bits = c_limb_bits - partial_bits;

      // The bits pushed out of the top limb form a new most significant limb.
      const auto top = static_cast<limb_type>(read_limb(m_storage, m_size - c_additional_limb) >> carry_bits);
      write_limb(m_storage, m_size + whole_limbs, top);

      if (top != 0)
      {
        ++significant_count;
      }

      // Each destination limb joins its source limb with the high bits of the limb below.
      for (std::size_t index = m_size - c_additional_limb; index != 0; --index)
      {
        const auto high = static_cast<wide_type>(read_limb(m_storage, index)) << partial_bits;
        const auto low = static_cast<wide_type>(read_limb(m_storage, index - 1)) >> carry_bits;

        write_limb(m_storage, index + whole_limbs, static_cast<limb_type>((high | low) % c_limb_radix));
      }

      const auto lowest = static_cast<wide_type>(read_limb(m_storage, 0)) << partial_bits;
      write_limb(m_storage, whole_limbs, static_cast<limb_type>(lowest % c_limb_radix));
    }

    // Whole-limb shifting introduces zero limbs at the least significant end.
    for (std::size_t index{}; index < whole_limbs; ++index)
    {
      write_limb(m_storage, index, limb_type{});
    }

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

    // The reservation is an upper bound, so a larger source means the bound was wrong.
    if (other.m_size > m_capacity)
    {
      return status_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    for (std::size_t index{}; index < other.m_size; ++index)
    {
      write_limb(m_storage, index, read_limb(other.m_storage, index));
    }

    m_size = other.m_size;

    return status_result::success();
  }
} // namespace adaptive_json5::detail
