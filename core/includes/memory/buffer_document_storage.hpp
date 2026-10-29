#ifndef BUFFER_DOCUMENT_STORAGE_HPP_
#define BUFFER_DOCUMENT_STORAGE_HPP_

#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/result.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Construct document storage inside a caller-owned buffer.
     * @par Requirements
     * P003; P037; J5-040; J5-046; J5-047; J5-051; J5-101.
     * @param buffer Writable storage reserved exclusively for this document.
     * @param size Number of writable bytes in buffer.
     * @param max_owned_bytes Ceiling for outstanding requested allocation bytes.
     * @return A storage owner, or allocation_failure if the control block cannot fit.
     * @note The buffer must remain alive and at its original address until the
     * owner is destroyed. Null buffers fail without accessing storage.
     * Control-block bytes and alignment padding consume buffer capacity but are
     * excluded from max_owned_bytes, following accounted_storage's contract.
     * Performs no heap allocation. Failure constructs no live control block.
     */
    [[nodiscard]] result<document_storage_owner> make_buffer_document_storage(void* buffer, std::size_t size,
                                                                              std::size_t max_owned_bytes) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* BUFFER_DOCUMENT_STORAGE_HPP_ */
