#ifndef ADAPTIVE_JSON5_RESULT_HPP_
#define ADAPTIVE_JSON5_RESULT_HPP_

#include "error.hpp"

#include <optional>
#include <type_traits>

namespace adaptive_json5
{
  /**
   * @brief Result containing either a success value or an error.
   * @par Requirements
   * P002; J5-003; J5-045.
   * @note The value-bearing definition will be implemented separately.
   */
  template <typename T, typename E = error>
  class result;

  /**
   * @brief Non-throwing success/error result without a success payload.
   * @par Requirements
   * P002; J5-003; J5-045.
   * @note Failure information is stored inline without allocation.
   * error_if() returns nullptr on success. Otherwise, its pointer refers
   * to error storage owned by this result.
   */
  template <typename E>
  class result<void, E>
  {
      /**
       * @brief Reject error types incompatible with non-throwing operations.
       * @par Requirements
       * P002; J5-003; J5-045.
       * @note These checks verify exception guarantees, not allocation behavior.
       */

      // Failure construction copies the error into its inline storage.
      static_assert(std::is_nothrow_copy_constructible_v<E>, "E must support non-throwing copy construction");

      // Moving a result must preserve the non-throwing contract.
      static_assert(std::is_nothrow_move_constructible_v<E>, "E must support non-throwing move construction");

      // The compiler-generated assignment operators also operate on E.
      static_assert(std::is_nothrow_copy_assignable_v<E>, "E must support non-throwing copy assignment");

      static_assert(std::is_nothrow_move_assignable_v<E>, "E must support non-throwing move assignment");

      // Destroying a result must safely destroy its active error.
      static_assert(std::is_nothrow_destructible_v<E>, "E must support non-throwing destruction");

    public:
      static constexpr result success() noexcept
      {
        return result{};
      }

      static constexpr result failure(E reason) noexcept
      {
        return result{reason};
      }

      constexpr bool has_value() const noexcept
      {
        return !m_error.has_value();
      }

      constexpr const E* error_if() const noexcept
      {
        return m_error ? &*m_error : nullptr;
      }

    private:
      constexpr result() noexcept = default;

      constexpr explicit result(E reason) noexcept : m_error(reason) {}

      std::optional<E> m_error;
  };

  /**
   * @brief Success/error result using the library's minimal error type.
   * @par Requirements
   * P002; J5-003; J5-045.
   */
  using status_result = result<void>;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_RESULT_HPP_ */
