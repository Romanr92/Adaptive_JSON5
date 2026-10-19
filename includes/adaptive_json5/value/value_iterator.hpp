#ifndef VALUE_ITERATOR_HPP_
#define VALUE_ITERATOR_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  struct container_links;
  namespace detail
  {
    class value_storage;
    struct value_record;
  } // namespace detail
  /**
   * @brief Traverse direct array elements or object members in source order.
   * @par Requirements
   * P030; J5-030; J5-031; J5-039; J5-040.
   * @note Borrows storage without extending its lifetime.
   * Moving or destroying the owning document invalidates this iterator.
   * Do not modify document relationships during iteration.
   * Traversal allocates nothing and follows no nested descendants.
   * A default-constructed iterator is invalid.
   */
  class value_iterator
  {
    private:
      const detail::value_storage* m_storage = nullptr;
      value_index m_parent{};
      value_index m_next{};
      std::size_t m_remaining{};
      bool m_object{};

      /**
       * @brief Retain the validated container's direct-child traversal state.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039.
       */
      value_iterator(const detail::value_storage& storage, value_index parent, const container_links& links,
                     bool object) noexcept;

    public:
      /**
       * @brief Construct an invalid iterator without borrowing storage.
       * @par Requirements
       * P030; J5-039.
       */
      constexpr value_iterator() noexcept = default;

      /**
       * @brief Begin direct-child iteration over an array or object.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039.
       * @note Rejects scalar values and invalid container indices.
       * Requires relationships maintained by validated document operations.
       */
      static result<value_iterator> create(const detail::value_storage& storage, value_index parent) noexcept;

      /**
       * @brief Return the next direct child or successful end of iteration.
       * @par Requirements
       * P030; J5-030; J5-031; J5-039; J5-040.
       * @note Failure leaves traversal state unchanged.
       * Repeated calls after completion return successful empty optionals.
       * Each call performs constant work; P031 adds work accounting.
       */
      result<std::optional<value_view>> next() noexcept;
  };
} // namespace adaptive_json5

#endif /* VALUE_ITERATOR_HPP_ */
