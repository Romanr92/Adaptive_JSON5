#ifndef ADAPTIVE_JSON5_SCHEMA_SCHEMA_KEYWORD_HPP_
#define ADAPTIVE_JSON5_SCHEMA_SCHEMA_KEYWORD_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace adaptive_json5
{
  /**
   * @brief Draft-07 keywords known to the schema component.
   * @par Requirements
   * P044; P046; J5-053; J5-062; J5-064; ADR 0006 decisions 10 and 13.
   * @note none marks a schema root (no keyword leads to it); false_schema marks a
   * diagnostic from the boolean schema false. Every other enumerator is the keyword
   * spelled by keyword_name(). Unknown keywords have no enumerator; they are counted.
   */
  enum class schema_keyword : std::uint8_t
  {
    none,
    false_schema,
    schema,
    id,
    ref,
    comment,
    title,
    description,
    default_value,
    read_only,
    write_only,
    examples,
    multiple_of,
    maximum,
    exclusive_maximum,
    minimum,
    exclusive_minimum,
    max_length,
    min_length,
    pattern,
    additional_items,
    items,
    max_items,
    min_items,
    unique_items,
    contains,
    max_properties,
    min_properties,
    required,
    additional_properties,
    definitions,
    properties,
    pattern_properties,
    dependencies,
    property_names,
    const_value,
    enum_values,
    type,
    format,
    content_media_type,
    content_encoding,
    if_schema,
    then_schema,
    else_schema,
    all_of,
    any_of,
    one_of,
    not_schema
  };

  namespace detail
  {
    /**
     * @brief JSON spelling of every schema_keyword, indexed by its value.
     * @par Requirements
     * P044; J5-064.
     * @note none and false_schema have no spelling.
     */
    inline constexpr std::array<std::string_view, 48> c_keyword_names{"",
                                                                      "",
                                                                      "$schema",
                                                                      "$id",
                                                                      "$ref",
                                                                      "$comment",
                                                                      "title",
                                                                      "description",
                                                                      "default",
                                                                      "readOnly",
                                                                      "writeOnly",
                                                                      "examples",
                                                                      "multipleOf",
                                                                      "maximum",
                                                                      "exclusiveMaximum",
                                                                      "minimum",
                                                                      "exclusiveMinimum",
                                                                      "maxLength",
                                                                      "minLength",
                                                                      "pattern",
                                                                      "additionalItems",
                                                                      "items",
                                                                      "maxItems",
                                                                      "minItems",
                                                                      "uniqueItems",
                                                                      "contains",
                                                                      "maxProperties",
                                                                      "minProperties",
                                                                      "required",
                                                                      "additionalProperties",
                                                                      "definitions",
                                                                      "properties",
                                                                      "patternProperties",
                                                                      "dependencies",
                                                                      "propertyNames",
                                                                      "const",
                                                                      "enum",
                                                                      "type",
                                                                      "format",
                                                                      "contentMediaType",
                                                                      "contentEncoding",
                                                                      "if",
                                                                      "then",
                                                                      "else",
                                                                      "allOf",
                                                                      "anyOf",
                                                                      "oneOf",
                                                                      "not"};
  } // namespace detail

  /**
   * @brief Return the JSON spelling of a keyword.
   * @par Requirements
   * P044; J5-064.
   * @return The keyword name; empty for none and false_schema and for unknown values.
   * @par ID SWU-SCHEMA-065
   */
  [[nodiscard]] constexpr std::string_view keyword_name(schema_keyword keyword) noexcept
  {
    const auto index = static_cast<std::size_t>(keyword);
    return (index < detail::c_keyword_names.size()) ? detail::c_keyword_names[index] : std::string_view{};
  }
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_SCHEMA_KEYWORD_HPP_ */
