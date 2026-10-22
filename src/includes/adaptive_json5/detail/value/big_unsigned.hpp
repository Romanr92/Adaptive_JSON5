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
   * @brief Own an exact unsigned integer using fallible caller storage.
   * @par Requirements
   * P038; J5-003; J5-037; J5-046; J5-048.
   * @note Zero has no limbs and requires no allocation.
   * Limbs contain 16 significant bits, least significant first.
   * The provider state must outlive this object and its allocations.
   * Use accounted-storage callbacks to enforce a shared scratch-byte ceiling.
   * Copying and moving are initially unavailable.
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
  };
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_DETAIL_BIG_UNSIGNED_HPP_ */
