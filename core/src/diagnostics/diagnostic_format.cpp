#include "adaptive_json5/diagnostic_format.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <limits>
#include <string_view>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Append message pieces to caller storage while counting the untruncated length.
     * @par Requirements
     * J5-045; issue #96.
     * @note Writes at most capacity - 1 characters and reserves the last byte for NUL.
     * Keeps counting after the buffer is full so callers learn the required size.
     * Performs no allocation and never writes outside buffer.
     */
    class message_writer
    {
      private:
        char* m_buffer;
        std::size_t m_capacity;
        std::size_t m_length{};

        /**
         * @brief Start an empty message in caller storage.
         * @par Requirements
         * Issue #96.
         * @param buffer Caller storage, or nullptr when capacity is 0.
         * @param capacity Size of buffer in bytes, including the NUL.
         */
        message_writer(char* buffer, std::size_t capacity) noexcept : m_buffer{buffer}, m_capacity{capacity} {}

      public:
        /**
         * @brief Create a writer, rejecting storage it could not write safely.
         * @par Requirements
         * J5-045; issue #96.
         * @param buffer Caller storage, or nullptr when capacity is 0.
         * @param capacity Size of buffer in bytes, including the NUL.
         * @return The writer, or wrong_type_or_access for a null buffer with nonzero capacity.
         * @note A null buffer with zero capacity is a valid size query.
         */
        [[nodiscard]] static result<message_writer> create(char* buffer, std::size_t capacity) noexcept
        {
          if ((buffer == nullptr) && (capacity != 0U))
          {
            return result<message_writer>::failure(error::without_location(error_code::wrong_type_or_access));
          }

          return result<message_writer>::success(message_writer{buffer, capacity});
        }

        /**
         * @brief Append text, copying only what still fits before the reserved NUL byte.
         * @par Requirements
         * Issue #96.
         * @param text Characters to append; contains no input bytes.
         */
        void append(std::string_view text) noexcept
        {
          for (const char character : text)
          {
            // The last byte stays free for the terminator, so stop copying one byte early.
            if ((m_capacity != 0U) && (m_length < (m_capacity - 1U)))
            {
              m_buffer[m_length] = character;
            }

            ++m_length;
          }
        }

        /**
         * @brief Append an unsigned number in decimal without locale or allocation.
         * @par Requirements
         * Issue #96.
         * @param value Number to print, e.g. a line or column.
         */
        void append_number(std::size_t value) noexcept
        {
          // Enough room for the largest std::size_t in decimal (20 digits for 64 bits).
          constexpr std::size_t c_max_digits = std::numeric_limits<std::size_t>::digits10 + 1U;
          char digits[c_max_digits]{};
          std::size_t start = c_max_digits;

          // Produce digits from the right, so the loop needs no reversal step.
          do
          {
            --start;
            digits[start] = static_cast<char>('0' + (value % 10U));
            value /= 10U;
          } while (value != 0U);

          append(std::string_view{&digits[start], c_max_digits - start});
        }

        /**
         * @brief Terminate the stored text and report the full message length.
         * @par Requirements
         * Issue #96.
         * @return Untruncated length excluding the NUL.
         * @note Writes the NUL after the last stored character whenever capacity is nonzero.
         */
        std::size_t finish() noexcept
        {
          if (m_capacity != 0U)
          {
            // Truncated text ends at the reserved byte; complete text ends right after itself.
            m_buffer[(m_length < m_capacity) ? m_length : (m_capacity - 1U)] = '\0';
          }

          return m_length;
        }
    };
  } // namespace
} // namespace adaptive_json5
