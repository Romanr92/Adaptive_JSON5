#ifndef HOSTED_PARSE_HPP_
#define HOSTED_PARSE_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/retained_source.hpp"
#include "adaptive_json5/value/warning_list.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    struct hosted_storage_block;

    /**
     * @brief Own the heap block that holds a hosted document's storage accounting.
     * @par Requirements
     * J5-040; J5-046; J5-051.
     * @note Not a supported public API. Releases the block on destruction; a moved-from
     * owner holds no block.
     */
    class [[nodiscard]] hosted_storage_owner
    {
      private:
        hosted_storage_block* m_block{};

      public:
        /**
         * @brief Take ownership of a block, or of nothing when block is nullptr.
         * @par Requirements
         * J5-040.
         */
        explicit hosted_storage_owner(hosted_storage_block* block) noexcept;

        /**
         * @brief Release the owned block.
         * @par Requirements
         * J5-040.
         */
        ~hosted_storage_owner() noexcept;

        /**
         * @brief Transfer the block and leave other empty.
         * @par Requirements
         * J5-040.
         */
        hosted_storage_owner(hosted_storage_owner&& other) noexcept;

        /**
         * @brief Copying is not allowed.
         * @par Requirements
         * J5-040.
         * @note Two owners of one block would both delete it, so the block would be
         * released twice. Ownership can only be transferred by moving.
         */
        hosted_storage_owner(const hosted_storage_owner&) = delete;

        /**
         * @brief Copy assignment is not allowed.
         * @par Requirements
         * J5-040.
         * @note Same reason as the copy constructor: one block must have exactly one owner.
         */
        hosted_storage_owner& operator=(const hosted_storage_owner&) = delete;

        /**
         * @brief Move assignment is not allowed.
         * @par Requirements
         * J5-040.
         * @note Replacing the block of a live owner would release accounting that a
         * document may still need. Only move construction transfers a block, into an
         * owner that has none yet.
         */
        hosted_storage_owner& operator=(hosted_storage_owner&&) = delete;

        /**
         * @brief Borrow the owned block, or nullptr.
         * @par Requirements
         * J5-040.
         */
        [[nodiscard]] hosted_storage_block* get() const noexcept;
    };
  } // namespace detail

  class hosted_document;

  /**
   * @brief Parse a complete document with heap storage and default or given options.
   * @par Requirements
   * P037; J5-026; J5-040; J5-046; J5-047; J5-051.
   * @param input Borrowed bytes, alive and unchanged throughout this call.
   * @param options Syntax, duplicate policy and limits; defaults to parse_options{}.
   * @return A document that owns its storage, or an explicit failure.
   * @note Same parsing, limits and errors as parse_document. Storage comes from the
   * global heap through ::operator new(std::nothrow); max_owned_bytes still bounds it.
   * An unavailable heap block reports allocation_failure without a location.
   * Optional hosted component (AdaptiveJSON5::hosted); not part of the embedded parser
   * target (J5-101). Thread safety as parse_document: each call has its own storage.
   */
  [[nodiscard]] result<hosted_document> parse(input_view input, const parse_options& options = {}) noexcept;

  /**
   * @brief Own a parsed document together with the storage it was allocated from.
   * @par Requirements
   * P037; J5-040; J5-041; J5-051.
   * @note Movable, not copyable. The document's memory is released before its storage
   * accounting. A moved-from object holds an empty document and reports zero bytes.
   */
  class [[nodiscard]] hosted_document
  {
    private:
      // Declared first so it is destroyed last, after the document has released its memory.
      detail::hosted_storage_owner m_storage;
      parsed_document m_parsed;

      /**
       * @brief Combine a storage block with the document parsed through it.
       * @par Requirements
       * J5-040.
       */
      hosted_document(detail::hosted_storage_owner&& storage, parsed_document&& parsed) noexcept;

      friend result<hosted_document> parse(input_view input, const parse_options& options) noexcept;

    public:
      /**
       * @brief Transfer the document and its storage, leaving other empty.
       * @par Requirements
       * J5-040; J5-051.
       * @note The storage block moves by pointer, so the accounting keeps its address
       * and the document's allocations stay valid. other reports zero bytes afterwards.
       */
      hosted_document(hosted_document&& other) noexcept = default;

      /**
       * @brief Release the document's memory, then its storage block.
       * @par Requirements
       * J5-040.
       * @note Member order guarantees this sequence: m_parsed is destroyed before
       * m_storage, so every release still finds its accounting.
       */
      ~hosted_document() noexcept = default;

      /**
       * @brief Copying is not allowed.
       * @par Requirements
       * J5-040.
       * @note A document owns its values and storage exclusively; copying would need
       * a second storage block and a deep copy of every value.
       */
      hosted_document(const hosted_document&) = delete;

      /**
       * @brief Copy assignment is not allowed.
       * @par Requirements
       * J5-040.
       * @note Same reason as the copy constructor.
       */
      hosted_document& operator=(const hosted_document&) = delete;

      /**
       * @brief Move assignment is not allowed.
       * @par Requirements
       * J5-040.
       * @note document itself cannot be move-assigned, and replacing a live document
       * would have to release its values and storage in the right order first.
       * Construct a new hosted_document instead.
       */
      hosted_document& operator=(hosted_document&&) = delete;

      /**
       * @brief Borrow the parsed values.
       * @par Requirements
       * J5-028; J5-040.
       */
      [[nodiscard]] const document& values() const noexcept;

      /**
       * @brief Borrow every retained warning in source order.
       * @par Requirements
       * J5-017; J5-041.
       */
      [[nodiscard]] const warning_list& warnings() const noexcept;

      /**
       * @brief Borrow the retained source copy, empty unless options requested it.
       * @par Requirements
       * J5-041.
       */
      [[nodiscard]] const retained_source& source() const noexcept;

      /**
       * @brief Report bytes currently owned through this document's storage.
       * @par Requirements
       * J5-046; J5-047.
       */
      [[nodiscard]] std::size_t current_bytes() const noexcept;

      /**
       * @brief Report the highest owned byte count reached, including during parsing.
       * @par Requirements
       * J5-046; J5-047.
       */
      [[nodiscard]] std::size_t peak_bytes() const noexcept;
  };
} // namespace adaptive_json5

#endif /* HOSTED_PARSE_HPP_ */
