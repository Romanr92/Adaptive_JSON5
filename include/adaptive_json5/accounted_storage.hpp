#ifndef ACCOUNTED_STORAGE_HPP_
#define ACCOUNTED_STORAGE_HPP_

#include "checked_size.hpp"
#include "error.hpp"
#include "owned_block.hpp"
#include "result.hpp"
#include "storage.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Enforce an outstanding-byte ceiling over caller-provided storage.
   * @par Requirements
   * P031; J5-040; J5-046; J5-047; J5-051.
   * @note Counts requested allocation bytes, including unused capacity.
   * Provider-internal overhead is excluded.
   * This object and its provider state must outlive every returned block.
   * Its address must remain stable while any returned block exists.
   * Operations require external synchronization if shared between threads.
   * Provider callbacks must not reenter this accounting object.
   * Construction performs no allocation.
   */
  class accounted_storage
  {
    private:
      storage_callbacks m_provider;
      std::size_t m_limit;
      std::size_t m_current_bytes{};
      std::size_t m_peak_bytes{};

      /**
       * @brief Forward an approved allocation and account for success.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note Called only by allocate() after provider and byte-limit checks.
       * Failure leaves current and peak accounting unchanged.
       */
      static void* allocate_callback(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<accounted_storage*>(context);

        void* const address = self.m_provider.allocate(self.m_provider.context, bytes, alignment);

        if (address == nullptr)
        {
          return nullptr;
        }

        // The public entry point proved this addition fits the byte ceiling.
        self.m_current_bytes += bytes;

        if (self.m_current_bytes > self.m_peak_bytes)
        {
          self.m_peak_bytes = self.m_current_bytes;
        }

        return address;
      }

      /**
       * @brief Release one owned allocation and remove its accounted bytes.
       * @par Requirements
       * P031; J5-040; J5-046; J5-051.
       * @note owned_block supplies the original allocation description once.
       * The originating accounting object and provider must still be alive.
       */
      static void deallocate_callback(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<accounted_storage*>(context);

        self.m_provider.deallocate(self.m_provider.context, address, bytes, alignment);

        // Only the matching owned block can invoke this private adapter.
        self.m_current_bytes -= bytes;
      }

    public:
      /**
       * @brief Borrow a provider and set its outstanding-byte ceiling.
       * @par Requirements
       * P031; J5-040; J5-047.
       * @param provider Caller storage whose state outlives returned blocks.
       * @param max_owned_bytes Inclusive maximum outstanding requested bytes.
       * @note Zero permits only empty blocks.
       */
      accounted_storage(const storage_callbacks& provider, std::size_t max_owned_bytes) noexcept
          : m_provider(provider), m_limit(max_owned_bytes)
      {
      }

      /**
       * @brief End accounting after every returned allocation has been released.
       * @par Requirements
       * P031; J5-040.
       * @note Does not own or release outstanding blocks itself.
       */
      ~accounted_storage() noexcept = default;

      // Returned blocks retain this object's address as their callback context.
      accounted_storage(const accounted_storage&) = delete;
      accounted_storage& operator=(const accounted_storage&) = delete;
      accounted_storage(accounted_storage&&) = delete;
      accounted_storage& operator=(accounted_storage&&) = delete;

      /**
       * @brief Allocate a block after checking the outstanding-byte ceiling.
       * @par Requirements
       * P031; J5-040; J5-046; J5-047; J5-051.
       * @return Owned block, resource_limit or allocation_failure.
       * @note Rejects missing provider callbacks before attempting allocation.
       * Byte-limit failure never calls the provider.
       * Alignment validation follows allocate_block() semantics.
       * Zero bytes produce an empty block after provider/alignment validation.
       * Failures leave current and peak accounting unchanged.
       */
      result<owned_block> allocate(std::size_t bytes, std::size_t alignment) noexcept
      {
        if (m_provider.allocate == nullptr || m_provider.deallocate == nullptr)
        {
          return result<owned_block>::failure(error::without_location(error_code::allocation_failure));
        }

        // Subtraction checks the request without overflowing the total.
        if ((m_current_bytes > m_limit) || (bytes > (m_limit - m_current_bytes)))
        {
          return result<owned_block>::failure(error::without_location(error_code::resource_limit));
        }

        // The owner retains our adapters so destruction updates accounting.
        const storage_callbacks adapter{this, &allocate_callback, &deallocate_callback};

        return adaptive_json5::allocate_block(adapter, bytes, alignment);
      }

      /**
       * @brief Allocate an element block after checked size multiplication.
       * @par Requirements
       * P031; J5-046; J5-047.
       * @note Overflow and byte-limit failures never call the provider.
       * Raw storage is returned; element lifetimes are managed by the caller.
       */
      result<owned_block> allocate_array(std::size_t count, std::size_t element_size, std::size_t alignment) noexcept
      {
        const auto bytes = checked_multiply(count, element_size);
        if (!bytes.has_value())
        {
          return result<owned_block>::failure(*bytes.error_if());
        }

        return allocate(*bytes.value_if(), alignment);
      }

      /**
       * @brief Return currently outstanding requested allocation bytes.
       * @par Requirements
       * P031; J5-047.
       */
      std::size_t current_bytes() const noexcept
      {
        return m_current_bytes;
      }

      /**
       * @brief Return the highest outstanding-byte total reached.
       * @par Requirements
       * P031; J5-047.
       * @note Releasing blocks reduces current bytes without reducing the peak.
       */
      std::size_t peak_bytes() const noexcept
      {
        return m_peak_bytes;
      }
  };
} // namespace adaptive_json5

#endif /* ACCOUNTED_STORAGE_HPP_ */
