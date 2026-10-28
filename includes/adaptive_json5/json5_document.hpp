#ifndef JSON5_DOCUMENT_HPP_
#define JSON5_DOCUMENT_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Own parsed values and their storage control block.
   * @par Requirements
   * P037; J5-026; J5-040; J5-041; J5-046; J5-051; J5-101.
   * @note Caller-buffer parsing uses no heap.
   * The caller buffer must remain alive and exclusively reserved until this
   * document is destroyed. Moving transfers ownership without moving the buffer.
   * Borrowed views follow document's existing move and lifetime rules.
   */
  class [[nodiscard]] json5_document
  {
    private:
      // Storage must be destroyed after every allocation belonging to m_parsed.
      detail::document_storage_owner m_storage;
      parsed_document m_parsed;

      /**
       * @brief Adopt parsed data and the storage supporting it.
       * @par Requirements
       * J5-040; J5-051.
       * @note parsed must have been allocated through storage's accounting.
       */
      json5_document(detail::document_storage_owner&& storage, parsed_document&& parsed) noexcept;

    public:
      /**
       * @brief Parse one complete document using a caller-owned buffer.
       * @par Requirements
       * P037; J5-026; J5-040; J5-046; J5-047; J5-051; J5-101.
       * @param input Borrowed input, unchanged throughout the call.
       * @param buffer Writable storage exclusively reserved for this document.
       * @param buffer_size Number of writable bytes in buffer.
       * @param options Syntax, policies and limits.
       * @return An owning document or an explicit failure.
       * @note Input and buffer must not overlap.
       * Failure releases this call's allocations and destroys its control block;
       * buffer contents are unspecified, but the buffer may be reused.
       * Source retention follows options without modification.
       * Physical exhaustion reports allocation_failure; configured limits report
       * resource_limit. Control-block overhead consumes buffer capacity but is
       * excluded from the requested-byte accounting ceiling.
       */
      [[nodiscard]] static result<json5_document> parse(input_view input, void* buffer, std::size_t buffer_size,
                                                        const parse_options& options = {}) noexcept;

      /**
       * @brief Transfer parsed data and ownership, leaving other empty.
       * @par Requirements
       * J5-040; J5-051.
       * @note Storage accounting retains its original address.
       */
      json5_document(json5_document&& other) noexcept;

      /**
       * @brief Release parsed allocations before destroying their storage.
       * @par Requirements
       * J5-040; J5-051.
       */
      ~json5_document() noexcept;

      /**
       * @brief Prevent copying exclusive document ownership.
       * @par Requirements
       * J5-040.
       */
      json5_document(const json5_document&) = delete;

      /**
       * @brief Prevent copy assignment of exclusive ownership.
       * @par Requirements
       * J5-040.
       */
      json5_document& operator=(const json5_document&) = delete;

      /**
       * @brief Prevent replacing live storage before its allocations are released.
       * @par Requirements
       * J5-040; J5-051.
       */
      json5_document& operator=(json5_document&&) = delete;

      /**
       * @brief Start navigating at the root value.
       * @par Requirements
       * J5-026; J5-028; J5-040.
       * @return The root, or a value holding the error of document::root(), for example
       * after this document was moved from.
       * @note The returned value borrows this document and must not outlive it or a move.
       */
      [[nodiscard]] json5_value root() const noexcept;

      /**
       * @brief Borrow the parsed values through the advanced document API.
       * @par Requirements
       * J5-028; J5-040.
       */
      [[nodiscard]] const document& values() const noexcept;

      /**
       * @brief Report outstanding requested allocation bytes.
       * @par Requirements
       * J5-046; J5-047.
       * @note Returns zero after ownership has been moved away.
       */
      [[nodiscard]] std::size_t current_bytes() const noexcept;

      /**
       * @brief Report the highest outstanding requested allocation byte count.
       * @par Requirements
       * J5-046; J5-047.
       * @note Returns zero after ownership has been moved away.
       */
      [[nodiscard]] std::size_t peak_bytes() const noexcept;
  };
} // namespace adaptive_json5

#endif /* JSON5_DOCUMENT_HPP_ */
