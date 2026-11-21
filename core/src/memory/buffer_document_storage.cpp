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
      // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
      // MISRA justification: Audit result only: the class declares no special members of the listed kind, so
      // the compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them
      // because implicit members that are never odr-used are not in its database.
      // codeql::misra_deviation_next_line(DEV-020.34)
      struct buffer_document_storage
      {
          buffer_arena arena;
          accounted_storage accounting;

          /**
           * @brief Initialize allocation space and its requested-byte ceiling.
           * @par Requirements
           * J5-046; J5-047.
           * @note Construction performs no allocation.
           * @par ID SWU-MEMORY-017
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
       * @par ID SWU-MEMORY-018
       */
      void release_buffer_document_storage(void* block) noexcept
      {
        // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
        // MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used
        // MISRA justification: End the control block's lifetime after its allocations have been released.
        // The caller owns the backing buffer and retains it after destruction.
        // Converts the untyped storage pointer to the byte or object type it was created as. This is a
        // static_cast from void*, not an integer-to-pointer conversion.
        // codeql::misra_deviation_next_line(DEV-001.02)
        static_cast<buffer_document_storage*>(block)->~buffer_document_storage();
      }
    } // namespace

    /**
     * @brief Place a stable storage control block inside the supplied buffer.
     * @par Requirements
     * P003; P037; J5-040; J5-046; J5-047; J5-051; J5-101.
     * @par ID SWU-MEMORY-019
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

      constexpr std::size_t c_control_bytes = sizeof(buffer_document_storage);

      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: std::align removes leading padding from space, but not the object's size.
      // Use a byte address to start the arena immediately after the control block.
      // The alignment check has already established room for the control block.
      // codeql::misra_deviation_next_line(DEV-002.07)
      auto* allocation_begin = static_cast<std::byte*>(aligned) + c_control_bytes;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: align_within() checked that space holds the control block, so space is at least
      // c_control_bytes.
      // codeql::misra_deviation_next_line(DEV-004.77)
      const std::size_t allocation_size = space - c_control_bytes;

      // MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used
      // MISRA justification: Construct the control block in caller storage without heap allocation (J5-101).
      // align_within() has checked the size and alignment of the storage.
      // codeql::misra_deviation_next_line(DEV-001.01)
      auto* block = ::new (aligned) buffer_document_storage(allocation_begin, allocation_size, max_owned_bytes);

      return storage_result::success(
        document_storage_owner{block, &block->accounting, &release_buffer_document_storage});
    }
  } // namespace detail
} // namespace adaptive_json5
