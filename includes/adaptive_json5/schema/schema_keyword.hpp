#ifndef ADAPTIVE_JSON5_SCHEMA_SCHEMA_KEYWORD_HPP_
#define ADAPTIVE_JSON5_SCHEMA_SCHEMA_KEYWORD_HPP_

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

  /**
   * @brief Return the JSON spelling of a keyword.
   * @par Requirements
   * P044; J5-064.
   * @return The keyword name; empty for none and false_schema and for unknown values.
   * @par ID SWU-SCHEMA-065
   */
  [[nodiscard]] constexpr std::string_view keyword_name(schema_keyword keyword) noexcept
  {
    switch (keyword)
    {
    case schema_keyword::schema:
      return "$schema";
    case schema_keyword::id:
      return "$id";
    case schema_keyword::ref:
      return "$ref";
    case schema_keyword::comment:
      return "$comment";
    case schema_keyword::title:
      return "title";
    case schema_keyword::description:
      return "description";
    case schema_keyword::default_value:
      return "default";
    case schema_keyword::read_only:
      return "readOnly";
    case schema_keyword::write_only:
      return "writeOnly";
    case schema_keyword::examples:
      return "examples";
    case schema_keyword::multiple_of:
      return "multipleOf";
    case schema_keyword::maximum:
      return "maximum";
    case schema_keyword::exclusive_maximum:
      return "exclusiveMaximum";
    case schema_keyword::minimum:
      return "minimum";
    case schema_keyword::exclusive_minimum:
      return "exclusiveMinimum";
    case schema_keyword::max_length:
      return "maxLength";
    case schema_keyword::min_length:
      return "minLength";
    case schema_keyword::pattern:
      return "pattern";
    case schema_keyword::additional_items:
      return "additionalItems";
    case schema_keyword::items:
      return "items";
    case schema_keyword::max_items:
      return "maxItems";
    case schema_keyword::min_items:
      return "minItems";
    case schema_keyword::unique_items:
      return "uniqueItems";
    case schema_keyword::contains:
      return "contains";
    case schema_keyword::max_properties:
      return "maxProperties";
    case schema_keyword::min_properties:
      return "minProperties";
    case schema_keyword::required:
      return "required";
    case schema_keyword::additional_properties:
      return "additionalProperties";
    case schema_keyword::definitions:
      return "definitions";
    case schema_keyword::properties:
      return "properties";
    case schema_keyword::pattern_properties:
      return "patternProperties";
    case schema_keyword::dependencies:
      return "dependencies";
    case schema_keyword::property_names:
      return "propertyNames";
    case schema_keyword::const_value:
      return "const";
    case schema_keyword::enum_values:
      return "enum";
    case schema_keyword::type:
      return "type";
    case schema_keyword::format:
      return "format";
    case schema_keyword::content_media_type:
      return "contentMediaType";
    case schema_keyword::content_encoding:
      return "contentEncoding";
    case schema_keyword::if_schema:
      return "if";
    case schema_keyword::then_schema:
      return "then";
    case schema_keyword::else_schema:
      return "else";
    case schema_keyword::all_of:
      return "allOf";
    case schema_keyword::any_of:
      return "anyOf";
    case schema_keyword::one_of:
      return "oneOf";
    case schema_keyword::not_schema:
      return "not";
    case schema_keyword::none:
    case schema_keyword::false_schema:
    default:
      return std::string_view{};
    }
  }
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_SCHEMA_KEYWORD_HPP_ */
