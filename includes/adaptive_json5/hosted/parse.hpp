#ifndef HOSTED_PARSE_HPP_
#define HOSTED_PARSE_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

namespace adaptive_json5
{
  namespace hosted
  {
    /**
     * @brief Parse a complete document with heap storage and default or given options.
     * @par Requirements
     * P037; J5-026; J5-040; J5-046; J5-047; J5-051.
     * @param input Borrowed bytes, alive and unchanged throughout this call.
     * @param options Syntax, duplicate policy and limits; defaults to parse_options{}.
     * @return A json5_document that owns its storage, or an explicit failure.
     * @note Same parsing, limits and errors as parse_document. Storage comes from the
     * global heap through ::operator new(std::nothrow); max_owned_bytes still bounds it.
     * An unavailable heap block reports allocation_failure without a location.
     * Optional hosted component (AdaptiveJSON5::hosted); not part of the embedded parser
     * target (J5-101). Thread safety as parse_document: each call has its own storage.
     */
    [[nodiscard]] result<json5_document> parse(input_view input, const parse_options& options = {}) noexcept;
  } // namespace hosted
} // namespace adaptive_json5

#endif /* HOSTED_PARSE_HPP_ */
