#include "value/big_unsigned.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
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
    constexpr std::uint32_t c_decimal_radix = 10;
    constexpr std::uint32_t c_hexadecimal_radix = 16;
    constexpr std::size_t c_limb_passes = 2;
    constexpr std::size_t c_tail_work_units = 1;
    constexpr std::uint32_t c_limb_bits = 16;
    constexpr std::size_t c_shift_setup_work_units = 1;
    constexpr std::uint32_t c_decimal_radix_2 = 100;
    constexpr std::uint32_t c_decimal_radix_3 = 1000;
    constexpr std::uint32_t c_decimal_radix_4 = 10000;
    constexpr std::size_t c_additional_limb = 1;
    constexpr std::size_t c_lowest_limb = 0;

    /**
     * @brief Read one initialized limb from owned byte storage.
     * @par Requirements
     * P038; J5-037; J5-046.
     * @param storage Block containing the initialized limb.
     * @param index Existing limb index.
     * @return Stored limb value.
     * @note Caller guarantees an in-bounds initialized limb.
     * Byte copying avoids constructing typed objects inside raw storage.
     * @par ID SWU-VALUE-003
     */
    [[nodiscard]] limb_type read_limb(const owned_block& storage, std::size_t index) noexcept
    {
      limb_type value{};
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Limbs are stored as bytes. Copy the chosen limb into a live local value
      // without assuming that the block contains an aligned limb object.
      // codeql::misra_deviation_next_line(DEV-002.15)
      const auto* bytes = static_cast<const std::byte*>(storage.data());

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: index is below the slot count of the block, and the block size (count * sizeof)
      // was computed with checked_multiply when it was allocated, so this product fits std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.38)
      const std::size_t offset = index * sizeof(limb_type);
      // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
      // MISRA justification: std::memcpy copies bytes between non-overlapping buffers whose sizes were checked
      // when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object
      // representation to or from untyped storage without alignment or aliasing assumptions.
      // codeql::misra_deviation_next_line(DEV-023.02)
      static_cast<void>(std::memcpy(&value, bytes + offset, sizeof(value)));

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
     * @par ID SWU-VALUE-004
     */
    // MISRA rule: RULE-10-1-1 cpp/misra/pointer-or-ref-param-not-const
    // MISRA justification: The function writes into the bytes owned by the block. owned_block::data() is const-
    // qualified, but a non-const reference documents that the call modifies the block's contents.
    // codeql::misra_deviation_next_line(DEV-018.01)
    void write_limb(owned_block& storage, std::size_t index, limb_type value) noexcept
    {
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Store the limb's object representation in the reserved byte slot with memcpy,
      // so writing it does not require an aligned limb object in the block.
      // codeql::misra_deviation_next_line(DEV-002.16)
      auto* bytes = static_cast<std::byte*>(storage.data());

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: index is below the slot count of the block, and the block size (count * sizeof)
      // was computed with checked_multiply when it was allocated, so this product fits std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.37)
      const std::size_t offset = index * sizeof(limb_type);
      // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
      // MISRA justification: std::memcpy copies bytes between non-overlapping buffers whose sizes were checked
      // when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object
      // representation to or from untyped storage without alignment or aliasing assumptions.
      // codeql::misra_deviation_next_line(DEV-023.01)
      static_cast<void>(std::memcpy(bytes + offset, &value, sizeof(value)));
    }
  } // namespace

  /**
   * @brief Upper bound on the bits of an integer below 10^digits.
   * @par Requirements
   * P038; J5-037; J5-046.
   * @param digits Number of decimal digits, or the exponent of a power of ten.
   * @return ceil(digits * 10 / 3) bits, or resource_limit on overflow.
   * @par ID SWU-VALUE-005
   */
  result<std::size_t> decimal_digit_bits(std::size_t digits) noexcept
  {
    constexpr std::size_t c_bits_numerator = 10;
    constexpr std::size_t c_bits_denominator = 3;

    const auto scaled = checked_multiply(digits, c_bits_numerator, resource_limit_reason::conversion_size);

    if (!scaled.has_value())
    {
      return result<std::size_t>::failure(*scaled.error_if());
    }

    // Adding denominator - 1 before dividing rounds up.
    const auto rounded =
      checked_add(*scaled.value_if(), c_bits_denominator - 1, resource_limit_reason::conversion_size);

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
   * @par ID SWU-VALUE-006
   */
  result<std::size_t> limbs_for_bits(std::size_t bits) noexcept
  {
    const auto rounded =
      checked_add(bits, static_cast<std::size_t>(c_limb_bits) - 1U, resource_limit_reason::conversion_size);

    if (!rounded.has_value())
    {
      return result<std::size_t>::failure(*rounded.error_if());
    }

    // The extra limb absorbs the +1 bit of a power of ten and any rounding in the callers' bounds.
    return checked_add(*rounded.value_if() / c_limb_bits, c_additional_limb, resource_limit_reason::conversion_size);
  }

  /**
   * @brief Construct an allocation-free exact zero.
   * @par Requirements
   * P038; J5-046.
   * @param provider Borrowed fallible storage provider.
   * @par ID SWU-VALUE-007
   */
  big_unsigned::big_unsigned(const storage_callbacks& provider) noexcept : m_provider(provider) {}

  /**
   * @brief Reserve limb storage for an upper bound before loading any digits.
   * @par Requirements
   * P038; J5-037; J5-046.
   * @param limbs Upper bound on the limbs this integer will hold.
   * @return Success, a provider failure, or internal_error when nonzero or already reserved.
   * @par ID SWU-VALUE-008
   */
  status_result big_unsigned::reserve(std::size_t limbs) noexcept
  {
    // One reservation per integer, made while it is still zero, so nothing needs copying.
    if ((m_size != 0) || (m_capacity != 0))
    {
      return status_result::failure(error::internal_fault());
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

    // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
    // MISRA justification: The object is moved exactly once in this expression and is not read again
    // afterwards; the reported accesses are the operand expressions that form this same move (for example
    // value_if() on a result that still holds its value).
    // codeql::misra_deviation_next_line(DEV-024.01)
    m_storage = std::move(*allocation.value_if());
    m_capacity = limbs;

    return status_result::success();
  }

  /**
   * @brief Report whether no nonzero limbs are stored.
   * @par Requirements
   * P038; J5-037.
   * @par ID SWU-VALUE-009
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
   * @par ID SWU-VALUE-010
   */
  result<std::size_t> big_unsigned::bit_length() const noexcept
  {
    using length_result = result<std::size_t>;

    if (m_size == 0)
    {
      return length_result::success(0);
    }

    // Widen first so the shifts and comparisons stay unsigned.
    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: Callers reach this only for a nonzero value, which has at least one limb, so m_size
    // - 1 is the top limb index.
    // codeql::misra_deviation_next_line(DEV-004.36)
    auto top = static_cast<wide_type>(read_limb(m_storage, m_size - c_additional_limb));
    std::size_t top_bits{};

    // Canonical values have a nonzero top limb, so this loop runs at least once.
    while (top != 0U)
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Counts the bits of one 16-bit limb, so top_bits is at most 16.
      // codeql::misra_deviation_next_line(DEV-004.35)
      ++top_bits;
      top >>= 1U;
    }

    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: Callers reach this only for a nonzero value, which has at least one limb, so m_size
    // - 1 is the top limb index.
    // codeql::misra_deviation_next_line(DEV-004.34)
    const auto lower_bits = checked_multiply(m_size - c_additional_limb, static_cast<std::size_t>(c_limb_bits),
                                             resource_limit_reason::conversion_size);

    if (!lower_bits.has_value())
    {
      return length_result::failure(*lower_bits.error_if());
    }

    const auto total_bits = checked_add(*lower_bits.value_if(), top_bits, resource_limit_reason::conversion_size);

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
   * @par ID SWU-VALUE-011
   */
  status_result big_unsigned::append_digit(std::uint32_t digit, std::uint32_t radix, work_budget& work) noexcept
  {
    const bool supported_radix = (radix == c_decimal_radix) || (radix == c_decimal_radix_2) ||
                                 (radix == c_decimal_radix_3) || (radix == c_decimal_radix_4) ||
                                 (radix == c_hexadecimal_radix);

    if (!supported_radix || (digit >= radix))
    {
      return status_result::failure(error::internal_fault());
    }

    const auto passes = checked_multiply(m_size, c_limb_passes, resource_limit_reason::work);

    if (!passes.has_value())
    {
      return status_result::failure(*passes.error_if());
    }

    const auto units = checked_add(*passes.value_if(), c_tail_work_units, resource_limit_reason::work);

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

    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
    // bound that the body does not modify, and changed only in the increment expression. The counter is
    // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
    // codeql::misra_deviation_next_line(DEV-017.08)
    for (std::size_t index{}; index < m_size; ++index)
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The limb is below 2^16, radix and carry are at most 10000, so the result is below
      // 2^32 and fits wide_type.
      // codeql::misra_deviation_next_line(DEV-004.33)
      const wide_type expanded = (static_cast<wide_type>(read_limb(m_storage, index)) * radix) + carry;

      carry = expanded / c_limb_radix;
    }

    const bool grows = (carry != 0);

    // The reservation is an upper bound, so needing more means the bound was wrong.
    if (grows && (m_size >= m_capacity))
    {
      return status_result::failure(error::internal_fault());
    }

    // No fallible operations remain, so the integer is updated in place.
    carry = digit;

    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
    // bound that the body does not modify, and changed only in the increment expression. The counter is
    // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
    // codeql::misra_deviation_next_line(DEV-017.07)
    for (std::size_t index{}; index < m_size; ++index)
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The limb is below 2^16, radix and carry are at most 10000, so the result is below
      // 2^32 and fits wide_type.
      // codeql::misra_deviation_next_line(DEV-004.32)
      const wide_type expanded = (static_cast<wide_type>(read_limb(m_storage, index)) * radix) + carry;

      write_limb(m_storage, index, static_cast<limb_type>(expanded % c_limb_radix));

      carry = expanded / c_limb_radix;
    }

    if (grows)
    {
      write_limb(m_storage, m_size, static_cast<limb_type>(carry));
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The value's capacity was reserved for the extra limb before this call, so m_size
      // stays within it.
      // codeql::misra_deviation_next_line(DEV-004.31)
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
   * @par ID SWU-VALUE-012
   */
  result<magnitude_order> big_unsigned::compare(const big_unsigned& other, work_budget& work) const noexcept
  {
    using comparison_result = result<magnitude_order>;

    constexpr std::size_t c_setup_work_units = 1;

    const std::size_t traversal_units = (m_size == other.m_size) ? m_size : 0;

    const auto units = checked_add(traversal_units, c_setup_work_units, resource_limit_reason::work);

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

      // Widen first so the comparisons stay unsigned.
      const auto left = static_cast<wide_type>(read_limb(m_storage, index));
      const auto right = static_cast<wide_type>(read_limb(other.m_storage, index));

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
   * @brief Account for shifting the source limbs into the full destination width.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @par ID SWU-VALUE-192
   */
  static status_result charge_shift(std::size_t size, std::size_t destination_count, work_budget& work) noexcept
  {
    const auto traversal_units = checked_add(size, destination_count, resource_limit_reason::work);

    if (!traversal_units.has_value())
    {
      return status_result::failure(*traversal_units.error_if());
    }

    const auto units = checked_add(*traversal_units.value_if(), c_shift_setup_work_units, resource_limit_reason::work);

    if (!units.has_value())
    {
      return status_result::failure(*units.error_if());
    }

    return work.charge(*units.value_if());
  }

  /**
   * @brief Include room for a possible carry when a shift crosses a limb boundary.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @par ID SWU-VALUE-193
   */
  static result<std::size_t> shifted_capacity(std::size_t destination_count, std::uint32_t partial_bits) noexcept
  {
    if (partial_bits != 0)
    {
      return checked_add(destination_count, c_additional_limb, resource_limit_reason::conversion_size);
    }
    return result<std::size_t>::success(destination_count);
  }

  /**
   * @brief Shift limbs from most to least significant within the prevalidated destination capacity.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @par ID SWU-VALUE-194
   */
  void big_unsigned::shift_reserved_limbs(std::size_t whole_limbs, std::uint32_t partial_bits,
                                          std::size_t significant_count) noexcept
  {
    if (partial_bits == 0)
    {
      // Move limbs upwards starting at the top, so no source limb is overwritten before it is read.
      std::size_t index = m_size;

      while (index != 0U)
      {
        --index;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: The capacity for m_size + whole_limbs limbs was reserved before shifting, so the
        // destination index fits.
        // codeql::misra_deviation_next_line(DEV-004.30)
        write_limb(m_storage, index + whole_limbs, read_limb(m_storage, index));
      }
    }
    else
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: partial_bits is bit_count % c_limb_bits, so it is below c_limb_bits.
      // codeql::misra_deviation_next_line(DEV-004.29)
      const std::uint32_t carry_bits = c_limb_bits - partial_bits;

      // The bits pushed out of the top limb form a new most significant limb.
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Callers reach this only for a nonzero value, which has at least one limb, so
      // m_size - 1 is the top limb index.
      // codeql::misra_deviation_next_line(DEV-004.28)
      const auto top_limb = static_cast<wide_type>(read_limb(m_storage, m_size - c_additional_limb));
      const wide_type top = top_limb >> carry_bits;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The capacity for m_size + whole_limbs limbs was reserved before shifting, so the
      // destination index fits.
      // codeql::misra_deviation_next_line(DEV-004.27)
      write_limb(m_storage, m_size + whole_limbs, static_cast<limb_type>(top));

      if (top != 0U)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Counts limbs within the capacity reserved for the shifted value, so it cannot
        // wrap.
        // codeql::misra_deviation_next_line(DEV-004.26)
        ++significant_count;
      }

      // Each destination limb joins its source limb with the high bits of the limb below.
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Callers reach this only for a nonzero value, which has at least one limb, so
      // m_size - 1 is the top limb index.
      // codeql::misra_deviation_next_line(DEV-004.25)
      std::size_t index = m_size - c_additional_limb;

      while (index != 0U)
      {
        const auto high = static_cast<wide_type>(read_limb(m_storage, index)) << partial_bits;
        const auto low = static_cast<wide_type>(read_limb(m_storage, index - 1U)) >> carry_bits;

        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: The capacity for m_size + whole_limbs limbs was reserved before shifting, so the
        // destination index fits.
        // codeql::misra_deviation_next_line(DEV-004.24)
        write_limb(m_storage, index + whole_limbs, static_cast<limb_type>((high | low) % c_limb_radix));
        --index;
      }

      const auto lowest = static_cast<wide_type>(read_limb(m_storage, c_lowest_limb)) << partial_bits;
      write_limb(m_storage, whole_limbs, static_cast<limb_type>(lowest % c_limb_radix));
    }

    // Whole-limb shifting introduces zero limbs at the least significant end.
    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
    // bound that the body does not modify, and changed only in the increment expression. The counter is
    // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
    // codeql::misra_deviation_next_line(DEV-017.06)
    for (std::size_t index{}; index < whole_limbs; ++index)
    {
      write_limb(m_storage, index, limb_type{});
    }

    m_size = significant_count;
  }

  /**
   * @brief Shift through replacement storage before publishing the new magnitude.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @param bit_count Exact power-of-two scaling exponent.
   * @param work Shared budget charged before limb processing.
   * @return Success or an explicit failure preserving the original value.
   * @note Allocation and size checks precede all writes to replacement storage.
   * @par ID SWU-VALUE-013
   */
  status_result big_unsigned::shift_left(std::size_t bit_count, work_budget& work) noexcept
  {
    if ((bit_count == 0) || (m_size == 0))
    {
      return status_result::success();
    }

    const std::size_t whole_limbs = bit_count / c_limb_bits;

    const auto partial_bits = static_cast<std::uint32_t>(bit_count % c_limb_bits);

    const auto base_count = checked_add(m_size, whole_limbs, resource_limit_reason::conversion_size);

    if (!base_count.has_value())
    {
      return status_result::failure(*base_count.error_if());
    }

    const auto capacity = shifted_capacity(*base_count.value_if(), partial_bits);
    if (!capacity.has_value())
    {
      return status_result::failure(*capacity.error_if());
    }
    const std::size_t destination_count = *capacity.value_if();

    const auto charged = charge_shift(m_size, destination_count, work);
    if (!charged.has_value())
    {
      return status_result::failure(*charged.error_if());
    }

    if (destination_count > m_capacity)
    {
      return status_result::failure(error::internal_fault());
    }

    shift_reserved_limbs(whole_limbs, partial_bits, *base_count.value_if());

    return status_result::success();
  }

  /**
   * @brief Charge both subtraction passes and their setup before changing any limbs.
   * @par Requirements
   * P038; J5-003; J5-037; J5-048.
   * @par ID SWU-VALUE-195
   */
  static status_result charge_subtraction(std::size_t size, work_budget& work) noexcept
  {
    constexpr std::size_t c_subtraction_passes = 2;
    constexpr std::size_t c_subtraction_setup_units = 1;
    const auto passes = checked_multiply(size, c_subtraction_passes, resource_limit_reason::work);

    if (!passes.has_value())
    {
      return status_result::failure(*passes.error_if());
    }

    const auto units = checked_add(*passes.value_if(), c_subtraction_setup_units, resource_limit_reason::work);

    if (!units.has_value())
    {
      return status_result::failure(*units.error_if());
    }

    return work.charge(*units.value_if());
  }

  /**
   * @brief Subtract with borrow after validating ordering and reserving work.
   * @par Requirements
   * P038; J5-003; J5-037; J5-048.
   * @param other Borrowed subtrahend, including this object itself.
   * @param work Shared work budget.
   * @return Success or an explicit failure before mutation.
   * @note Retains allocated storage and restores canonical significant-limb count.
   * @par ID SWU-VALUE-014
   */
  status_result big_unsigned::subtract(const big_unsigned& other, work_budget& work) noexcept
  {
    constexpr wide_type c_borrow_units = 1;

    const auto ordering = compare(other, work);

    if (!ordering.has_value())
    {
      return status_result::failure(*ordering.error_if());
    }

    if (*ordering.value_if() == magnitude_order::less)
    {
      return status_result::failure(error::internal_fault());
    }

    if (other.m_size == 0)
    {
      return status_result::success();
    }

    const auto charged = charge_subtraction(m_size, work);
    if (!charged.has_value())
    {
      return status_result::failure(*charged.error_if());
    }

    wide_type borrow{};

    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
    // bound that the body does not modify, and changed only in the increment expression. The counter is
    // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
    // codeql::misra_deviation_next_line(DEV-017.05)
    for (std::size_t index{}; index < m_size; ++index)
    {
      const auto left = static_cast<wide_type>(read_limb(m_storage, index));

      const wide_type right =
        (index < other.m_size) ? static_cast<wide_type>(read_limb(other.m_storage, index)) : wide_type{};

      const wide_type subtrahend = right + borrow;
      // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
      // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
      // every path assigns the real value before use. For constants and results, the value is read later in the
      // same function or template instantiation, which the tool does not track.
      // codeql::misra_deviation_next_line(DEV-007.19)
      wide_type difference{};

      if (left < subtrahend)
      {
        // Borrow one base-65536 unit from the next limb.
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: left is below the limb radix and subtrahend is at most the radix, so the
        // borrowed difference lies in 0 to 2^17 in the 32-bit wide type.
        // codeql::misra_deviation_next_line(DEV-004.23)
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
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Callers reach this only for a nonzero value, which has at least one limb, so
      // m_size - 1 is the top limb index.
      // codeql::misra_deviation_next_line(DEV-004.22)
      const std::size_t highest_index = m_size - c_additional_limb;

      const auto highest = static_cast<wide_type>(read_limb(m_storage, highest_index));

      if (highest != 0U)
      {
        break;
      }

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: The loop runs only while m_size is nonzero, so m_size is at least one.
      // codeql::misra_deviation_next_line(DEV-004.21)
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
   * @par ID SWU-VALUE-015
   */
  status_result big_unsigned::assign(const big_unsigned& other, work_budget& work) noexcept
  {
    constexpr std::size_t c_copy_setup_working_units = 1;

    if (this == &other)
    {
      return status_result::success();
    }

    const auto units = checked_add(other.m_size, c_copy_setup_working_units, resource_limit_reason::work);

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
      return status_result::failure(error::internal_fault());
    }

    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
    // bound that the body does not modify, and changed only in the increment expression. The counter is
    // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
    // codeql::misra_deviation_next_line(DEV-017.04)
    for (std::size_t index{}; index < other.m_size; ++index)
    {
      write_limb(m_storage, index, read_limb(other.m_storage, index));
    }

    m_size = other.m_size;

    return status_result::success();
  }
} // namespace adaptive_json5::detail
