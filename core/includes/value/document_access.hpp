#ifndef DOCUMENT_ACCESS_HPP_
#define DOCUMENT_ACCESS_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Internal construction and mutation of documents (ADR 0007).
     * @par Requirements
     * P033; P034; P035; P036; J5-030; J5-031; J5-040; J5-041; J5-048.
     * @note Not a supported public API; never installed.
     */
    struct document_access
    {
        /**
         * @brief Append a completed value to a detached array without ancestor traversal.
         * @return Success or an explicit relationship/access error; failure changes nothing.
         * @par ID SWU-VALUE-042
         */
        [[nodiscard]] static status_result attach_parsed_element(document& values, value_index array,
                                                                 value_index child) noexcept;

        /**
         * @brief Append a completed named value to a detached object without ancestor traversal.
         * @return Success or an explicit relationship/access/range error.
         * @note Success transfers name; failure leaves it unchanged.
         * @par ID SWU-VALUE-043
         */
        [[nodiscard]] static status_result attach_parsed_member(document& values, value_index object,
                                                                decoded_string&& name, value_index child,
                                                                source_span name_span, std::size_t input_size) noexcept;

        /**
         * @brief Reserve exactly the counted number of records in an empty document.
         * @return Success, or value_storage::reserve_exact's failure with the document unchanged.
         * @note Afterwards appends beyond records fail instead of growing (ADR 0003, decision 7).
         * @par ID SWU-VALUE-044
         */
        [[nodiscard]] static status_result reserve_records_exact(document& values, std::size_t records) noexcept;

        /**
         * @brief Construct an empty document; construction is internal (ADR 0007).
         * @par Requirements
         * P030; J5-028; J5-040; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @note Allocates nothing and initially has no root.
         * @par ID SWU-VALUE-158
         */
        [[nodiscard]] static document create(const storage_callbacks& storage, std::size_t max_records) noexcept;

        /**
         * @brief Create a detached null value.
         * @par Requirements
         * P030; J5-028; J5-046; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-159
         */
        [[nodiscard]] static result<value_index> make_null(document& values) noexcept;

        /**
         * @brief Create a detached Boolean value.
         * @par Requirements
         * P030; J5-028; J5-046; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-160
         */
        [[nodiscard]] static result<value_index> make_boolean(document& values, bool value) noexcept;

        /**
         * @brief Transfer decoded content into a detached string value.
         * @par Requirements
         * P030; J5-028; J5-040; J5-046; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @note Consumes value on entry, even if record allocation fails.
         * @par ID SWU-VALUE-161
         */
        [[nodiscard]] static result<value_index> make_string(document& values, decoded_string value) noexcept;

        /**
         * @brief Transfer validated numeric spelling into a detached numeric value.
         * @par Requirements
         * P030; J5-028; J5-034; J5-038; J5-040; J5-046; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @note Consumes value on entry, even if record allocation fails.
         * @par ID SWU-VALUE-162
         */
        [[nodiscard]] static result<value_index> make_number(document& values, owned_number value) noexcept;

        /**
         * @brief Create a detached empty array.
         * @par Requirements
         * P030; J5-028; J5-030; J5-046; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-163
         */
        [[nodiscard]] static result<value_index> make_array(document& values) noexcept;

        /**
         * @brief Create a detached empty object.
         * @par Requirements
         * P030; J5-028; J5-031; J5-046; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-164
         */
        [[nodiscard]] static result<value_index> make_object(document& values) noexcept;

        /**
         * @brief Append a detached value or subtree to an array, checking ancestors.
         * @par Requirements
         * P030; J5-030; J5-039; J5-040; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-165
         */
        [[nodiscard]] static status_result append_element(document& values, value_index array,
                                                          value_index child) noexcept;

        /**
         * @brief Append a named detached value or subtree to an object, checking ancestors.
         * @par Requirements
         * P030; J5-031; J5-039; J5-040; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @note Success transfers name; failure leaves it unchanged.
         * @par ID SWU-VALUE-166
         */
        [[nodiscard]] static status_result append_member(document& values, value_index object, decoded_string&& name,
                                                         value_index child) noexcept;

        /**
         * @brief Append a named detached value with its name-token range, checking ancestors.
         * @par Requirements
         * P036; J5-031; J5-032; J5-039; J5-040; J5-041; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @note Success transfers name and records its span; failure changes nothing.
         * @par ID SWU-VALUE-167
         */
        [[nodiscard]] static status_result append_member(document& values, value_index object, decoded_string&& name,
                                                         value_index child, source_span name_span,
                                                         std::size_t input_size) noexcept;

        /**
         * @brief Assign or replace a value's original source range.
         * @par Requirements
         * P032; J5-039; J5-041; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-168
         */
        [[nodiscard]] static status_result set_source_span(document& values, value_index index, source_span span,
                                                           std::size_t input_size) noexcept;

        /**
         * @brief Assign or replace an object member name's original source range.
         * @par Requirements
         * P036; J5-032; J5-039; J5-041; J5-044; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-169
         */
        [[nodiscard]] static status_result set_member_name_source_span(document& values, value_index index,
                                                                       source_span span,
                                                                       std::size_t input_size) noexcept;

        /**
         * @brief Establish one detached value as the document root.
         * @par Requirements
         * P030; J5-028; J5-039; ADR 0007.
         * @return The result of the private document member, unchanged.
         * @par ID SWU-VALUE-170
         */
        [[nodiscard]] static status_result set_root(document& values, value_index index) noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* DOCUMENT_ACCESS_HPP_ */
