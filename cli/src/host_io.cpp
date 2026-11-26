#include "cli/host_io.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "cli/cli_io.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace adaptive_json5::cli
{
  namespace
  {
    constexpr std::size_t c_max_path = 4096U;

    /**
     * @brief Read all of stream into buffer; too_large when more than capacity bytes remain.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-024
     */
    io_status read_stream(std::FILE* stream, char* buffer, std::size_t capacity, std::size_t& size) noexcept
    {
      size = std::fread(buffer, 1U, capacity, stream);
      if (std::ferror(stream) != 0)
      {
        return io_status::read_error;
      }
      if (size == capacity)
      {
        char extra = '\0';
        if (std::fread(&extra, 1U, 1U, stream) == 1U)
        {
          return io_status::too_large;
        }
      }
      return io_status::ok;
    }

    /**
     * @brief Read a file, or standard input for "-".
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-025
     */
    io_status read_input(void* /* context */, std::string_view path, char* buffer, std::size_t capacity,
                         std::size_t& size) noexcept
    {
      size = 0U;
      if (path == "-")
      {
        return read_stream(stdin, buffer, capacity, size);
      }
      if (path.size() >= c_max_path)
      {
        return io_status::not_found;
      }
      char name[c_max_path] = {};
      for (std::size_t index = 0U; index < path.size(); ++index)
      {
        name[index] = path[index];
      }
      std::FILE* file = std::fopen(name, "rb");
      if (file == nullptr)
      {
        return io_status::not_found;
      }
      const io_status status = read_stream(file, buffer, capacity, size);
      static_cast<void>(std::fclose(file));
      return status;
    }

    /**
     * @brief Write text to standard output or standard error.
     * @par Requirements
     * P056; J5-071.
     * @par ID SWU-CLI-026
     */
    bool write_output(void* /* context */, output_stream stream, std::string_view text) noexcept
    {
      std::FILE* target = (stream == output_stream::out) ? stdout : stderr;
      return std::fwrite(text.data(), 1U, text.size(), target) == text.size();
    }

    /**
     * @brief malloc-backed allocation (host tool; ADR 0009).
     * @par Requirements
     * P056; J5-046.
     * @par ID SWU-CLI-027
     */
    result<void*> allocate(void* /* context */, std::size_t bytes, std::size_t /* alignment */) noexcept
    {
      void* address = std::malloc(bytes);
      return (address == nullptr) ? result<void*>::failure(error::without_location(error_code::allocation_failure))
                                  : result<void*>::success(address);
    }

    /**
     * @brief Release malloc-backed storage.
     * @par Requirements
     * P056; J5-046.
     * @par ID SWU-CLI-028
     */
    void deallocate(void* /* context */, void* address, std::size_t /* bytes */, std::size_t /* alignment */) noexcept
    {
      std::free(address);
    }
  } // namespace

  /**
   * @brief The real host environment.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-023
   */
  environment host_environment() noexcept
  {
    environment host{};
    host.input = input_source{nullptr, &read_input};
    host.output = output_sink{nullptr, &write_output};
    host.storage = storage_callbacks{nullptr, &allocate, &deallocate};
    return host;
  }
} // namespace adaptive_json5::cli
