#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_ASSERTIONS_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_ASSERTIONS_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_work.hpp"

#include <cstdint>

namespace adaptive_json5::detail
{
  /**
   * @brief Set of failed keywords: bit n is set when schema_keyword n failed.
   * @par Requirements
   * P047; J5-057; J5-063.
   */
  using keyword_mask = std::uint64_t;

  /**
   * @brief Return the keyword_mask bit of a keyword.
   * @par Requirements
   * P047; J5-057.
   * @par ID SWU-SCHEMA-206
   */
  [[nodiscard]] constexpr keyword_mask keyword_bit(schema_keyword keyword) noexcept
  {
    return keyword_mask{1U} << static_cast<std::uint32_t>(keyword);
  }

  /**
   * @brief Evaluate a node's assertion keywords (those without subschemas) on one instance value.
   * @par Requirements
   * P047; J5-057; J5-059; J5-061; Draft-07 Validation section 6.
   * @param scratch Work storage for equality and number analysis, released before returning.
   * @return The keywords that failed (zero when all passed), or an operational failure.
   * @note P047: type (value-based integer), enum, const. Keywords that do not apply to the
   * instance's type pass. Every evaluated keyword is checked, also after a failure.
   * @par ID SWU-SCHEMA-207
   */
  [[nodiscard]] result<keyword_mask, schema_error> check_assertions(const compiled_schema& schema,
                                                                    const schema_node& node, const value_view& instance,
                                                                    schema_work& work,
                                                                    const storage_callbacks& scratch) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_ASSERTIONS_HPP_ */
