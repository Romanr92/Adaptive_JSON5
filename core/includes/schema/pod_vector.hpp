#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_POD_VECTOR_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_POD_VECTOR_HPP_

#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace adaptive_json5::detail
{
  /**
   * @brief Growable array of trivially copyable elements in caller storage.
   * @par Requirements
   * P044; J5-046; J5-065; J5-066.
   * @note Grows by doubling through allocate_array(); the old block is released after the
   * copy. Elements are never destroyed individually. Not thread-safe.
   */
  template <typename T>
  class [[nodiscard]] pod_vector
  {
      static_assert(std::is_trivially_copyable_v<T>, "pod_vector holds trivially copyable elements");

    private:
      storage_callbacks m_storage{};
      owned_block m_block{};
      std::size_t m_size{};
      std::size_t m_capacity{};

      static constexpr std::size_t c_first_capacity = 8U;

      /**
       * @brief Ensure room for one more element.
       * @par Requirements
       * P044; J5-046.
       * @par ID SWU-SCHEMA-037
       */
      [[nodiscard]] result<void, schema_error> grow() noexcept
      {
        if (m_size < m_capacity)
        {
          return result<void, schema_error>::success();
        }
        const std::size_t limit = std::numeric_limits<std::size_t>::max() / (2U * sizeof(T));
        if (m_capacity > limit)
        {
          return result<void, schema_error>::failure(schema_error::of(schema_error_code::allocation_failure));
        }
        const std::size_t capacity = (m_capacity == 0U) ? c_first_capacity : (m_capacity * 2U);
        auto block = allocate_array(m_storage, capacity, sizeof(T), alignof(T));
        if (!block.has_value())
        {
          return result<void, schema_error>::failure(schema_error::from_storage(*block.error_if()));
        }
        if (m_size != 0U)
        {
          static_cast<void>(std::memcpy(block.value_if()->data(), m_block.data(), m_size * sizeof(T)));
        }
        m_block = std::move(*block.value_if());
        m_capacity = capacity;
        return result<void, schema_error>::success();
      }

    public:
      /**
       * @brief Create an empty vector; no allocation.
       * @par Requirements
       * P044; J5-046.
       * @par ID SWU-SCHEMA-038
       */
      explicit pod_vector(const storage_callbacks& storage) noexcept : m_storage(storage) {}

      pod_vector(pod_vector&&) noexcept = default;
      pod_vector& operator=(pod_vector&&) noexcept = default;
      pod_vector(const pod_vector&) = delete;
      pod_vector& operator=(const pod_vector&) = delete;
      ~pod_vector() noexcept = default;

      /**
       * @brief Append a copy of value.
       * @par Requirements
       * P044; J5-046.
       * @return success, or a storage failure; failure leaves the vector unchanged.
       * @par ID SWU-SCHEMA-039
       */
      [[nodiscard]] result<void, schema_error> push_back(const T& value) noexcept
      {
        const auto grown = grow();
        if (!grown.has_value())
        {
          return grown;
        }
        static_cast<void>(
          std::memcpy(static_cast<unsigned char*>(m_block.data()) + (m_size * sizeof(T)), &value, sizeof(T)));
        ++m_size;
        return result<void, schema_error>::success();
      }

      /**
       * @brief Return a copy of the element at index; requires index < size().
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-040
       */
      [[nodiscard]] T get(std::size_t index) const noexcept
      {
        T value{};
        static_cast<void>(
          std::memcpy(&value, static_cast<const unsigned char*>(m_block.data()) + (index * sizeof(T)), sizeof(T)));
        return value;
      }

      /**
       * @brief Replace the element at index; requires index < size().
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-041
       */
      void set(std::size_t index, const T& value) noexcept
      {
        static_cast<void>(
          std::memcpy(static_cast<unsigned char*>(m_block.data()) + (index * sizeof(T)), &value, sizeof(T)));
      }

      /**
       * @brief Remove the last element; requires a nonempty vector.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-042
       */
      void pop_back() noexcept
      {
        --m_size;
      }

      /**
       * @brief Shrink to count elements; requires count <= size().
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-043
       */
      void truncate(std::size_t count) noexcept
      {
        m_size = count;
      }

      /**
       * @brief Return the number of elements.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-044
       */
      [[nodiscard]] std::size_t size() const noexcept
      {
        return m_size;
      }

      /**
       * @brief Borrow the element storage, or nullptr while nothing was allocated.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-045
       */
      [[nodiscard]] const void* data() const noexcept
      {
        return m_block.data();
      }

      /**
       * @brief Borrow the element storage for writing, or nullptr while nothing was allocated.
       * @par Requirements
       * P044.
       * @note Pointers are invalidated by the next growth.
       * @par ID SWU-SCHEMA-088
       */
      [[nodiscard]] T* writable() noexcept
      {
        return static_cast<T*>(m_block.data());
      }

      /**
       * @brief Hand the element block to a new owner; the vector becomes empty.
       * @par Requirements
       * P044; J5-066.
       * @par ID SWU-SCHEMA-046
       */
      [[nodiscard]] owned_block release() noexcept
      {
        m_size = 0U;
        m_capacity = 0U;
        return std::move(m_block);
      }
  };
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_POD_VECTOR_HPP_ */
