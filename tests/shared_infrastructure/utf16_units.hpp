#ifndef ADAPTIVE_JSON5_TEST_UTF16_UNITS_HPP_
#define ADAPTIVE_JSON5_TEST_UTF16_UNITS_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace adaptive_json5_test
{
  /**
   * @brief Copy a UTF-16 string literal into std::uint16_t code units, without its terminator.
   * @par Requirements
   * J5-018; J5-029.
   * @param text A u"..." literal; the test-only way to spell UTF-16 text readably (ADR 0008).
   * @return The code units as the library's std::uint16_t type.
   */
  template <std::size_t N>
  constexpr std::array<std::uint16_t, N - 1U> utf16(const char16_t (&text)[N]) noexcept
  {
    std::array<std::uint16_t, N - 1U> units{};
    for (std::size_t index{}; index + 1U < N; ++index)
    {
      units[index] = static_cast<std::uint16_t>(text[index]);
    }
    return units;
  }

  /**
   * @brief Copy a std::u16string of expected test text into std::uint16_t code units.
   * @par Requirements
   * J5-018; J5-029.
   * @param text UTF-16 test text (ADR 0008: UTF-16 literals stay in test code only).
   * @return The code units as the library's std::uint16_t type.
   */
  inline std::vector<std::uint16_t> utf16(const std::u16string& text)
  {
    std::vector<std::uint16_t> units;
    units.reserve(text.size());
    for (const auto unit : text)
    {
      units.push_back(static_cast<std::uint16_t>(unit));
    }
    return units;
  }
} // namespace adaptive_json5_test

#endif /* ADAPTIVE_JSON5_TEST_UTF16_UNITS_HPP_ */
