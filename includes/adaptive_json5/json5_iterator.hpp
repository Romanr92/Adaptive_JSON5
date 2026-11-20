#ifndef JSON5_ITERATOR_HPP_
#define JSON5_ITERATOR_HPP_

#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/value_iterator.hpp"

#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Walk the elements of an array or the members of an object in source order.
   * @par Requirements
   * J5-030; J5-031; J5-039; J5-048; SSR-02; #154.
   * @note Created by json5_value::children(). Each next() does constant work, so a full walk
   * is linear in the child count. Borrows the document like json5_value; allocates nothing.
   * Not copyable, movable or assignable: hold the iterator returned by children() in place
   * (auto it = value.children(); relies on C++17 guaranteed copy elision).
   * Every function is noexcept and never throws.
   */
  class [[nodiscard]] json5_iterator
  {
    private:
      // Traversal state, or the error of the step that produced this iterator.
      result<value_iterator> m_state;
      // Scratch storage of the owning document, handed on to each child json5_value.
      storage_callbacks m_scratch;

      /**
       * @brief Wrap traversal state or an error together with the document's scratch storage.
       * @par Requirements
       * J5-030; J5-040.
       * @par ID SWU-VALUE-180
       */
      json5_iterator(result<value_iterator> state, const storage_callbacks& scratch) noexcept;

      friend class json5_value;

    public:
      /**
       * @brief End the walk; releases nothing, because the iterator owns no storage.
       * @par Requirements
       * J5-030; J5-040.
       * @note The document and its storage are unaffected; values already returned by
       * next() stay valid while the document lives.
       */
      ~json5_iterator() noexcept;

      /**
       * @brief Prevent copying, so one walk has exactly one traversal position.
       * @par Requirements
       * J5-030; J5-040.
       */
      json5_iterator(const json5_iterator&) = delete;

      /**
       * @brief Prevent copy assignment of traversal state.
       * @par Requirements
       * J5-030; J5-040.
       */
      json5_iterator& operator=(const json5_iterator&) = delete;

      /**
       * @brief Prevent moving; children() returns the iterator through guaranteed copy elision.
       * @par Requirements
       * J5-030; J5-040.
       */
      json5_iterator(json5_iterator&&) = delete;

      /**
       * @brief Prevent move assignment of traversal state.
       * @par Requirements
       * J5-030; J5-040.
       */
      json5_iterator& operator=(json5_iterator&&) = delete;

      /**
       * @brief Return the next child, or an empty optional at the end.
       * @par Requirements
       * J5-030; J5-031; J5-039; J5-048.
       * @return The next element/member; empty after the last one (repeatable); the held
       * error; wrong_type_or_access (wrong_type) when the value was not an array or object;
       * internal_error for a damaged child list.
       * @note Constant work per call. A failure leaves the position unchanged.
       * @par ID SWU-VALUE-181
       */
      [[nodiscard]] result<std::optional<json5_value>> next() noexcept;
  };
} // namespace adaptive_json5

#endif /* JSON5_ITERATOR_HPP_ */
