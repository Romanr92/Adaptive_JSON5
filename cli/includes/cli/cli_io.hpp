#ifndef ADAPTIVE_JSON5_CLI_CLI_IO_HPP_
#define ADAPTIVE_JSON5_CLI_CLI_IO_HPP_

#include "adaptive_json5/memory/storage.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace adaptive_json5::cli
{
  /**
   * @brief Outcome of reading one input.
   * @par Requirements
   * P056; J5-070.
   */
  enum class io_status : std::uint8_t
  {
    ok,
    not_found,  // The file does not exist or cannot be opened.
    read_error, // Opening succeeded but reading failed.
    too_large   // The input has more bytes than the buffer holds.
  };

  /**
   * @brief Injectable bounded reader for files and standard input ("-").
   * @par Requirements
   * P056; J5-070; ADR 0007.
   * @note read fills buffer with at most capacity bytes and sets size; when the input has more
   * bytes it returns too_large. The real adapter is in host_io.cpp; units use fakes.
   */
  struct input_source
  {
      void* context = nullptr;
      io_status (*read)(void* context, std::string_view path, char* buffer, std::size_t capacity,
                        std::size_t& size) noexcept = nullptr;
  };

  /**
   * @brief Output stream of the tool.
   * @par Requirements
   * P056; J5-070.
   */
  enum class output_stream : std::uint8_t
  {
    out, // Results and machine diagnostics.
    err  // Usage errors and readable failure messages.
  };

  /**
   * @brief Injectable output sink (stdout and stderr in the real tool).
   * @par Requirements
   * P056; J5-071.
   * @note write returns false when the text could not be written.
   */
  struct output_sink
  {
      void* context = nullptr;
      bool (*write)(void* context, output_stream stream, std::string_view text) noexcept = nullptr;
  };

  /**
   * @brief Everything the tool needs from its host: input, output and memory.
   * @par Requirements
   * P056; J5-070; ADR 0007.
   */
  struct environment
  {
      input_source input{};
      output_sink output{};
      storage_callbacks storage{};
  };
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_CLI_IO_HPP_ */
