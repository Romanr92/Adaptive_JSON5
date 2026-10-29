#include "adaptive_json5/memory/document_storage_owner.hpp"

#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Adopt a stable storage block and its destruction operation.
     * @par Requirements
     * J5-040; J5-046.
     */
    document_storage_owner::document_storage_owner(void* block, accounted_storage* accounting,
                                                   void (*release)(void*) noexcept) noexcept
        : m_block(block), m_accounting(accounting), m_release(release)
    {
    }

    /**
     * @brief Destroy the owned control block, if ownership remains.
     * @par Requirements
     * J5-040; J5-051.
     */
    document_storage_owner::~document_storage_owner() noexcept
    {
      if (m_block != nullptr)
      {
        m_release(m_block);
      }
    }

    /**
     * @brief Transfer the pointers and leave the original owner empty.
     * @par Requirements
     * J5-040; J5-051.
     */
    document_storage_owner::document_storage_owner(document_storage_owner&& other) noexcept
        : m_block(std::exchange(other.m_block, nullptr)), m_accounting(std::exchange(other.m_accounting, nullptr)),
          m_release(std::exchange(other.m_release, nullptr))
    {
    }

    /**
     * @brief Borrow accounting without changing storage ownership.
     * @par Requirements
     * J5-040; J5-046.
     */
    [[nodiscard]] accounted_storage* document_storage_owner::accounting() const noexcept
    {
      return m_accounting;
    }

  } // namespace detail
} // namespace adaptive_json5
