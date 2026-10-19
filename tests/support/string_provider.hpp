#ifndef ADAPTIVE_JSON5_TEST_STRING_PROVIDER_HPP_
#define ADAPTIVE_JSON5_TEST_STRING_PROVIDER_HPP_
// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace adaptive_json5_test
{
  using namespace adaptive_json5;
  /**
   * @brief Fixed aligned caller storage with deterministic failure and release checks.
   * @par Requirements
   * P003; P013; J5-040; J5-046; J5-051.
   * @note Each slot records its actual allocation description; no heap is used.
   */
  class string_provider
  {
    private:
      struct alignas(std::max_align_t) slot
      {
          std::array<unsigned char, 512> data{};
          std::size_t bytes{};
          std::size_t alignment{};
          bool active{};
      };
      std::array<slot, 16> m_slots{};

      /**
       * @brief Allocate a reusable aligned slot or fail at the selected request.
       * @par Requirements
       * P013; J5-046.
       */
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<string_provider*>(context);
        ++self.m_attempts;
        if (self.m_attempts == self.m_fail_on || bytes > 512 || alignment > alignof(slot))
        {
          return nullptr;
        }
        for (auto& item : self.m_slots)
        {
          if (!item.active)
          {
            item.active = true;
            item.bytes = bytes;
            item.alignment = alignment;
            ++self.m_allocations;
            self.m_live_bytes += bytes;
            if (self.m_live_bytes > self.m_peak_bytes)
            {
              self.m_peak_bytes = self.m_live_bytes;
            }
            return item.data.data();
          }
        }
        return nullptr;
      }

      /**
       * @brief Verify the original allocation description and release its slot.
       * @par Requirements
       * P003; P013; J5-040; J5-051.
       */
      static void deallocate(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<string_provider*>(context);
        for (auto& item : self.m_slots)
        {
          if (item.data.data() == address && item.active)
          {
            EXPECT_EQ(bytes, item.bytes);
            EXPECT_EQ(alignment, item.alignment);
            item.active = false;
            self.m_live_bytes -= item.bytes;
            ++self.m_releases;
            return;
          }
        }
        ADD_FAILURE() << "Unknown allocation or duplicate release";
      }

    public:
      std::size_t m_attempts{};
      std::size_t m_allocations{};
      std::size_t m_releases{};
      std::size_t m_live_bytes{};
      std::size_t m_peak_bytes{};
      std::size_t m_fail_on{};

      /**
       * @brief Verify that all owners released their allocations before the provider.
       * @par Requirements
       * P013; J5-040.
       */
      ~string_provider()
      {
        EXPECT_EQ(m_live_bytes, 0u);
        EXPECT_EQ(m_allocations, m_releases);
      }

      /**
       * @brief Borrow this provider through the production callback interface.
       * @par Requirements
       * P013; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &deallocate};
      }
  };

  /**
   * @brief Check exact stored units using the public checked accessor.
   * @par Requirements
   * P013; J5-018; J5-029.
   */
  void expect_units(const decoded_string& text, const std::u16string& expected)
  {
    ASSERT_EQ(text.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      const auto actual = text.unit_at(index);
      ASSERT_TRUE(actual.has_value()) << index;
      EXPECT_EQ(*actual.value_if(), expected[index]) << index;
    }
  }

  /**
   * @brief Check the minimal allocation-free failure representation.
   * @par Requirements
   * P013; J5-003.
   */
  template <typename T>
  void expect_error(const result<T>& actual, error_code code)
  {
    ASSERT_FALSE(actual.has_value());
    ASSERT_NE(actual.error_if(), nullptr);
    EXPECT_EQ(actual.error_if()->code, code);
    EXPECT_FALSE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 0u);
  }

} // namespace adaptive_json5_test
#endif
