#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_POD_VECTOR_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_POD_VECTOR_HPP_

#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"

#include <cstddef>
#include <cstring>
#include <optional>
#include <type_traits>
#include <utility>

namespace adaptive_json5::detail
{
  /**
   * @brief Return the capacity after growing from current, or nullopt when doubling would overflow.
   * @par Requirements
   * P044; J5-046.
   * @return 8 for an empty vector, otherwise twice current; nullopt when the doubled element
   * count times element_size would not fit in std::size_t.
   * @note Non-template so that the overflow check exists once and is unit-tested directly.
   * @par ID SWU-SCHEMA-189
   */
  [[nodiscard]] std::optional<std::size_t> next_capacity(std::size_t current, std::size_t element_size) noexcept;

  /**
   * @brief Copy bytes between two blocks; does nothing for zero bytes (null blocks allowed then).
   * @par Requirements
   * P044; J5-046.
   * @par ID SWU-SCHEMA-190
   */
  void copy_bytes(void* destination, const void* source, std::size_t bytes) noexcept;

  /**
   * @brief Growable array of trivially copyable elements in caller storage.
   * @par Requirements
   * P044; J5-046; J5-065; J5-066.
   * @note Grows by doubling through allocate_array(); the old block is released after the
   * copy. Elements are never destroyed individually. Not thread-safe.
   */
  template <typename T>
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA rule: RULE-4-1-2 cpp/misra/implicit-declaration-of-copy-constructor-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.1)
  class [[nodiscard]] pod_vector
  {
      static_assert(std::is_trivially_copyable_v<T>, "pod_vector holds trivially copyable elements");

    private:
      storage_callbacks m_storage{};
      owned_block m_block{};
      std::size_t m_size{};
      std::size_t m_capacity{};

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
        const auto capacity = next_capacity(m_capacity, sizeof(T));
        if (!capacity.has_value())
        {
          return result<void, schema_error>::failure(schema_error::of(schema_error_code::allocation_failure));
        }
        auto block = allocate_array(m_storage, *capacity, sizeof(T), alignof(T));
        if (!block.has_value())
        {
          return result<void, schema_error>::failure(schema_error::from_storage(*block.error_if()));
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: m_size elements of sizeof(T) bytes were allocated before, so the product is a block
        // size that already fit in std::size_t.
        // codeql::misra_deviation_next_line(DEV-101.6)
        copy_bytes(block.value_if()->data(), m_block.data(), m_size * sizeof(T));
        // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
        // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
        // once.
        // codeql::misra_deviation_next_line(DEV-108.1)
        m_block = std::move(*block.value_if());
        m_capacity = *capacity;
        return result<void, schema_error>::success();
      }

    public:
      /**
       * @brief Create an empty vector; no allocation.
       * @par Requirements
       * P044; J5-046.
       * @par ID SWU-SCHEMA-038
       */
      // MISRA rule: RULE-15-1-4 cpp/misra/non-static-member-not-init-before-use
      // MISRA justification: Move construction and accessors are noexcept and leave the source empty; members have
      // default member initializers, so none is read before it is set.
      // codeql::misra_deviation_next_line(DEV-109.2)
      explicit pod_vector(const storage_callbacks& storage) noexcept : m_storage(storage) {}

      /**
       * @brief Take over another vector's elements; other becomes empty.
       * @par Requirements
       * P044; J5-046.
       * @par ID SWU-SCHEMA-163
       */
      // MISRA rule: RULE-18-4-1 cpp/misra/exception-unfriendly-function-must-be-noexcept
      // MISRA rule: RULE-15-0-2 cpp/misra/invalid-signature-for-special-member-function
      // MISRA justification: Move construction and accessors are noexcept and leave the source empty; members have
      // default member initializers, so none is read before it is set.
      // codeql::misra_deviation_next_line(DEV-109.1)
      pod_vector(pod_vector&& other) noexcept
          : m_storage(other.m_storage), m_block(std::move(other.m_block)), m_size(other.m_size),
            m_capacity(other.m_capacity)
      {
        other.m_size = 0U;
        other.m_capacity = 0U;
      }

      pod_vector& operator=(pod_vector&&) = delete;
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
        // MISRA rule: RULE-7-11-3 cpp/misra/function-pointer-conversion-context
        // MISRA justification: Address computation inside an owned block from caller storage, as in the parser
        // storage layer (DEV-002/DEV-003); the offset is within the block size that was allocated.
        // codeql::misra_deviation_next_line(DEV-107.2)
        const auto grown = grow();
        if (!grown.has_value())
        {
          return grown;
        }
        static_cast<void>(
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
          // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
          // MISRA rule: RULE-8-7-1 cpp/misra/pointer-argument-to-cstring-function-is-invalid
          // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check,
          // the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
          // Address computation inside an owned block from caller storage, as in the parser storage layer
          // (DEV-002/DEV-003); the offset is within the block size that was allocated. pod_vector copies trivially
          // copyable elements (static_assert) within allocated blocks whose sizes are element counts times sizeof(T),
          // as in the parser (DEV-023).
          // codeql::misra_deviation_next_line(DEV-101.5)
          std::memcpy(static_cast<unsigned char*>(m_block.data()) + (m_size * sizeof(T)), &value, sizeof(T)));
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.4)
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
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
          // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
          // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check,
          // the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
          // Address computation inside an owned block from caller storage, as in the parser storage layer
          // (DEV-002/DEV-003); the offset is within the block size that was allocated. pod_vector copies trivially
          // copyable elements (static_assert) within allocated blocks whose sizes are element counts times sizeof(T),
          // as in the parser (DEV-023).
          // codeql::misra_deviation_next_line(DEV-101.3)
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
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
          // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
          // MISRA rule: RULE-8-7-1 cpp/misra/pointer-argument-to-cstring-function-is-invalid
          // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check,
          // the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
          // Address computation inside an owned block from caller storage, as in the parser storage layer
          // (DEV-002/DEV-003); the offset is within the block size that was allocated. pod_vector copies trivially
          // copyable elements (static_assert) within allocated blocks whose sizes are element counts times sizeof(T),
          // as in the parser (DEV-023).
          // codeql::misra_deviation_next_line(DEV-101.2)
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
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.1)
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
        // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
        // MISRA justification: Address computation inside an owned block from caller storage, as in the parser
        // storage layer (DEV-002/DEV-003); the offset is within the block size that was allocated.
        // codeql::misra_deviation_next_line(DEV-107.1)
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
