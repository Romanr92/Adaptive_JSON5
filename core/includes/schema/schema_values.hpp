#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_VALUES_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_VALUES_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstddef>
#include <cstdint>

namespace adaptive_json5::detail
{
  /**
   * @brief Bits of schema_node::type_mask, one per Draft-07 primitive type name.
   * @par Requirements
   * P047; J5-057; Draft-07 Validation section 6.1.1.
   */
  enum class type_bit : std::uint8_t
  {
    null_type = 1U,
    boolean_type = 2U,
    object_type = 4U,
    array_type = 8U,
    number_type = 16U,
    string_type = 32U,
    integer_type = 64U
  };

  /**
   * @brief Parse a `type` keyword value into a mask of type_bit values.
   * @par Requirements
   * P047; J5-054; J5-057.
   * @return The non-zero mask, or invalid_schema (keyword_shape) at the value for anything but
   * a known type name or a non-empty array of distinct known type names.
   * @par ID SWU-SCHEMA-193
   */
  [[nodiscard]] result<std::uint8_t, schema_error> parse_type_keyword(const value_view& value) noexcept;

  /**
   * @brief Deep-copy a schema value (const or enum) into the compiled schema's value document.
   * @par Requirements
   * P047; J5-061; J5-066; ADR 0006 decision 4.
   * @param source Value in a registered schema document.
   * @param target Value document owned by the compiled schema.
   * @param storage Result storage for copied strings, names and numbers.
   * @param steps Compile steps used so far; one is charged per copied value.
   * @param step_limit Inclusive max_compile_steps.
   * @return The copy's index, resource_limit (max_compile_steps) or a storage failure.
   * @note Iterative, with an explicit stack in storage that is released before returning.
   * @par ID SWU-SCHEMA-194
   */
  [[nodiscard]] result<value_index, schema_error> copy_value(const value_view& source, document& target,
                                                             const storage_callbacks& storage, std::size_t& steps,
                                                             std::size_t step_limit) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_VALUES_HPP_ */
