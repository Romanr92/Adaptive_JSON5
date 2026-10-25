#ifndef DOCUMENT_ACCESS_HPP_
#define DOCUMENT_ACCESS_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Internal parser operations that need a document's private storage.
     * @par Requirements
     * P033; P034; P035; P036; J5-030; J5-031; J5-040; J5-041; J5-048.
     * @note Not a supported public API; never installed.
     */
    struct document_access
    {
        /**
         * @brief Append a completed value to a detached array without ancestor traversal.
         * @return Success or an explicit relationship/access error; failure changes nothing.
         */
        [[nodiscard]] static status_result attach_parsed_element(document& values, value_index array,
                                                                 value_index child) noexcept;

        /**
         * @brief Append a completed named value to a detached object without ancestor traversal.
         * @return Success or an explicit relationship/access/range error.
         * @note Success transfers name; failure leaves it unchanged.
         */
        [[nodiscard]] static status_result attach_parsed_member(document& values, value_index object,
                                                                decoded_string&& name, value_index child,
                                                                source_span name_span, std::size_t input_size) noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* DOCUMENT_ACCESS_HPP_ */
