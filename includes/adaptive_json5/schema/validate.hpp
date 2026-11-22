#ifndef ADAPTIVE_JSON5_SCHEMA_VALIDATE_HPP_
#define ADAPTIVE_JSON5_SCHEMA_VALIDATE_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/validation_options.hpp"
#include "adaptive_json5/schema/validation_report.hpp"
#include "adaptive_json5/value/document.hpp"

#include <cstddef>

namespace adaptive_json5
{
  /**
   * @brief Validate a parsed instance against a compiled schema.
   * @par Requirements
   * P046; J5-048; J5-049; J5-050; J5-053; J5-056; J5-059; J5-061; J5-063; J5-065; ADR 0006
   * decisions 3, 4, 5, 6, 7 and 12.
   * @return The report (valid or invalid with diagnostics), or (validate phase):
   * unsupported_instance from the pre-pass; nonprogressing_reference when the same
   * (schema node, instance value) pair recurs on the active `$ref` chain; cancelled;
   * resource_limit (max_depth, max_reference_steps, max_branch_evaluations,
   * max_work_steps); storage failures. An operational failure always wins: no partial
   * report is returned.
   * @note Evaluated keywords (P046): boolean schemas, `$ref` (siblings ignored), allOf,
   * anyOf, oneOf, not, if/then/else, properties and items (schema or array form). Other
   * keywords are compiled and evaluated from P047 on. Uses an explicit frame stack in
   * options.work_storage, released before returning; max_depth bounds the stack. The
   * compiled schema is read only, so validations with separate work storage may run
   * concurrently.
   * @par ID SWU-SCHEMA-139
   */
  [[nodiscard]] result<validation_report, schema_error>
  validate(const compiled_schema& schema, const document& instance, const validation_options& options) noexcept;

  /**
   * @brief Write a diagnostic's schema location as a URI fragment JSON Pointer.
   * @par Requirements
   * P046; J5-064; RFC 6901; ADR 0006 decision 13.
   * @param buffer Destination; nothing past capacity is written and no terminator is added.
   * @return The number of bytes the full location needs, for example 9 for `#/allOf/0`.
   * The pointer is relative to the diagnostic's schema_resource.
   * @par ID SWU-SCHEMA-140
   */
  [[nodiscard]] std::size_t format_schema_location(const compiled_schema& schema, const schema_diagnostic& diagnostic,
                                                   char* buffer, std::size_t capacity) noexcept;

  /**
   * @brief Write a diagnostic's instance location as an RFC 6901 JSON Pointer.
   * @par Requirements
   * P046; J5-064; RFC 6901; ADR 0006 decision 13.
   * @param instance The document that was validated; it must be the same, unchanged document.
   * @param buffer Destination; nothing past capacity is written and no terminator is added.
   * @param scratch Storage for the search stack and name conversion, released before returning.
   * @return The number of bytes the full pointer needs ("" for the root, for example
   * `/a~1b/0`), internal_error when instance_position names no value of instance, or a
   * storage failure.
   * @note Searches the document from the root (linear in its size) and writes member names
   * as UTF-8 with `~` and `/` escaped.
   * @par ID SWU-SCHEMA-164
   */
  [[nodiscard]] result<std::size_t, schema_error> format_instance_location(const document& instance,
                                                                           const schema_diagnostic& diagnostic,
                                                                           char* buffer, std::size_t capacity,
                                                                           const storage_callbacks& scratch) noexcept;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_VALIDATE_HPP_ */
