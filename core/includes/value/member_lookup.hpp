#ifndef ADAPTIVE_JSON5_INTERNAL_MEMBER_LOOKUP_HPP_
#define ADAPTIVE_JSON5_INTERNAL_MEMBER_LOOKUP_HPP_
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/member_lookup.hpp"
namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Find whether an object contains one unambiguous decoded name.
     * @par Requirements
     * P030; J5-031; J5-033; J5-039; J5-040.
     * @param storage Storage belonging to the inspected document.
     * @param object Object index belonging to storage.
     * @param name Exact decoded name to find.
     * @return Explicit lookup outcome or an access/comparison failure.
     * @note Rejects arrays and scalar values, including empty arrays.
     * Preserves every member and never selects among duplicate matches.
     * Allocates nothing and does not modify the document or supplied name.
     * Requires relationships maintained by validated document operations.
     * Work is linear in visited members plus examined name units.
     * P031 adds configurable work accounting.
     */
    [[nodiscard]] result<member_lookup> lookup_member(const value_storage& storage, value_index object,
                                                      const decoded_string& name) noexcept;
  } // namespace detail
} // namespace adaptive_json5
#endif
