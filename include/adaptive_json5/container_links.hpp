#ifndef CONTAINER_LINKS_HPP_
#define CONTAINER_LINKS_HPP_

#include "value_index.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Describe an ordered chain of direct child values.
   * @par Requirements
   * P030; J5-030; J5-031.
   * @note Owns no child records or allocations.
   * Empty chains have count zero and unset first/last indices.
   * Nonempty chains have set first/last indices.
   * Each child record supplies the next-sibling index.
   * The last child's next-sibling index is unset.
   * count includes direct children only, not nested descendants.
   * The document builder must enforce bounds, membership and acyclic links.
   */
  struct container_links
  {
      value_index first{};
      value_index last{};
      std::size_t count{};
  };

  /**
   * @brief Describe an array's elements in source order.
   * @par Requirements
   * P030; J5-028; J5-030.
   * @note Repeated values remain separate element records.
   * Child ownership belongs to the document.
   */
  struct array_data
  {
      container_links elements{};
  };

  /**
   * @brief Describe an object's members in source order.
   * @par Requirements
   * P030; J5-028; J5-031.
   * @note Every member remains observable, including repeated decoded names.
   * Each direct child record will retain its owned member name.
   * These links do not perform lookup or apply duplicate-name policy.
   * Child ownership belongs to the document.
   */
  struct object_data
  {
      container_links members{};
  };
} // namespace adaptive_json5

#endif /* CONTAINER_LINKS_HPP_ */
