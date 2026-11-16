#ifndef CONTAINER_LINKS_HPP_
#define CONTAINER_LINKS_HPP_

#include "value/record_link.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Describe an ordered chain of direct child values.
   * @par Requirements
   * P030; J5-030; J5-031.
   * @note Owns no child records or allocations.
   * first/last are untagged private record links (ADR 0007 §4).
   * Empty chains have count zero and unset first/last links.
   * Nonempty chains have set first/last links.
   * Each child record supplies the next-sibling link.
   * The last child's next-sibling link is unset.
   * count includes direct children only, not nested descendants.
   * Internal document construction enforces bounds, membership and acyclic links.
   */
  struct [[nodiscard]] container_links
  {
      detail::record_link first{};
      detail::record_link last{};
      std::size_t count{};
  };

  /**
   * @brief Describe an array's elements in source order.
   * @par Requirements
   * P030; J5-028; J5-030.
   * @note Repeated values remain separate element records.
   * Child ownership belongs to the document.
   */
  struct [[nodiscard]] array_data
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
  struct [[nodiscard]] object_data
  {
      container_links members{};
  };
} // namespace adaptive_json5

#endif /* CONTAINER_LINKS_HPP_ */
