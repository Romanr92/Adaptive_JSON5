#include "adaptive_json5/parser.hpp"

#include "adaptive_json5/detail/parser/parser_engine.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"

#include <utility>

namespace adaptive_json5
{
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
                                         accounted_storage& accounting) noexcept
  {
    using document_result = result<parsed_document>;

    // Each call starts with independent cursor, frame, and budget state.
    detail::parser_engine engine(input, options, accounting);
    auto parsed = engine.run_complete();

    if (!parsed.has_value())
    {
      return document_result::failure(*parsed.error_if());
    }

    auto& tree = *parsed.value_if();

    // Transfer ownership only after the entire input has been accepted.
    return document_result::success(parsed_document{std::move(tree.values), tree.warning_count, tree.first_warning});
  }
} // namespace adaptive_json5
