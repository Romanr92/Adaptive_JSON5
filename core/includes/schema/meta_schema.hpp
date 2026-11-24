#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_META_SCHEMA_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_META_SCHEMA_HPP_

#include <string_view>

namespace adaptive_json5::detail
{
  /// @brief Base URI under which the embedded Draft-07 meta-schema is registered.
  inline constexpr std::string_view c_draft07_meta_schema_uri{"http://json-schema.org/draft-07/schema"};

  /**
   * @brief Return the embedded Draft-07 meta-schema text (strict JSON, provenance in the source).
   * @par Requirements
   * P054; J5-054; J5-053.
   * @par ID SWU-SCHEMA-346
   */
  [[nodiscard]] std::string_view draft07_meta_schema() noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_META_SCHEMA_HPP_ */
