#include "adaptive_json5/hosted/parse.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/retained_source.hpp"
#include "adaptive_json5/value/warning_list.hpp"

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
  } // namespace

  namespace detail
  {
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

    hosted_storage_owner::hosted_storage_owner(hosted_storage_block* block) noexcept : m_block(block) {}

    hosted_storage_owner::~hosted_storage_owner() noexcept
    {
      delete m_block;
    }

    hosted_storage_owner::hosted_storage_owner(hosted_storage_owner&& other) noexcept : m_block(other.m_block)
    {
      other.m_block = nullptr;
    }

    hosted_storage_block* hosted_storage_owner::get() const noexcept
    {
      return m_block;
    }
  } // namespace detail

  hosted_document::hosted_document(detail::hosted_storage_owner&& storage, parsed_document&& parsed) noexcept
      : m_storage(std::move(storage)), m_parsed(std::move(parsed))
  {
  }

  const document& hosted_document::values() const noexcept
  {
    return m_parsed.values;
  }

  const warning_list& hosted_document::warnings() const noexcept
  {
    return m_parsed.warnings;
  }

  const retained_source& hosted_document::source() const noexcept
  {
    return m_parsed.source;
  }

  std::size_t hosted_document::current_bytes() const noexcept
  {
    return (m_storage.get() != nullptr) ? m_storage.get()->accounting.current_bytes() : 0;
  }

  std::size_t hosted_document::peak_bytes() const noexcept
  {
    return (m_storage.get() != nullptr) ? m_storage.get()->accounting.peak_bytes() : 0;
  }

  result<hosted_document> parse(input_view input, const parse_options& options) noexcept
  {
    using hosted_result = result<hosted_document>;

    // The accounting must outlive the document, so it lives in a heap block the result owns.
    detail::hosted_storage_owner storage{new (std::nothrow)
                                           detail::hosted_storage_block(options.limits.max_owned_bytes)};

    if (storage.get() == nullptr)
    {
      return hosted_result::failure(error::without_location(error_code::allocation_failure));
    }

    auto parsed = parse_document(input, options, storage.get()->accounting);

    // On failure the parser already released its allocations; storage then frees the block.
    if (!parsed.has_value())
    {
      return hosted_result::failure(*parsed.error_if());
    }

    return hosted_result::success(hosted_document{std::move(storage), std::move(*parsed.value_if())});
  }
} // namespace adaptive_json5
