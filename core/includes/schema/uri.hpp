#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_URI_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_URI_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "schema/pod_vector.hpp"

#include <string_view>

namespace adaptive_json5::detail
{
  /**
   * @brief Components of a URI reference split per RFC 3986 appendix B.
   * @par Requirements
   * P044; J5-055; ADR 0006 decision 8.
   * @note Views into the split text; the has_ flags tell an empty component from an
   * absent one.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.7)
  struct uri_parts
  {
      std::string_view scheme{};
      std::string_view authority{};
      std::string_view path{};
      std::string_view query{};
      std::string_view fragment{};
      bool has_scheme{};
      bool has_authority{};
      bool has_query{};
      bool has_fragment{};
  };

  /**
   * @brief Split a URI reference into its five components.
   * @par Requirements
   * P044; J5-055.
   * @note Never fails: every string is a URI reference under the appendix B grammar.
   * @par ID SWU-SCHEMA-047
   */
  [[nodiscard]] uri_parts split_uri(std::string_view text) noexcept;

  /**
   * @brief Resolve a reference against an absolute base URI (RFC 3986 section 5.2).
   * @par Requirements
   * P044; J5-055; ADR 0006 decision 8.
   * @param base Absolute URI (has a scheme); its fragment is ignored.
   * @param reference Reference to resolve; an absolute reference is only normalized.
   * @param out Receives the target, appended after its current contents.
   * @param scratch Storage for the merged path, released before returning.
   * @return success, internal_error when base is not absolute, or a storage failure.
   * @note Normalization: scheme and host lowercased, dot segments removed, an empty
   * fragment written as no fragment. Percent-encoding is left unchanged.
   * @par ID SWU-SCHEMA-048
   */
  [[nodiscard]] result<void, schema_error> resolve_uri(std::string_view base, std::string_view reference,
                                                       pod_vector<char>& out,
                                                       const storage_callbacks& scratch) noexcept;

  /**
   * @brief Return the text before the first '#', the URI without its fragment.
   * @par Requirements
   * P044; J5-055.
   * @par ID SWU-SCHEMA-049
   */
  [[nodiscard]] std::string_view without_fragment(std::string_view uri) noexcept;

  /**
   * @brief Borrow a pod_vector<char> as text.
   * @par Requirements
   * P044.
   * @par ID SWU-SCHEMA-050
   */
  [[nodiscard]] std::string_view text_of(const pod_vector<char>& text, std::size_t offset, std::size_t length) noexcept;

  /**
   * @brief Append text to a pod_vector<char>.
   * @par Requirements
   * P044; J5-046.
   * @par ID SWU-SCHEMA-051
   */
  [[nodiscard]] result<void, schema_error> append_text(pod_vector<char>& out, std::string_view text) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_URI_HPP_ */
