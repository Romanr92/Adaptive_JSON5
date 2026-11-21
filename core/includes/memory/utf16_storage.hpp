#ifndef ADAPTIVE_JSON5_INTERNAL_MEMORY_UTF16_STORAGE_HPP_
#define ADAPTIVE_JSON5_INTERNAL_MEMORY_UTF16_STORAGE_HPP_

#include <cstddef>
#include <cstdint>

namespace adaptive_json5
{
  namespace detail
  {
    /** @brief Bytes that store one UTF-16 code unit in decoded string storage. */
    constexpr std::size_t c_utf16_unit_bytes = 2U;

    /**
     * @brief Write UTF-16 code units into byte storage, low byte first.
     * @par Requirements
     * P013; J5-029; J5-040.
     * @param destination First byte to write; must have room for count * c_utf16_unit_bytes.
     * @param units Code units to store; may be nullptr only when count is zero.
     * @param count Number of code units.
     * @note Writes each byte explicitly, so no memcpy or pointer cast is needed
     * (MISRA C++:2023). The layout is fixed little-endian and read back by load_utf16_unit().
     * @par ID SWU-MEMORY-078
     */
    inline void store_utf16_units(std::byte* destination, const std::uint16_t* units, std::size_t count) noexcept
    {
      constexpr std::uint32_t c_low_byte_mask = 0xFFU;
      constexpr std::uint32_t c_bits_per_byte = 8U;

      std::byte* next = destination;
      const std::uint16_t* source = units;
      std::size_t remaining = count;

      while (remaining != std::size_t{})
      {
        const auto unit = static_cast<std::uint32_t>(*source);
        *next = static_cast<std::byte>(unit & c_low_byte_mask);
        ++next;
        *next = static_cast<std::byte>(unit >> c_bits_per_byte);
        ++next;
        ++source;
        --remaining;
      }
    }

    /**
     * @brief Read one UTF-16 code unit stored by store_utf16_units().
     * @par Requirements
     * P013; J5-018; J5-029.
     * @param unit_bytes First of the c_utf16_unit_bytes bytes of the unit.
     * @return The code unit.
     * @par ID SWU-MEMORY-079
     */
    [[nodiscard]] inline std::uint16_t load_utf16_unit(const std::byte* unit_bytes) noexcept
    {
      constexpr std::uint32_t c_bits_per_byte = 8U;

      const std::byte* next = unit_bytes;
      const auto low = std::to_integer<std::uint32_t>(*next);
      ++next;
      const auto high = std::to_integer<std::uint32_t>(*next);

      return static_cast<std::uint16_t>(low | (high << c_bits_per_byte));
    }
  } // namespace detail
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_INTERNAL_MEMORY_UTF16_STORAGE_HPP_ */
