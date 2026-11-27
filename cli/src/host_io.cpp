#include "cli/host_io.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "cli/cli_io.hpp"

#include <array>
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
    // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-types-shall-not-be-used
    // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles
    // use bounded snprintf formatting with the return length checked.
    // codeql::misra_deviation_next_line(DEV-207.4)
    io_status read_stream(std::FILE* stream, char* buffer, std::size_t capacity, std::size_t& size) noexcept
    {
      // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
      // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-functions-shall-not-be-used
      // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles
      // use bounded snprintf formatting with the return length checked. The constant, index or code unit is
      // representable in the destination. Standard stream macros expand to host runtime expressions.
      // codeql::misra_deviation_next_line(DEV-205.5)
      size = std::fread(buffer, 1U, capacity, stream);
      // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-functions-shall-not-be-used
      // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles
      // use bounded snprintf formatting with the return length checked.
      // codeql::misra_deviation_next_line(DEV-207.3)
      if (std::ferror(stream) != 0)
      {
        return io_status::read_error;
      }
      if (size == capacity)
      {
        char extra = '\0';
        // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
        // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-functions-shall-not-be-used
        // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite
        // doubles use bounded snprintf formatting with the return length checked. The constant, index or code unit is
        // representable in the destination. Standard stream macros expand to host runtime expressions.
        // codeql::misra_deviation_next_line(DEV-205.4)
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
        // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
        // MISRA justification: The constant, index or code unit is representable in the destination. Standard stream
        // macros expand to host runtime expressions.
        // codeql::misra_deviation_next_line(DEV-205.3)
        return read_stream(stdin, buffer, capacity, size);
      }
      if (path.size() >= c_max_path)
      {
        return io_status::not_found;
      }
      std::array<char, c_max_path> name{};
      std::size_t index = 0U;
      while (index < path.size())
      {
        name[index] = path[index];
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Indices, offsets and counters follow the enclosing sequence or storage bounds. Decimal
        // accumulation uses a digit bound or an explicit range check.
        // codeql::misra_deviation_next_line(DEV-201.7)
        ++index;
      }
      // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-types-shall-not-be-used
      // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-functions-shall-not-be-used
      // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles
      // use bounded snprintf formatting with the return length checked.
      // codeql::misra_deviation_next_line(DEV-207.2)
      std::FILE* file = std::fopen(name.data(), "rb");
      if (file == nullptr)
      {
        return io_status::not_found;
      }
      const io_status status = read_stream(file, buffer, capacity, size);
      // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-functions-shall-not-be-used
      // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles
      // use bounded snprintf formatting with the return length checked.
      // codeql::misra_deviation_next_line(DEV-207.1)
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
      // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
      // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-types-shall-not-be-used
      // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles
      // use bounded snprintf formatting with the return length checked. The constant, index or code unit is
      // representable in the destination. Standard stream macros expand to host runtime expressions.
      // codeql::misra_deviation_next_line(DEV-205.2)
      std::FILE* target = (stream == output_stream::out) ? stdout : stderr;
      // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
      // MISRA rule: RULE-30-0-1 cpp/misra/cstdio-functions-shall-not-be-used
      // MISRA justification: The optional host adapter uses standard C streams for byte-oriented I/O. Finite doubles
      // use bounded snprintf formatting with the return length checked. The constant, index or code unit is
      // representable in the destination. Standard stream macros expand to host runtime expressions.
      // codeql::misra_deviation_next_line(DEV-205.1)
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
      // MISRA rule: RULE-21-6-2 cpp/misra/dynamic-memory-managed-manually
      // MISRA rule: RULE-21-6-1 cpp/misra/dynamic-memory-should-not-be-used
      // MISRA justification: The host storage callbacks pair malloc and free. Allocation failure is returned to the
      // caller.
      // codeql::misra_deviation_next_line(DEV-208.2)
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
      // MISRA rule: RULE-21-6-2 cpp/misra/dynamic-memory-managed-manually
      // MISRA rule: RULE-21-6-1 cpp/misra/dynamic-memory-should-not-be-used
      // MISRA rule: RULE-0-1-2 cpp/misra/unused-return-value-misra-cpp
      // MISRA justification: The host storage callbacks pair malloc and free. Allocation failure is returned to the
      // caller. The host free function returns void. A byte-copy destination is already known to the caller.
      // codeql::misra_deviation_next_line(DEV-208.1)
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
