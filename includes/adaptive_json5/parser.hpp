#ifndef PARSER_HPP_
#define PARSER_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Own a successfully parsed complete document and warning metadata.
   * @par Requirements
   * P037; J5-026; J5-040; J5-051.
   * @note values has an established root.
   * The input bytes need not remain alive after parsing succeeds.
   * The originating accounting object and its provider must outlive values.
   * Counts all warnings while retaining only the first descriptor.
   */
  struct parsed_document
  {
      document values;
      std::size_t warning_count{};
      std::optional<string_warning> first_warning{};
  };

  /**
   * @brief Parse exactly one root value surrounded by mode-valid trivia.
   * @par Requirements
   * P037; J5-026; J5-040; J5-047; J5-049; J5-051.
   * @param input Borrowed bytes, alive and unchanged throughout this call.
   * @param options Syntax, duplicate policy and resource limits.
   * @param accounting Caller-owned storage accounting at a stable address.
   * @return Owned complete document or an explicit failure.
   * @note Rejects empty input, trivia-only input and trailing nontrivia.
   * Any admitted value type may be the root.
   * accounting.byte_limit() must equal options.limits.max_owned_bytes.
   * accounting and its provider must outlive the returned document.
   * Existing allocations through accounting share its byte ceiling.
   * Failure publishes no document and releases this call's allocations.
   * Each call uses fresh parsing state; accounting remains reusable.
   * No warning callbacks are invoked.
   */
  result<parsed_document> parse_document(input_view input, const parse_options& options,
                                         accounted_storage& accounting) noexcept;
} // namespace adaptive_json5

#endif /* PARSER_HPP_ */
