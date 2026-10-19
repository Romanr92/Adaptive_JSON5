#ifndef VALUE_RECORD_HPP_
#define VALUE_RECORD_HPP_

#include "adaptive_json5/detail/value/container_links.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <optional>
#include <variant>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Store exactly one owned value payload or container description.
     * @par Requirements
     * P030; J5-028; J5-030; J5-031; J5-040.
     * @note std::monostate represents an actual null value.
     * Numbers and strings own their data through caller-provided storage.
     * Array/object alternatives contain links, not recursively owned children.
     * Copying is unavailable because owned scalar alternatives are move-only.
     */
    using value_payload = std::variant<std::monostate, bool, decoded_string, owned_number, array_data, object_data>;

    /**
     * @brief Store one value and its document-relative relationship metadata.
     * @par Requirements
     * P030; P032; J5-028; J5-030; J5-031; J5-040; J5-041.
     * @note Internal representation; document operations enforce invariants.
     * Default construction creates a detached null record without allocation.
     * parent is unset for a root or detached record.
     * next_sibling is unset for a detached record or the last child.
     * Object children have an owned member_name, including empty names.
     * Array children and roots have no member_name.
     * Source spans are optional for manually constructed values.
     * A member-name source span requires a member_name.
     * Destruction releases this record's data without following child links.
     * Moving transfers owned data; moved-from records must not be traversed.
     */
    struct value_record
    {
        value_payload payload{};
        value_index parent{};
        value_index next_sibling{};
        std::optional<decoded_string> member_name{};
        std::optional<adaptive_json5::source_span> source_span{};
        std::optional<adaptive_json5::source_span> member_name_source_span{};
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* VALUE_RECORD_HPP_ */
