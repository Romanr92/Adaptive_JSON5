#include "memory/buffer_document_storage.hpp"

#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/buffer_arena.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/result.hpp"
#include "memory/pointer_alignment.hpp"

#include <cstddef>
#include <new>

namespace adaptive_json5
{
  namespace detail
  {
    namespace
    {
      /**
       * @brief Keep a document's provider and accounting inside its caller buffer.
       * @par Requirements
       * P003; P037; J5-040; J5-046; J5-047; J5-101.
       * @note The arena precedes accounting so accounting is destroyed first.
       * Neither object may move while document allocations exist.
       */
      struct buffer_document_storage
      {
          buffer_arena arena;
          accounted_storage accounting;

          /**
           * @brief Initialize allocation space and its requested-byte ceiling.
           * @par Requirements
           * J5-046; J5-047.
           * @note Construction performs no allocation.
           */
          buffer_document_storage(void* buffer, std::size_t size, std::size_t max_owned_bytes) noexcept
              : arena(buffer, size), accounting(arena.callbacks(), max_owned_bytes)
          {
          }
      };

      /**
       * @brief Destroy the control block without freeing the caller's buffer.
       * @par Requirements
       * J5-040; J5-051; J5-101.
       * @note Every document allocation must already have been released.
       */
      void release_buffer_document_storage(void* block) noexcept
      {
        static_cast<buffer_document_storage*>(block)->~buffer_document_storage();
      }
    } // namespace

    /**
     * @brief Place a stable storage control block inside the supplied buffer.
     * @par Requirements
     * P003; P037; J5-040; J5-046; J5-047; J5-051; J5-101.
     */
    result<document_storage_owner> make_buffer_document_storage(void* buffer, std::size_t size,
                                                                std::size_t max_owned_bytes) noexcept
    {
      using storage_result = result<document_storage_owner>;

      if (buffer == nullptr)
      {
        return storage_result::failure(error::without_location(error_code::allocation_failure));
      }

      void* position = buffer;
      std::size_t space = size;

      // Reserve aligned control storage before constructing objects, in the buffer.
      void* aligned =
        detail::align_within(alignof(buffer_document_storage), sizeof(buffer_document_storage), position, space);

      if (aligned == nullptr)
      {
        return storage_result::failure(error::without_location(error_code::allocation_failure));
      }

      // std::align removes leading padding from space, but not the object's size.
      auto* allocation_begin = static_cast<unsigned char*>(aligned) + sizeof(buffer_document_storage);
      const std::size_t allocation_size = space - sizeof(buffer_document_storage);

      // Placement construction uses caller storage; it does not allocate from the heap.
      auto* block = ::new (aligned) buffer_document_storage(allocation_begin, allocation_size, max_owned_bytes);

      return storage_result::success(
        document_storage_owner{block, &block->accounting, &release_buffer_document_storage});
    }
  } // namespace detail
} // namespace adaptive_json5
