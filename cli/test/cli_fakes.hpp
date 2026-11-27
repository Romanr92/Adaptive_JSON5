#ifndef ADAPTIVE_JSON5_CLI_TEST_CLI_FAKES_HPP_
#define ADAPTIVE_JSON5_CLI_TEST_CLI_FAKES_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "cli/cli_app.hpp"
#include "cli/cli_io.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace adaptive_json5_test
{
  /**
   * @brief In-memory host for CLI units: named inputs, captured output, counting storage.
   * @par Requirements
   * P056; P057; P058; J5-070; J5-071.
   * @note "-" names standard input. read_status forces an I/O outcome for one path.
   * fail_on makes allocation n fail (1-based; 0 never). The destructor checks that everything
   * allocated was released.
   */
  class fake_host
  {
    public:
      std::map<std::string, std::string> files;
      std::map<std::string, adaptive_json5::cli::io_status> read_status;
      std::string out;
      std::string err;
      bool fail_writes{};
      std::size_t fail_on{};
      std::size_t attempts{};
      std::size_t live{};

      fake_host() = default;
      fake_host(const fake_host&) = delete;
      fake_host& operator=(const fake_host&) = delete;

      ~fake_host()
      {
        EXPECT_EQ(live, 0U);
      }

      adaptive_json5::cli::environment environment()
      {
        adaptive_json5::cli::environment host{};
        host.input = adaptive_json5::cli::input_source{this, &read};
        host.output = adaptive_json5::cli::output_sink{this, &write};
        host.storage = adaptive_json5::storage_callbacks{this, &allocate, &release};
        return host;
      }

      /**
       * @brief Run the tool with arguments (the program name is added) and return the exit code.
       */
      int run(std::vector<std::string> arguments)
      {
        arguments.insert(arguments.begin(), "adaptive_json5_examples");
        std::vector<const char*> argv;
        for (const std::string& argument : arguments)
        {
          argv.push_back(argument.c_str());
        }
        return adaptive_json5::cli::run_cli(static_cast<int>(argv.size()), argv.data(), environment());
      }

    private:
      static adaptive_json5::cli::io_status read(void* context, std::string_view path, char* buffer,
                                                 std::size_t capacity, std::size_t& size) noexcept
      {
        auto& self = *static_cast<fake_host*>(context);
        size = 0U;
        const std::string key{path};
        const auto forced = self.read_status.find(key);
        if (forced != self.read_status.end())
        {
          return forced->second;
        }
        const auto file = self.files.find(key);
        if (file == self.files.end())
        {
          return adaptive_json5::cli::io_status::not_found;
        }
        if (file->second.size() > capacity)
        {
          return adaptive_json5::cli::io_status::too_large;
        }
        std::memcpy(buffer, file->second.data(), file->second.size());
        size = file->second.size();
        return adaptive_json5::cli::io_status::ok;
      }

      static bool write(void* context, adaptive_json5::cli::output_stream stream, std::string_view text) noexcept
      {
        auto& self = *static_cast<fake_host*>(context);
        if (self.fail_writes)
        {
          return false;
        }
        (stream == adaptive_json5::cli::output_stream::out ? self.out : self.err).append(text.data(), text.size());
        return true;
      }

      static adaptive_json5::result<void*> allocate(void* context, std::size_t bytes, std::size_t) noexcept
      {
        auto& self = *static_cast<fake_host*>(context);
        ++self.attempts;
        if (self.attempts == self.fail_on)
        {
          return adaptive_json5::result<void*>::failure(
            adaptive_json5::error::without_location(adaptive_json5::error_code::allocation_failure));
        }
        void* address = std::malloc(bytes);
        if (address == nullptr)
        {
          return adaptive_json5::result<void*>::failure(
            adaptive_json5::error::without_location(adaptive_json5::error_code::allocation_failure));
        }
        self.live += bytes;
        return adaptive_json5::result<void*>::success(address);
      }

      static void release(void* context, void* address, std::size_t bytes, std::size_t) noexcept
      {
        auto& self = *static_cast<fake_host*>(context);
        self.live -= bytes;
        std::free(address);
      }
  };
} // namespace adaptive_json5_test

#endif /* ADAPTIVE_JSON5_CLI_TEST_CLI_FAKES_HPP_ */
