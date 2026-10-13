#ifndef ADAPTIVE_JSON5_CURSOR_HPP_
#define ADAPTIVE_JSON5_CURSOR_HPP_

#include "input.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Bounded cursor tracking current position in borrowed input bytes.
   * @par Requirements
   * P004; J5-004; J5-041; J5-044.
   * @note Never reads past the explicit buffer length.
   * Operates without requiring or assuming a terminating NUL byte.
   */
  class cursor
  {
    private:
      input_view m_view{};
      std::size_t m_offset = 0;

    public:
      static constexpr std::size_t c_zero_bytes = 0;
      /**
       * @brief Construct a cursor over borrowed input.
       * @par Requirements
       * P004; J5-004.
       */
      constexpr explicit cursor(input_view view) noexcept : m_view(view), m_offset(c_zero_bytes) {}

      constexpr cursor() noexcept = default;

      /**
       * @brief Check if the cursor has reached or exceeded the end of the input.
       * @par Requirements
       * P004; J5-004.
       */
      constexpr bool is_at_end() const noexcept
      {
        return m_offset >= m_view.size;
      }

      /**
       * @brief Return the zero-based original byte offset in input.
       * @par Requirements
       * P004; J5-041; J5-044.
       */
      constexpr std::size_t offset() const noexcept
      {
        return m_offset;
      }

      /**
       * @brief Return the number of unconsumed bytes remaining.
       * @par Requirements
       * P004; J5-004.
       */
      constexpr std::size_t remaining() const noexcept
      {
        return (m_offset < m_view.size) ? (m_view.size - m_offset) : c_zero_bytes;
      }

      /**
       * @brief Inspect the byte at the current offset without advancing.
       * @par Requirements
       * P004; J5-004; J5-044.
       * @return The current byte, or std::nullopt if at end of input.
       */
      constexpr std::optional<char> peek() const noexcept
      {
        if (is_at_end() || (m_view.data == nullptr))
        {
          return std::nullopt;
        }
        return m_view.data[m_offset];
      }

      /**
       * @brief Look ahead by a given byte count without advancing.
       * @par Requirements
       * P004; J5-004.
       * @param delta Zero-based lookahead index from current position.
       * @return The byte at (offset + delta), or std::nullopt if past end.
       */
      constexpr std::optional<char> peek(std::size_t delta) const noexcept
      {
        if ((m_view.data == nullptr) || (delta >= remaining()))
        {
          return std::nullopt;
        }
        return m_view.data[m_offset + delta];
      }

      /**
       * @brief Consume and return the byte at the current offset.
       * @par Requirements
       * P004; J5-004.
       * @return The consumed byte, or std::nullopt if at end of input.
       */
      constexpr std::optional<char> next() noexcept
      {
        if (is_at_end() || (m_view.data == nullptr))
        {
          return std::nullopt;
        }
        return m_view.data[m_offset++];
      }

      /**
       * @brief Advance the cursor forward by count bytes, clamped to end.
       * @par Requirements
       * P004; J5-004.
       */
      constexpr void advance(std::size_t count) noexcept
      {
        const std::size_t rem = remaining();
        m_offset += (count < rem) ? count : rem;
      }

      /**
       * @brief Return an input_view spanning from start_offset to current offset.
       * @par Requirements
       * P004; J5-041.
       */
      constexpr input_view span_from(std::size_t start_offset) const noexcept
      {
        if ((m_view.data == nullptr) || (start_offset >= m_offset))
        {
          return input_view{nullptr, c_zero_bytes};
        }
        return input_view{m_view.data + start_offset, m_offset - start_offset};
      }
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_CURSOR_HPP_ */
