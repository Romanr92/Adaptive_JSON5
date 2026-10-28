#ifndef ADAPTIVE_JSON5_RESULT_HPP_
#define ADAPTIVE_JSON5_RESULT_HPP_

#include "adaptive_json5/error.hpp"

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  /**
   * @brief Result containing either a success value or an error.
   * @par Requirements
   * P002; J5-003; J5-045.
   * @note Alternatives are stored inline and inspected without throwing.
   * Assignment is disabled. Copying is available only for copyable payloads.
   */
  template <typename T, typename E = error>
  class [[nodiscard]] result
  {
      /**
       * @brief Require non-throwing construction and destruction.
       * @par Requirements
       * P002; J5-003; J5-045.
       * @note Move-only success values are supported.
       */

      // Success construction transfers the value into inline storage.
      static_assert(std::is_nothrow_move_constructible_v<T>, "T must support non-throwing move construction");

      // Permit move-only values; when copying is available, it must not throw.
      static_assert(!std::is_copy_constructible_v<T> || std::is_nothrow_copy_constructible_v<T>,
                    "Copy construction of T must be non-throwing when available");

      // Destroying the active value must not throw.
      static_assert(std::is_nothrow_destructible_v<T>, "T must support non-throwing destruction");

      // Failure construction and copying a result must safely copy its error.
      static_assert(std::is_nothrow_copy_constructible_v<E>, "E must support non-throwing copy construction");

      // Moving a failed result must safely transfer its error.
      static_assert(std::is_nothrow_move_constructible_v<E>, "E must support non-throwing move construction");

      static_assert(std::is_nothrow_destructible_v<E>, "E must support non-throwing destruction");

    private:
      static constexpr std::size_t c_value_index = 0;
      static constexpr std::size_t c_error_index = 1;

      /**
       * @brief Default construction is not allowed.
       * @par Requirements
       * P002; J5-003.
       * @note Every result is either a success or a failure; callers choose explicitly
       * through success() or failure().
       */
      result() = delete;

      // Variant manages the lifetime of the active value or error
      std::variant<T, E> m_storage;

      /**
       * @brief Initialize the success alternative directly.
       * @par Requirements
       * P002; J5-003.
       */
      constexpr result(std::in_place_index_t<c_value_index>, T&& value) noexcept
          : m_storage(std::in_place_index<c_value_index>, std::move(value))
      {
      }

      /**
       * @brief Initialize the error alternative directly.
       * @par Requirements
       * P002; J5-003; J5-045.
       */
      constexpr result(std::in_place_index_t<c_error_index>, E&& reason) noexcept
          : m_storage(std::in_place_index<c_error_index>, std::move(reason))
      {
      }

    public:
      /**
       * @brief Construct a successful result containing a value.
       * @par Requirements
       * P002; J5-003.
       */
      [[nodiscard]] static constexpr result success(T value) noexcept
      {
        return result{std::in_place_index<c_value_index>, std::move(value)};
      }

      /**
       * @brief Construct a failed result containing an error.
       * @par Requirements
       * P002; J5-003; J5-045.
       */
      [[nodiscard]] static constexpr result failure(E reason) noexcept
      {
        return result{std::in_place_index<c_error_index>, std::move(reason)};
      }

      /**
       * @brief Copy a result when its alternatives support copying.
       * @par Requirements
       * P002; J5-003.
       */
      result(const result&) noexcept = default;

      /**
       * @brief Move the active alternative into another result.
       * @par Requirements
       * P002; J5-003.
       */
      result(result&&) noexcept = default;

      /**
       * @brief Copy assignment is not allowed.
       * @par Requirements
       * P002; J5-003.
       * @note Assignment would have to switch a live value or error in place; construct a
       * new result instead.
       */
      result& operator=(const result&) = delete;

      /**
       * @brief Move assignment is not allowed.
       * @par Requirements
       * P002; J5-003.
       * @note Same reason as copy assignment.
       */
      result& operator=(result&&) = delete;

      /**
       * @brief Report whether the result contains a success value.
       * @par Requirements
       * P002; J5-003.
       */
      [[nodiscard]] constexpr bool has_value() const noexcept
      {
        return m_storage.index() == c_value_index;
      }

      /**
       * @brief Inspect the success value without modifying it.
       * @par Requirements
       * P002; J5-003.
       * @return Pointer to the stored value, or nullptr on failure.
       * @note The pointer remains dependent on this result's lifetime.
       */
      [[nodiscard]] constexpr const T* value_if() const noexcept
      {
        // Select by index so identical value and error types are supported.
        return std::get_if<c_value_index>(&m_storage);
      }

      /**
       * @brief Access the success value for modification or ownership transfer.
       * @par Requirements
       * P002; J5-003.
       * @return Pointer to the stored value, or nullptr on failure.
       * @note The pointer remains dependent on this result's lifetime.
       */
      [[nodiscard]] constexpr T* value_if() noexcept
      {
        return std::get_if<c_value_index>(&m_storage);
      }

      /**
       * @brief Inspect the failure information without modifying it.
       * @par Requirements
       * P002; J5-003; J5-045.
       * @return Pointer to the stored error, or nullptr on success.
       * @note The pointer remains dependent on this result's lifetime.
       */
      [[nodiscard]] constexpr const E* error_if() const noexcept
      {
        // Checked access returns nullptr when error alternative is inactive.
        return std::get_if<c_error_index>(&m_storage);
      }
  };

  /**
   * @brief Non-throwing success/error result without a success payload.
   * @par Requirements
   * P002; J5-003; J5-045.
   * @note Failure information is stored inline without allocation.
   * error_if() returns nullptr on success. Otherwise, its pointer refers
   * to error storage owned by this result.
   */
  template <typename E>
  class [[nodiscard]] result<void, E>
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
      /**
       * @brief Construct success without a payload.
       * @par Requirements
       * P002; J5-003.
       */
      [[nodiscard]] static constexpr result success() noexcept
      {
        return result{};
      }

      /**
       * @brief Construct failure with a payload.
       * @par Requirements
       * P002; J5-003.
       */
      [[nodiscard]] static constexpr result failure(E reason) noexcept
      {
        return result{reason};
      }

      [[nodiscard]] constexpr bool has_value() const noexcept
      {
        return !m_error.has_value();
      }

      /**
       * @brief Construct failure with a payload.
       * @par Requirements
       * P002; J5-003.
       */
      [[nodiscard]] constexpr const E* error_if() const noexcept
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
