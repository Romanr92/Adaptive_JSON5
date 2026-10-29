#ifndef JSON5_DOCUMENT_ACCESS_HPP_
#define JSON5_DOCUMENT_ACCESS_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Build json5_document objects for the storage paths of the library.
     * @par Requirements
     * P037; J5-040; J5-051.
     * @note Private: shared by the caller-buffer path and the optional hosted library.
     */
    struct json5_document_access
    {
        /**
         * @brief Parse one complete document through the storage of an owner.
         * @par Requirements
         * P037; J5-026; J5-040; J5-046; J5-051.
         * @param input Borrowed input bytes.
         * @param options Syntax, policies and limits, passed unchanged to parse_document.
         * @param storage Owner of a live control block; accounting() must not be nullptr.
         * @return The document owning storage, or the error of parse_document, unchanged.
         * @note On failure the parser has released its allocations before storage is
         * destroyed.
         */
        [[nodiscard]] static result<json5_document> parse_with_storage(input_view input, const parse_options& options,
                                                                       document_storage_owner&& storage) noexcept;
    };
  } // namespace detail
} // namespace adaptive_json5

#endif /* JSON5_DOCUMENT_ACCESS_HPP_ */
