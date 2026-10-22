#ifndef ADAPTIVE_JSON5_DETAIL_BIG_UNSIGNED_HPP_
#define ADAPTIVE_JSON5_DETAIL_BIG_UNSIGNED_HPP_

#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5::detail
{
  /**
   * @brief Describe the ordering of two exact unsigned magnitudes.
   * @par Requirements
   * P038; J5-037.
   * @par Outcomes
   * - less: The left magnitude is smaller.
   * - equal: Both magnitudes have the same value.
   * - greater: The left magnitude is larger.
   * @note Describes magnitude only; signs and non-finite values are handled separately.
   */
  enum class magnitude_order
  {
    less,
    equal,
    greater
  };

  /**
   * @brief Own an exact unsigned integer using fallible caller storage.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @note Zero has no significant limbs. Initial construction requires no allocation.
   * Operations may retain allocated capacity after producing zero..
   * Limbs contain 16 significant bits, least significant first.
   * The provider state must outlive this object and its allocations.
   * Use accounted-storage callbacks to enforce a shared scratch-byte ceiling.
   * Copying and moving are initially unavailable.
   *
   */
  class [[nodiscard]] big_unsigned
  {
    private:
      storage_callbacks m_provider;
      owned_block m_storage{};
      std::size_t m_size{};

    public:
      /**
       * @brief Construct zero without allocating storage.
       * @par Requirements
       * P038; J5-046.
       * @param provider Fallible storage provider borrowed for future allocations.
       */
      explicit big_unsigned(const storage_callbacks& provider) noexcept;

      /**
       * @brief Release any owned limb storage through its original provider.
       * @par Requirements
       * P038; J5-040; J5-046.
       * @note Performs no allocation.
       */
      ~big_unsigned() noexcept = default;

      big_unsigned(const big_unsigned&) = delete;
      big_unsigned& operator=(const big_unsigned&) = delete;
      big_unsigned(big_unsigned&&) = delete;
      big_unsigned& operator=(big_unsigned&&) = delete;

      /**
       * @brief Report whether the exact value is zero.
       * @par Requirements
       * P038; J5-037.
       * @note Performs constant work without allocation.
       */
      [[nodiscard]] bool is_zero() const noexcept;

      /**
       * @brief Multiply by a spelling radix and add one numeric digit.
       * @par Requirements
       * P038; J5-003; J5-037; J5-046; J5-048.
       * @param digit Numeric digit value below radix.
       * @param radix Supported spelling base: 10 or 16.
       * @param work Shared budget charged before limb processing.
       * @return Success or an explicit access, storage or resource failure.
       * @note Invalid radix/digit returns wrong_type_or_access.
       * Charges two work units per existing limb plus one.
       * Failure preserves the integer; successful work charges are not refunded.
       * Growth temporarily retains both old and new allocations.
       */
      [[nodiscard]] status_result append_digit(unsigned int digit, unsigned int radix, work_budget& work) noexcept;

      /**
       * @brief Compare this exact magnitude with another.
       * @par Requirements
       * P038; J5-003; J5-037; J5-048.
       * @param other Borrowed magnitude to compare against.
       * @param work Shared budget charged before traversal.
       * @return Exact ordering or an explicit resource failure.
       * @note Allocates nothing and modifies neither integer.
       * Charges one setup unit and, for equal limb counts, one unit per limb.
       * Reserves the full traversal charge even when a difference is found early.
       * Relies on zero having no limbs and nonzero values having no leading zero limbs.
       */
      [[nodiscard]] result<magnitude_order> compare(const big_unsigned& other, work_budget& work) const noexcept;

      /**
       * @brief Multiply this exact magnitude by a power of two.
       * @par Requirements
       * P038; J5-003; J5-037; J5-046; J5-048.
       * @param bit_count Number of binary positions to shift left.
       * @param work Shared budget charged before traversal or allocation.
       * @return Success or an explicit storage/resource failure.
       * @note A zero shift or zero value succeeds without allocation or work charge.
       * Failure preserves the integer; successful work charges are not refunded.
       * Nontrivial shifts build a replacement while retaining the old allocation.
       * Charges one setup unit, one per source limb and one per destination slot.
       * A partial-limb shift reserves one additional slot, possibly unused.
       * Unused capacity does not count as a significant limb.
       */
      [[nodiscard]] status_result shift_left(std::size_t bit_count, work_budget& work) noexcept;

      /**
       * @brief Subtract another magnitude without producing a negative result.
       * @par Requirements
       * P038; J5-003; J5-037; J5-048.
       * @param other Borrowed magnitude to subtract; may refer to this object.
       * @param work Shared budget charged before traversal and mutation.
       * @return Success, wrong_type_or_access for a negative result,
       * or an explicit resource failure.
       * @note Allocates nothing. Failure preserves the integer.
       * Charges comparison work first.
       * For a nonzero subtrahend, additionally charges two units per original
       * destination limb plus one before mutation.
       * Removes zero most-significant limbs from the significant count.
       * Existing storage remains allocated, including when the result is zero.
       */
      [[nodiscard]] status_result subtract(const big_unsigned& other, work_budget& work) noexcept;

      /**
       * @brief Copy another exact magnitude using this object's storage provider.
       * @par Requirements
       * P038; J5-003; J5-037; J5-046; J5-048.
       * @param other Borrowed source magnitude; may refer to this object.
       * @param work Shared budget charged before allocation or traversal.
       * @return Success or an explicit storage/resource failure.
       * @note Self-assignment succeeds without allocation or work charge.
       * Other copies charge one setup unit plus one unit per source limb.
       * Failure preserves the destination; successful charges are not refunded.
       * Copying zero retains existing storage but clears the significant count.
       * Nonzero copies temporarily retain old and replacement allocations.
       */
      [[nodiscard]] status_result assign(const big_unsigned& other, work_budget& work) noexcept;
  };
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_DETAIL_BIG_UNSIGNED_HPP_ */
