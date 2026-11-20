#ifndef ADAPTIVE_JSON5_TEST_BYTE_ARRAY_HPP_
#define ADAPTIVE_JSON5_TEST_BYTE_ARRAY_HPP_

#include <array>
#include <cstddef>
#include <cstdint>

namespace adaptive_json5_test
{
  /**
   * @brief Build a std::byte array from octet values for expected-output comparisons.
   * @par Requirements
   * J5-018; J5-029.
   * @param octets Integer or character values in the range 0..255.
   * @return The octets as std::byte, in order.
   * @note std::byte has no conversion from integers, so literal octet lists need this cast.
   */
  template <typename... Octets>
  [[nodiscard]] constexpr std::array<std::byte, sizeof...(Octets)> make_bytes(Octets... octets) noexcept
  {
    return {static_cast<std::byte>(octets)...};
  }

  /**
   * @brief Convert expected octet values to std::byte for comparison with exported output.
   * @par Requirements
   * J5-018; J5-029.
   * @param octets Expected octet values.
   * @return The same octets as std::byte.
   */
  template <std::size_t N>
  [[nodiscard]] constexpr std::array<std::byte, N> to_bytes(const std::array<std::uint8_t, N>& octets) noexcept
  {
    std::array<std::byte, N> converted{};

    for (std::size_t index{}; index < N; ++index)
    {
      converted[index] = static_cast<std::byte>(octets[index]);
    }

    return converted;
  }
  /**
   * @brief Build a char array from octet values for expected UTF-8 output comparisons.
   * @par Requirements

   * * J5-018; J5-029.
   * @param octets Integer or character values in the range 0..255.
   * @return The octets as
   * char, in order; values above 0x7F keep their bit pattern.
   * @note Mirrors how export_utf8() stores each byte
   * into its char buffer.
   */
  template <typename... Octets>
  [[nodiscard]] constexpr std::array<char, sizeof...(Octets)> make_chars(Octets... octets) noexcept
  {
    return {static_cast<char>(static_cast<std::uint8_t>(octets))...};
  }

  /**
   * @brief Convert expected octet values to char for comparison with exported output.
   * @par Requirements
   *
   * J5-018; J5-029.
   * @param octets Expected octet values.
   * @return The same octets as char.
   */
  template <std::size_t N>
  [[nodiscard]] constexpr std::array<char, N> to_chars(const std::array<std::uint8_t, N>& octets) noexcept
  {
    std::array<char, N> converted{};

    for (std::size_t index{}; index < N; ++index)
    {
      converted[index] = static_cast<char>(octets[index]);
    }

    return converted;
  }
} // namespace adaptive_json5_test

#endif /* ADAPTIVE_JSON5_TEST_BYTE_ARRAY_HPP_ */
