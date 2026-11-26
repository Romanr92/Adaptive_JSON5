#ifndef ADAPTIVE_JSON5_CLI_HOST_BUFFER_HPP_
#define ADAPTIVE_JSON5_CLI_HOST_BUFFER_HPP_

#include "adaptive_json5/memory/storage.hpp"

#include <cstddef>

namespace adaptive_json5::cli
{
  /**
   * @brief A byte buffer from the host storage, released on destruction.
   * @par Requirements
   * P056; P057; J5-046; ADR 0009.
   * @note data() is nullptr when the allocation failed; callers report allocation_failure.
   */
  class host_buffer
  {
    private:
      storage_callbacks m_storage;
      char* m_data{};
      std::size_t m_size{};

    public:
      /**
       * @brief Allocate size bytes.
       * @par Requirements
       * P056; J5-046.
       * @par ID SWU-CLI-015
       */
      host_buffer(const storage_callbacks& storage, std::size_t size) noexcept : m_storage(storage)
      {
        if ((storage.allocate != nullptr) && (size != 0U))
        {
          const auto block = storage.allocate(storage.context, size, alignof(char));
          if (block.has_value())
          {
            m_data = static_cast<char*>(*block.value_if());
            m_size = size;
          }
        }
      }

      host_buffer(const host_buffer&) = delete;
      host_buffer& operator=(const host_buffer&) = delete;
      host_buffer(host_buffer&&) = delete;
      host_buffer& operator=(host_buffer&&) = delete;

      /**
       * @brief Release the buffer.
       * @par Requirements
       * P056; J5-046.
       * @par ID SWU-CLI-016
       */
      ~host_buffer() noexcept
      {
        if ((m_data != nullptr) && (m_storage.deallocate != nullptr))
        {
          m_storage.deallocate(m_storage.context, m_data, m_size, alignof(char));
        }
      }

      /**
       * @brief Return the buffer, or nullptr when allocation failed.
       * @par Requirements
       * P056.
       * @par ID SWU-CLI-017
       */
      [[nodiscard]] char* data() const noexcept
      {
        return m_data;
      }

      /**
       * @brief Return the buffer size (0 when allocation failed).
       * @par Requirements
       * P057.
       * @par ID SWU-CLI-043
       */
      [[nodiscard]] std::size_t size() const noexcept
      {
        return m_size;
      }
  };
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_HOST_BUFFER_HPP_ */
