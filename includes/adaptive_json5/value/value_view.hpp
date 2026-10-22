#ifndef VALUE_VIEW_HPP_
#define VALUE_VIEW_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_kind.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  namespace detail
  {
    class value_storage;
    struct value_record;
  } // namespace detail
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
       */
      [[nodiscard]] result<const detail::value_record*> record() const noexcept;

      /**
       * @brief Inspect the payload only when its stored type matches T.
       * @par Requirements
       * P030; J5-028; J5-039.
       * @note Returns a borrowed pointer without numeric or Boolean coercion.
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
       */
      value_view(const detail::value_storage& storage, value_index index) noexcept;

      /**
       * @brief Return the stored value type without conversion.
       * @par Requirements
       * P030; J5-028; J5-039.
       */
      [[nodiscard]] result<value_kind> kind() const noexcept;
      /**
       * @brief Read a Boolean value without accepting numeric substitutes.
       * @par Requirements
       * P030; J5-028; J5-039.
       */
      [[nodiscard]] result<bool> as_boolean() const noexcept;

      /**
       * @brief Borrow decoded string content after checking the value type.
       * @par Requirements
       * P030; J5-028; J5-039; J5-040.
       * @note The returned pointer is invalidated by storage growth or destruction.
       */
      [[nodiscard]] result<const decoded_string*> as_string() const noexcept;

      /**
       * @brief Borrow an owned number after checking the value type.
       * @par Requirements
       * P030; J5-028; J5-039; J5-040.
       * @note Performs no numeric conversion.
       * The returned pointer is invalidated by storage growth or destruction.
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
       */
      [[nodiscard]] result<std::optional<adaptive_json5::source_span>> source_span() const noexcept;

      /**
       * @brief Return the direct element or member count.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039.
       * @note Rejects scalar values rather than treating them as empty containers.
       */
      [[nodiscard]] result<std::size_t> container_size() const noexcept;

      /**
       * @brief Borrow an array element by zero-based source-order position.
       * @par Requirements
       * P030; J5-030; J5-039; J5-040.
       * @note Rejects nonarrays and out-of-range positions.
       * Linear traversal; repeated indexed enumeration has quadratic total cost.
       */
      [[nodiscard]] result<value_view> element_at(std::size_t position) const noexcept;

      /**
       * @brief Borrow an object member's value by zero-based source-order position.
       * @par Requirements
       * P030; J5-031; J5-039; J5-040.
       * @note Includes every duplicate-name occurrence.
       * Rejects nonobjects and out-of-range positions.
       * Linear traversal; repeated indexed enumeration has quadratic total cost.
       */
      [[nodiscard]] result<value_view> member_at(std::size_t position) const noexcept;

      /**
       * @brief Borrow the decoded name attached to an object-member value.
       * @par Requirements
       * P030; J5-031; J5-039; J5-040.
       * @note An empty member name succeeds; an absent name is an access error.
       * The returned pointer is invalidated by storage growth or destruction.
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
       */
      [[nodiscard]] result<std::optional<adaptive_json5::source_span>> member_name_source_span() const noexcept;
  };
} // namespace adaptive_json5

#endif /* VALUE_VIEW_HPP_ */
