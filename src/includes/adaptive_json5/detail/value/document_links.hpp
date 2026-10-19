#ifndef DOCUMENT_LINKS_HPP_
#define DOCUMENT_LINKS_HPP_

#include "adaptive_json5/detail/common/checked_size.hpp"
#include "adaptive_json5/detail/value/container_links.hpp"
#include "adaptive_json5/detail/value/value_record.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>
#include <utility>
#include <variant>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Attach a detached child at the end of an ordered container.
     * @par Requirements
     * P030; J5-030; J5-031; J5-039; J5-040.
     * @param values Storage belonging to one document.
     * @param root Current document root, possibly unset.
     * @param parent Index of the destination array or object.
     * @param child Index of a detached value or detached subtree.
     * @param member_name External owned name for an object, or nullptr for an array.
     * @return Success or an explicit relationship/access error.
     * @note All indices must belong to values; indices do not encode owner identity.
     * Existing relationships must have been created through validated document operations.
     * Rejects an existing parent, root reuse, self-attachment and ancestor cycles.
     * Object names are preserved exactly, including empty and repeated names.
     * Failure leaves relationships and the supplied name unchanged.
     * Success transfers the supplied name, if present.
     * Performs no allocation or recursive traversal.
     * Ancestor inspection is bounded by stored record count; P031 adds work budgets.
     */
    status_result attach_child(value_storage& values, value_index root, value_index parent, value_index child,
                               decoded_string* member_name) noexcept;
  } // namespace detail
} // namespace adaptive_json5

#endif /* DOCUMENT_LINKS_HPP_ */
