#ifndef ADAPTIVE_JSON5_VALUE_VALUE_VIEW_HPP_
#define ADAPTIVE_JSON5_VALUE_VALUE_VIEW_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_kind.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace adaptive_json5
{
  namespace detail
  {
    class value_storage;
    struct value_record;
  } // namespace detail

  class value_iterator;
  /**
   * @brief Borrow checked, read-only access to one document-owned value.
   * @par Requirements
   * P030; P032; J5-028; J5-030; J5-031; J5-039; J5-040; J5-041.
   * @note Owns no storage and does not extend the document's lifetime.
   * Moving or destroying the storage invalidates this view.
   * Appending records preserves indices, but growth invalidates returned pointers.
   * A default-constructed view is invalid, not a null value.
   * Operations allocate nothing and never coerce between value types.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
  // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
  // implicit members that are never odr-used are not in its database.
  // codeql::misra_deviation_next_line(DEV-020.36)
  class [[nodiscard]] value_view
  {
    private:
      const detail::value_storage* m_storage = nullptr;
      value_index m_index{};

      /**
       * @brief Resolve the current record through checked storage access.
       * @par Requirements
       * P030; J5-039.
       * @note Reacquires the record instead of retaining a growth-sensitive pointer.
       * @par ID SWU-VALUE-122
       */
      [[nodiscard]] result<const detail::value_record*> record() const noexcept;

      /**
       * @brief Inspect the payload only when its stored type matches T.
       * @par Requirements
       * P030; J5-028; J5-039.
       * @note Returns a borrowed pointer without numeric or Boolean coercion.
       * @par ID SWU-VALUE-123
       */
      template <typename T>
      [[nodiscard]] result<const T*> payload_as() const noexcept;

      /**
       * @brief Resolve a direct child by its zero-based source-order position.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039.
       * @param position Direct-child ordinal, not a document record index.
       * @param object True for object members, false for array elements.
       * @note Requires document relationships maintained by validated operations.
       * Traversal is iterative and visits at most position plus one records.
       * P031 adds configurable work accounting.
       * @note A dangling child link reports internal_error (issue #125).
       * @par ID SWU-VALUE-124
       */
      [[nodiscard]] result<value_view> child_at(std::size_t position, bool object) const noexcept;

    public:
      /**
       * @brief Construct an invalid view without storage.
       * @par Requirements
       * P030; J5-039.
       */
      constexpr value_view() noexcept = default;

      /**
       * @brief Borrow a storage owner and a document-relative value index.
       * @par Requirements
       * P030; J5-039; J5-040.
       * @note The storage must remain alive at the same address.
       * Construction does not validate the index; accessors perform that check.
       * @par ID SWU-VALUE-125
       */
      value_view(const detail::value_storage& storage, value_index index) noexcept;

      /**
       * @brief Return the stored value type without conversion.
       * @par Requirements
       * P030; J5-028; J5-039.
       * @par ID SWU-VALUE-126
       */
      [[nodiscard]] result<value_kind> kind() const noexcept;
      /**
       * @brief Read a Boolean value without accepting numeric substitutes.
       * @par Requirements
       * P030; J5-028; J5-039.
       * @par ID SWU-VALUE-127
       */
      [[nodiscard]] result<bool> as_boolean() const noexcept;

      /**
       * @brief Borrow decoded string content after checking the value type.
       * @par Requirements
       * P030; J5-028; J5-039; J5-040.
       * @note The returned pointer is invalidated by storage growth or destruction.
       * @par ID SWU-VALUE-128
       */
      [[nodiscard]] result<const decoded_string*> as_string() const noexcept;

      /**
       * @brief Borrow an owned number after checking the value type.
       * @par Requirements
       * P030; J5-028; J5-039; J5-040.
       * @note Performs no numeric conversion.
       * The returned pointer is invalidated by storage growth or destruction.
       * @par ID SWU-VALUE-129
       */
      [[nodiscard]] result<const owned_number*> as_number() const noexcept;

      /**
       * @brief Copy this value's optional original source range.
       * @par Requirements
       * P032; J5-039; J5-041.
       * @return Stored span, successful nullopt when absent, or an access failure.
       * @note Absence of metadata is distinct from an invalid view.
       * Returned metadata owns no input text and extends no input lifetime.
       * The copied span remains valid independently of record storage.
       * @par ID SWU-VALUE-130
       */
      [[nodiscard]] result<std::optional<adaptive_json5::source_span>> source_span() const noexcept;

      /**
       * @brief Return the direct element or member count.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039.
       * @note Rejects scalar values rather than treating them as empty containers.
       * @par ID SWU-VALUE-131
       */
      [[nodiscard]] result<std::size_t> container_size() const noexcept;

      /**
       * @brief Borrow an array element by zero-based source-order position.
       * @par Requirements
       * P030; J5-030; J5-039; J5-040.
       * @note Rejects nonarrays (wrong_type) and out-of-range positions (index_out_of_range).
       * Linear traversal; repeated indexed enumeration has quadratic total cost.
       * @par ID SWU-VALUE-132
       */
      [[nodiscard]] result<value_view> element_at(std::size_t position) const noexcept;

      /**
       * @brief Borrow an object member's value by zero-based source-order position.
       * @par Requirements
       * P030; J5-031; J5-039; J5-040.
       * @note Includes every duplicate-name occurrence.
       * Rejects nonobjects (wrong_type) and out-of-range positions (index_out_of_range).
       * Linear traversal; repeated indexed enumeration has quadratic total cost.
       * @par ID SWU-VALUE-133
       * Use find_member() to look a member up by name in one pass.
       */
      [[nodiscard]] result<value_view> member_at(std::size_t position) const noexcept;

      /**
       * @brief Borrow the decoded name attached to an object-member value.
       * @par Requirements
       * P030; J5-031; J5-039; J5-040.
       * @note An empty member name succeeds; an absent name is an access error.
       * The returned pointer is invalidated by storage growth or destruction.
       * @par ID SWU-VALUE-134
       */
      [[nodiscard]] result<const decoded_string*> member_name() const noexcept;

      /**
       * @brief Copy an object member name's optional original source range.
       * @par Requirements
       * P036; J5-032; J5-039; J5-041; J5-044.
       * @return Stored span, successful nullopt when metadata is absent,
       * or an access failure when the view is invalid or has no member name.
       * @note An empty decoded member name is valid.
       * Quoted-name spans include their quotation marks.
       * Returned metadata contains no source pointer and performs no allocation.
       * @par ID SWU-VALUE-135
       */
      [[nodiscard]] result<std::optional<adaptive_json5::source_span>> member_name_source_span() const noexcept;

      /**
       * @brief Find the unique object member with the given UTF-8 name in one pass.
       * @par Requirements
       * P030; J5-030; J5-031; J5-033; J5-039; #124.
       * @param name Member name in UTF-8, compared exactly with the decoded name.
       * @return The member's value; wrong_type_or_access with wrong_type when this is not an
       * object, absent_member when no member has the name, or duplicate_member when more than
       * one member has it; internal_error when the member list is damaged; encoding when name
       * is not valid UTF-8.
       * @note Visits each member at most once (linear cost). Checks every member's parent and
       * name as member_at() does, and the list ends as value_iterator does: the first and last
       * links must match the count, only the last member may end the chain, and it must be
       * the member the object records as last. Preserved duplicates are never resolved
       * silently. Allocates nothing.
       * @note A dangling member link reports internal_error (issue #125).
       * @par ID SWU-VALUE-157
       */
      [[nodiscard]] result<value_view> find_member(std::string_view name) const noexcept;

      /**
       * @brief Return this view's tagged value index.
       * @par Requirements
       * P030; J5-039; J5-040.
       * @return Stored index, or invalid_argument for a default view.
       * @note Performs no allocation and does not extend storage lifetime.
       */
      [[nodiscard]] result<value_index> index() const noexcept;

      /**
       * @brief Begin linear direct-child iteration over this array or object.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039; J5-048; SSR-02; #154.
       * @return An iterator over the array elements or object members in source order;
       * wrong_type_or_access with invalid_argument for a default-constructed view, wrong_type
       * for a scalar; internal_error when the child list is damaged.
       * @note Each value_iterator::next() call does constant work, so a full walk is linear in
       * the child count, unlike repeated child_at() / at() calls. Allocates nothing. The
       * iterator borrows the same storage as this view and is invalidated with it.
       * @par ID SWU-VALUE-179
       */
      [[nodiscard]] result<value_iterator> children() const noexcept;
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_VALUE_VALUE_VIEW_HPP_ */
