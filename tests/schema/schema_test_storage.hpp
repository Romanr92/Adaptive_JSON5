#ifndef ADAPTIVE_JSON5_TEST_SCHEMA_TEST_STORAGE_HPP_
#define ADAPTIVE_JSON5_TEST_SCHEMA_TEST_STORAGE_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>

namespace adaptive_json5_test
{
  /**
   * @brief Heap test storage with counters and one-shot failure injection.
   * @par Requirements
   * P044; P046; J5-046; J5-051; J5-065.
   * @note Only test code uses malloc. fail_on counts allocation attempts from 1; 0 never fails.
   * The destructor expects every allocation to have been released.
   */
  class schema_test_storage
  {
    public:
      std::size_t attempts{};
      std::size_t fail_on{};
      std::size_t allocations{};
      std::size_t releases{};
      std::size_t live_bytes{};
      std::size_t peak_bytes{};

      schema_test_storage() = default;
      schema_test_storage(const schema_test_storage&) = delete;
      schema_test_storage& operator=(const schema_test_storage&) = delete;

      ~schema_test_storage()
      {
        EXPECT_EQ(live_bytes, 0U);
        EXPECT_EQ(allocations, releases);
      }

      adaptive_json5::storage_callbacks callbacks() noexcept
      {
        return adaptive_json5::storage_callbacks{this, &allocate, &release};
      }

    private:
      static adaptive_json5::result<void*> allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        using adaptive_json5::error;
        using adaptive_json5::error_code;
        auto& self = *static_cast<schema_test_storage*>(context);
        ++self.attempts;
        if ((self.attempts == self.fail_on) || (alignment > alignof(std::max_align_t)))
        {
          return adaptive_json5::result<void*>::failure(error::without_location(error_code::allocation_failure));
        }
        void* const address = std::malloc(bytes);
        if (address == nullptr)
        {
          return adaptive_json5::result<void*>::failure(error::without_location(error_code::allocation_failure));
        }
        ++self.allocations;
        self.live_bytes += bytes;
        self.peak_bytes = (self.live_bytes > self.peak_bytes) ? self.live_bytes : self.peak_bytes;
        return adaptive_json5::result<void*>::success(address);
      }

      static void release(void* context, void* address, std::size_t bytes, std::size_t /* alignment */) noexcept
      {
        auto& self = *static_cast<schema_test_storage*>(context);
        ++self.releases;
        self.live_bytes -= bytes;
        std::free(address);
      }
  };
} // namespace adaptive_json5_test

#endif /* ADAPTIVE_JSON5_TEST_SCHEMA_TEST_STORAGE_HPP_ */
