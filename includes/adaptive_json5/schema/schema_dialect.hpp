#ifndef ADAPTIVE_JSON5_SCHEMA_SCHEMA_DIALECT_HPP_
#define ADAPTIVE_JSON5_SCHEMA_SCHEMA_DIALECT_HPP_

#include <cstdint>
#include <string_view>

namespace adaptive_json5
{
  /**
   * @brief JSON Schema dialect chosen explicitly by the caller.
   * @par Requirements
   * P043; J5-053; ADR 0006 decision 2.
   * @note There is no default: compilation always receives the dialect, so a missing root
   * `$schema` never makes the library guess a draft. Only Draft-07 exists.
   */
  enum class schema_dialect : std::uint8_t
  {
    draft_07 = 1
  };

  /**
   * @brief Report whether a root `$schema` string names the given dialect.
   * @par Requirements
   * P043; J5-053; ADR 0006 decision 2.
   * @return True only for an exact match of `http://json-schema.org/draft-07/schema#` or the
   * same URI without the trailing `#`. No URI normalization is applied, so `https` and other
   * drafts do not match.
   * @par ID SWU-SCHEMA-001
   */
  [[nodiscard]] constexpr bool matches_dialect_uri(schema_dialect dialect, std::string_view uri) noexcept
  {
    constexpr std::string_view c_draft_07_uri{"http://json-schema.org/draft-07/schema#"};
    constexpr std::string_view c_draft_07_uri_without_fragment{"http://json-schema.org/draft-07/schema"};

    switch (dialect)
    {
    case schema_dialect::draft_07:
      return (uri == c_draft_07_uri) || (uri == c_draft_07_uri_without_fragment);
    default:
      return false;
    }
  }
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_SCHEMA_DIALECT_HPP_ */
