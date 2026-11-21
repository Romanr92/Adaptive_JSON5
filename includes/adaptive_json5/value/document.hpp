#ifndef ADAPTIVE_JSON5_VALUE_DOCUMENT_HPP_
#define ADAPTIVE_JSON5_VALUE_DOCUMENT_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/member_lookup.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    struct value_record;

    struct document_access;
    /**
     * @brief Own a growable sequence of independently stored value records.
     * @par Requirements
     * P030; J5-028; J5-039; J5-040; J5-046.
     * @note This declaration supports document's by-value layout; direct use of
     * detail::value_storage is not a supported public API.
     * Indices remain stable because records are appended without reordering.
     * Growth invalidates record pointers, but not their indices.
     * Provider state must outlive storage and owned payload allocations.
     * Cleanup is iterative and never follows container links.
     * max_records is a component limit, not complete P031 resource accounting.
     */
    class [[nodiscard]] value_storage
    {
      public:
        static constexpr std::size_t c_empty_size = 0;

      private:
        storage_callbacks m_provider;
        owned_block m_identity{};
        owned_block m_storage{};
        std::size_t m_size{};
        std::size_t m_capacity{};
        std::size_t m_max_records;
        bool m_fixed{}; // Set by reserve_exact(); appends then never grow.

        /**
         * @brief Address one slot within an already size-checked allocation.
         * @par Requirements
         * P030; J5-040; J5-046.
         * @note Private callers provide index below the allocated capacity.
         * This returns storage, without assuming a record is alive there.
         * @par ID SWU-VALUE-109
         */
        [[nodiscard]] static void* slot_address(const owned_block& storage, std::size_t index) noexcept;

        /**
         * @brief Access an already constructed record.
         * @par Requirements
         * P030; J5-039; J5-040.
         * @note Private callers provide index below the constructed record count.
         * @par ID SWU-VALUE-110
         */
        [[nodiscard]] value_record* live_record(std::size_t index) noexcept;

        /**
         * @brief Access an already constructed record without mutation.
         * @par Requirements
         * P030; J5-039; J5-040.
         * @note Private callers provide index below the constructed record count.
         * @par ID SWU-VALUE-111
         */
        [[nodiscard]] const value_record* live_record(std::size_t index) const noexcept;

        /**
         * @brief Destroy constructed records without releasing the raw block.
         * @par Requirements
         * P030; J5-040.
         * @note Reverse-order iteration uses no recursive child traversal.
         * @par ID SWU-VALUE-112
         */
        void destroy_records() noexcept;

        /**
         * @brief Ensure capacity before moving an incoming record.
         * @par Requirements
         * P030; J5-040; J5-046.
         * @note Allocation failure leaves existing records unchanged.
         * Growth temporarily retains both old and new raw blocks.
         * After reserve_exact(), a requirement beyond the capacity fails with
         * wrong_type_or_access (invalid_argument) instead of growing.
         * @par ID SWU-VALUE-113
         */
        // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
        // MISRA justification: The declaration and the definition use the same alias (status_result for
        // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
        // alias across translation units and reports it as different.
        // codeql::misra_deviation_next_line(DEV-013.20)
        [[nodiscard]] status_result reserve(std::size_t required) noexcept;

      public:
        /**
         * @brief Construct empty storage with an explicit record limit.
         * @par Requirements
         * P030; J5-040; J5-046.
         * @note Construction allocates nothing; a zero limit permits no records.
         * @par ID SWU-VALUE-114
         */
        value_storage(const storage_callbacks& provider, std::size_t max_records) noexcept;

        /**
         * @brief Transfer storage ownership and leave the source empty.
         * @par Requirements
         * P030; J5-040.
         * @note The source retains its provider and limit and can be reused.
         * @par ID SWU-VALUE-115
         */
        value_storage(value_storage&& other) noexcept;

        /**
         * @brief Destroy each live record before releasing its raw storage.
         * @par Requirements
         * P030; J5-040.
         * @note Performs no allocation and does not recursively follow children.
         * @par ID SWU-VALUE-117
         */
        ~value_storage() noexcept;

        /**
         * @brief Copying is not allowed.
         * @par Requirements
         * P030; J5-040; J5-046.
         * @note Copying every record needs storage, which can fail, and would duplicate
         * ownership of their payloads.
         */
        value_storage(const value_storage&) = delete;

        /**
         * @brief Copy assignment is not allowed.
         * @par Requirements
         * P030; J5-040.
         * @note Same reason as the copy constructor.
         */
        value_storage& operator=(const value_storage&) = delete;

        /**
         * @brief Move assignment is not allowed.
         * @par Requirements
         * P030; J5-040.
         * @note Replacing live records would have to destroy them first; construct new
         * storage instead.
         */
        value_storage& operator=(value_storage&&) = delete;

        /**
         * @brief Allocate exactly the given number of records once and stop growing.
         * @par Requirements
         * P030; J5-040; J5-046.
         * @param records Exact number of records this storage will hold.
         * @return Success, resource_limit above max_records, a provider failure, or
         * internal_error when storage was already allocated or reserved.
         * @note Call on empty storage only. Zero records allocate nothing.
         * A nonzero reservation allocates one identity token and one record block;
         * both requests use the provider and count against its owned-byte budget.
         * Afterwards, an append beyond records fails with wrong_type_or_access (invalid_argument) and never
         * allocates (ADR 0003, decision 7). Failure leaves the storage growable and unchanged.
         * @par ID SWU-VALUE-116
         */
        // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
        // MISRA justification: The declaration and the definition use the same alias (status_result for
        // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
        // alias across translation units and reports it as different.
        // codeql::misra_deviation_next_line(DEV-013.19)
        [[nodiscard]] status_result reserve_exact(std::size_t records) noexcept;

        /**
         * @brief Append an externally held record and return its stable index.
         * @par Requirements
         * P030; J5-028; J5-040; J5-046.
         * @param record Record that does not reside inside this storage.
         * @return Its new index or an explicit limit/allocation error.
         * @note Failure leaves existing records and the incoming record unchanged.
         * Success moves from record. Growth invalidates earlier record pointers.
         * Relationship invariants are enforced by the later document builder.
         * @par ID SWU-VALUE-118
         */
        [[nodiscard]] result<value_index> append(value_record&& record) noexcept;

        /**
         * @brief Access a record after checking its index.
         * @par Requirements
         * P030; J5-039.
         * @note Returned pointers are invalidated by growth or storage destruction.
         * @par ID SWU-VALUE-119
         */
        [[nodiscard]] result<value_record*> at(value_index index) noexcept;

        /**
         * @brief Access a record without permitting mutation.
         * @par Requirements
         * P030; J5-039.
         * @note Returned pointers are invalidated by growth or storage destruction.
         * @par ID SWU-VALUE-120
         */
        [[nodiscard]] result<const value_record*> at(value_index index) const noexcept;

        /**
         * @brief Return the number of constructed records.
         * @par Requirements
         * P030; J5-028.
         * @par ID SWU-VALUE-121
         */
        [[nodiscard]] std::size_t size() const noexcept;

        /**
         * @brief Return the identity token for this storage.
         * @par Requirements
         * P030; J5-039; J5-040; J5-046.
         * @return Token address, or nullptr before the first successful record allocation.
         * @note Used only for comparison and never dereferenced. The token remains stable
         * during growth and transfers with the records when storage is moved.
         * Moved-from storage has no token until its next successful record allocation.
         */
        [[nodiscard]] const void* identity() const noexcept;

        /**
         * @brief Tag an internal record position with this storage's identity.
         * @par Requirements
         * P030; J5-039; J5-040.
         * @param position Record position, or value_index::c_unset_position.
         * @return Index carrying the position and this storage's identity token,
         * or a default unset index for the sentinel.
         * @note Performs no allocation or membership/bounds validation.
         * Access through at() validates the index before reading a record.
         */
        [[nodiscard]] value_index issue(std::size_t position) const noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

namespace adaptive_json5
{
  /**
   * @brief Own value records and expose checked read-only views.
   * @par Requirements
   * P030; P032; J5-028; J5-030; J5-031; J5-033; J5-039; J5-040; J5-041.
   * @note Values are created detached until selected as root or attached later.
   * All indices passed to this document must originate from this document.
   * Indices carry a storage identity token so access can reject indices from another document.
   * Moving or destroying the document invalidates its existing views and iterators.
   * Appending preserves indices but may invalidate borrowed payload pointers.
   * Every storage provider used by records or payloads must outlive its allocations.
   * This component record limit does not replace P031 resource accounting.
   * Thread safety: const operations change no state, so several threads may read one
   * document concurrently. Any modification, move or destruction must not overlap
   * with other access to the same document. Each value_iterator is single-threaded.
   * Numeric conversions that take scratch storage modify that storage; concurrent
   * conversions need one provider per thread.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions
  // MISRA justification: The destructor is explicitly declared (~T() noexcept = default) next to the custom
  // move constructor, and copy and move assignment are explicitly deleted, so all special members are declared.
  // codeql::misra_deviation_next_line(DEV-020.06)
  class [[nodiscard]] document
  {
    private:
      // Construction and mutation are internal (ADR 0007); the parser reaches them
      // through detail::document_access. Not a supported public API.
      friend struct detail::document_access;

      detail::value_storage m_values;
      value_index m_root{};

      /**
       * @brief Construct an empty document with explicit storage and record limit.
       * @par Requirements
       * P030; J5-028; J5-040.
       * @note Allocates nothing and initially has no root.
       * @par ID SWU-VALUE-023
       */
      document(const storage_callbacks& storage, std::size_t max_records) noexcept;

      /**
       * @brief Create a detached null value.
       * @par Requirements
       * P030; J5-028; J5-046.
       * @par ID SWU-VALUE-025
       */
      [[nodiscard]] result<value_index> make_null() noexcept;

      /**
       * @brief Create a detached Boolean without numeric coercion.
       * @par Requirements
       * P030; J5-028; J5-046.
       * @par ID SWU-VALUE-026
       */
      [[nodiscard]] result<value_index> make_boolean(bool value) noexcept;

      /**
       * @brief Transfer decoded content into a detached string value.
       * @par Requirements
       * P030; J5-028; J5-040; J5-046.
       * @note Consumes value on entry, even if record allocation subsequently fails.
       * Empty decoded strings are valid values.
       * @par ID SWU-VALUE-027
       */
      [[nodiscard]] result<value_index> make_string(decoded_string value) noexcept;

      /**
       * @brief Transfer validated numeric spelling into a detached numeric value.
       * @par Requirements
       * P030; J5-028; J5-034; J5-038; J5-040; J5-046.
       * @note Consumes value on entry, even if record allocation subsequently fails.
       * Rejects moved-from owners with empty spelling.
       * @par ID SWU-VALUE-028
       */
      [[nodiscard]] result<value_index> make_number(owned_number value) noexcept;

      /**
       * @brief Create a detached empty array.
       * @par Requirements
       * P030; J5-028; J5-030; J5-046.
       * @par ID SWU-VALUE-029
       */
      [[nodiscard]] result<value_index> make_array() noexcept;

      /**
       * @brief Create a detached empty object.
       * @par Requirements
       * P030; J5-028; J5-031; J5-046.
       * @par ID SWU-VALUE-030
       */
      [[nodiscard]] result<value_index> make_object() noexcept;

      /**
       * @brief Append a detached value or subtree to an array.
       * @par Requirements
       * P030; J5-030; J5-039; J5-040.
       * @param array Array index belonging to this document.
       * @param child Detached child index belonging to this document.
       * @return Success or an explicit relationship/access error.
       * @note Preserves element order and performs no allocation.
       * Rejects wrong container type, existing attachment, root reuse and cycles.
       * The cycle check walks the parent's ancestors: O(depth), bounded by the record count.
       * Failure leaves the document unchanged.
       * @par ID SWU-VALUE-031
       */
      // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
      // MISRA justification: The declaration and the definition use the same alias (status_result for
      // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
      // alias across translation units and reports it as different.
      // codeql::misra_deviation_next_line(DEV-013.18)
      [[nodiscard]] status_result append_element(value_index array, value_index child) noexcept;

      /**
       * @brief Append a named detached value or subtree to an object.
       * @par Requirements
       * P030; J5-031; J5-039; J5-040.
       * @param object Object index belonging to this document.
       * @param name External owned decoded name, transferred only on success.
       * @param child Detached child index belonging to this document.
       * @return Success or an explicit relationship/access error.
       * @note Preserves member order, empty names and duplicate names.
       * Performs no allocation; the name must already own its decoded content.
       * Failure leaves the document and supplied name unchanged.
       * Success moves from name. Its provider must outlive the transferred allocation.
       * The cycle check walks the parent's ancestors: O(depth), bounded by the record count.
       * @par ID SWU-VALUE-032
       */
      // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
      // MISRA justification: The declaration and the definition use the same alias (status_result for
      // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
      // alias across translation units and reports it as different.
      // codeql::misra_deviation_next_line(DEV-013.17)
      [[nodiscard]] status_result append_member(value_index object, decoded_string&& name, value_index child) noexcept;

      /**
       * @brief Append a named detached value with its original name-token range.
       * @par Requirements
       * P036; J5-031; J5-032; J5-039; J5-040; J5-041.
       * @param object Object index belonging to this document.
       * @param name Owned decoded name, transferred only on success.
       * @param child Detached child index belonging to this document.
       * @param name_span Original member-name token offset and length in bytes.
       * @param input_size Original input size used to validate name_span.
       * @return Success or an explicit relationship/access/range failure.
       * @note Preserves source member order, empty names and repeated names.
       * Failure leaves relationships, metadata and the supplied name unchanged.
       * Success transfers the name and records its span atomically.
       * Stores no input pointer and performs no allocation.
       * Does not enforce duplicate-name rejection.
       * The cycle check walks the parent's ancestors: O(depth), bounded by the record count.
       * @par ID SWU-VALUE-033
       */
      // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
      // MISRA justification: The declaration and the definition use the same alias (status_result for
      // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
      // alias across translation units and reports it as different.
      // codeql::misra_deviation_next_line(DEV-013.16)
      [[nodiscard]] status_result append_member(value_index object, decoded_string&& name, value_index child,
                                                adaptive_json5::source_span name_span, std::size_t input_size) noexcept;

      /**
       * @brief Assign or replace a value's original source range.
       * @par Requirements
       * P032; J5-039; J5-041.
       * @param index Value index belonging to this document.
       * @param span Original zero-based byte offset and byte length.
       * @param input_size Original input size in bytes.
       * @return Success or an explicit access/range failure.
       * @note Failure leaves existing metadata unchanged.
       * A zero-length range at input_size is permitted.
       * Stores no input pointer and performs no allocation.
       * The caller must supply the correct original input size and provenance.
       * Bounds validation does not prove that the range spells this value.
       * @par ID SWU-VALUE-034
       */
      // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
      // MISRA justification: The declaration and the definition use the same alias (status_result for
      // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
      // alias across translation units and reports it as different.
      // codeql::misra_deviation_next_line(DEV-013.15)
      [[nodiscard]] status_result set_source_span(value_index index, adaptive_json5::source_span span,
                                                  std::size_t input_size) noexcept;

      /**
       * @brief Assign or replace an object member name's original source range.
       * @par Requirements
       * P036; J5-032; J5-039; J5-041; J5-044.
       * @param index Object-member value index belonging to this document.
       * @param span Original member-name token offset and length in bytes.
       * @param input_size Original input size in bytes.
       * @return Success or an explicit access/range failure.
       * @note Requires an existing member_name, including a valid empty name.
       * The span describes source spelling, including quotes when present.
       * Failure leaves existing metadata unchanged.
       * Stores no input pointer and performs no allocation.
       * The caller supplies the correct input size and source provenance.
       * @par ID SWU-VALUE-035
       */
      // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
      // MISRA justification: The declaration and the definition use the same alias (status_result for
      // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
      // alias across translation units and reports it as different.
      // codeql::misra_deviation_next_line(DEV-013.14)
      [[nodiscard]] status_result set_member_name_source_span(value_index index, adaptive_json5::source_span span,
                                                              std::size_t input_size) noexcept;

      /**
       * @brief Establish one detached value as the document root.
       * @par Requirements
       * P030; J5-028; J5-039.
       * @note Rejects an existing root, invalid index or attached/member record.
       * Failure leaves the document unchanged.
       * @par ID SWU-VALUE-036
       */
      // MISRA rule: RULE-6-9-1 cpp/misra/type-aliases-declaration
      // MISRA justification: The declaration and the definition use the same alias (status_result for
      // result<void>, or the same callback alias). There is only one alias declaration; the tool compares the
      // alias across translation units and reports it as different.
      // codeql::misra_deviation_next_line(DEV-013.13)
      [[nodiscard]] status_result set_root(value_index index) noexcept;

    public:
      /**
       * @brief Transfer record ownership and leave the source empty.
       * @par Requirements
       * P030; J5-040.
       * @note Existing views into either moved-from document state are invalid.
       * The source retains its storage configuration and can be reused.
       * @par ID SWU-VALUE-024
       */
      document(document&& other) noexcept;

      /**
       * @brief Release all records and their owned payloads iteratively.
       * @par Requirements
       * P030; J5-040.
       */
      ~document() noexcept = default;

      /**
       * @brief Copying is not allowed.
       * @par Requirements
       * P030; J5-040; J5-046.
       * @note A copy of every value needs storage, which can fail, and would duplicate
       * ownership; documents are moved instead.
       */
      document(const document&) = delete;

      /**
       * @brief Copy assignment is not allowed.
       * @par Requirements
       * P030; J5-040.
       * @note Same reason as the copy constructor.
       */
      document& operator=(const document&) = delete;

      /**
       * @brief Move assignment is not allowed.
       * @par Requirements
       * P030; J5-040.
       * @note Replacing a live document would release its values first; construct a new
       * document instead.
       */
      document& operator=(document&&) = delete;

      /**
       * @brief Borrow checked read-only access to a document-relative value.
       * @par Requirements
       * P030; J5-039; J5-040.
       * @note The returned view does not extend this document's lifetime.
       * @par ID SWU-VALUE-037
       */
      [[nodiscard]] result<value_view> view(value_index index) const noexcept;

      /**
       * @brief Begin source-order iteration over a container's direct children.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039; J5-040.
       * @param container Array or object index belonging to this document.
       * @note Rejects invalid indices and scalar values.
       * Empty containers produce valid iterators that immediately report completion.
       * The iterator borrows this document and allocates nothing.
       * Moving or destroying this document invalidates the iterator.
       * Do not modify document relationships during iteration.
       * @par ID SWU-VALUE-038
       */
      [[nodiscard]] result<value_iterator> iterate(value_index container) const noexcept;

      /**
       * @brief Inspect an object's members for an exact decoded-name match.
       * @par Requirements
       * P030; J5-031; J5-033; J5-039; J5-040.
       * @param object Object index belonging to this document.
       * @param name Decoded name borrowed for this call only.
       * @return Absent, unique or ambiguous match, or an explicit access failure.
       * @note Rejects invalid indices, arrays and scalar values.
       * Only a unique match supplies a borrowed value view.
       * Preserves duplicate members and never selects among ambiguous matches.
       * Allocates nothing and does not modify the document or name.
       * Moving or destroying the document invalidates any returned view.
       * @par ID SWU-VALUE-039
       */
      [[nodiscard]] result<member_lookup> lookup_member(value_index object, const decoded_string& name) const noexcept;

      /**
       * @brief Borrow the root or report that no root has been established.
       * @par Requirements
       * P030; J5-028; J5-039; J5-040.
       * @note An absent root is an access error, not a null value.
       * @par ID SWU-VALUE-040
       */
      [[nodiscard]] result<value_view> root() const noexcept;

      /**
       * @brief Return the number of owned records, including detached values.
       * @par Requirements
       * P030; J5-028.
       * @par ID SWU-VALUE-041
       */
      [[nodiscard]] std::size_t size() const noexcept;
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_VALUE_DOCUMENT_HPP_ */
