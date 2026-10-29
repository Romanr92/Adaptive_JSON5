#include "adaptive_json5/hosted/parse.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "memory/json5_document_access.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Allocate aligned heap storage without throwing.
     * @par Requirements
     * J5-046.
     * @return Storage, or nullptr when the heap cannot supply it.
     */
    void* hosted_allocate(void* /* context */, std::size_t bytes, std::size_t alignment) noexcept
    {
      return ::operator new(bytes, std::align_val_t{alignment}, std::nothrow);
    }

    /**
     * @brief Release storage obtained from hosted_allocate.
     * @par Requirements
     * J5-040.
     */
    void hosted_deallocate(void* /* context */, void* address, std::size_t /* bytes */, std::size_t alignment) noexcept
    {
      ::operator delete(address, std::align_val_t{alignment});
    }

    // Stateless, so every call can share it without any global mutable state (J5-050).
    constexpr storage_callbacks c_hosted_provider{nullptr, &hosted_allocate, &hosted_deallocate, nullptr};

    /**
     * @brief Heap block holding one document's storage accounting at a stable address.
     * @par Requirements
     * J5-040; J5-046.
     */
    struct hosted_storage_block
    {
        accounted_storage accounting;

        explicit hosted_storage_block(std::size_t max_owned_bytes) noexcept
            : accounting(c_hosted_provider, max_owned_bytes)
        {
        }
    };

    /**
     * @brief Destroy a hosted_storage_block; the release operation of its storage owner.
     * @par Requirements
     * J5-040; J5-051.
     * @param block A block created by hosted::parse.
     */
    void release_hosted_block(void* block) noexcept
    {
      delete static_cast<hosted_storage_block*>(block);
    }
  } // namespace

  namespace hosted
  {
    /**
     * @brief Parse a complete document with heap storage and default or given options.
     * @par Requirements
     * P037; J5-026; J5-040; J5-046; J5-047; J5-051.
     */
    result<json5_document> parse(input_view input, const parse_options& options) noexcept
    {
      // The accounting must outlive the document, so it lives in a heap block the document owns.
      auto* const block = new (std::nothrow) hosted_storage_block(options.limits.max_owned_bytes);

      if (block == nullptr)
      {
        return result<json5_document>::failure(error::without_location(error_code::allocation_failure));
      }

      detail::document_storage_owner storage{block, &block->accounting, &release_hosted_block};

      return detail::json5_document_access::parse_with_storage(input, options, std::move(storage));
    }
  } // namespace hosted
} // namespace adaptive_json5
