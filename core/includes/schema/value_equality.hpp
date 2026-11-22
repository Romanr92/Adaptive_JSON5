#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_VALUE_EQUALITY_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_VALUE_EQUALITY_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/schema_work.hpp"

namespace adaptive_json5::detail
{
  /**
   * @brief Type-aware deep equality of two JSON values (Draft-07 instance equality).
   * @par Requirements
   * P045; J5-059; J5-061; ADR 0006 decision 11.
   * @param left Value from any document.
   * @param right Value from any document.
   * @param work Budget; one equality_step() per visited pair, one work step per member-name
   * comparison.
   * @param scratch Storage for the explicit pair stack and exact number comparison,
   * released before returning.
   * @return The verdict, or an operational failure (budget, cancellation, storage).
   * @note Values of different kinds are never equal (booleans are not numbers). Numbers
   * compare their exact mathematical value, so 1, 1.0, 1e0 and 0x1 are equal. Arrays
   * compare element by element in order. Objects compare member counts, then look up each
   * member by decoded name, so member order is irrelevant; names are assumed unique (the
   * instance pre-pass and strict schema parsing guarantee it). No recursion.
   * @par ID SWU-SCHEMA-127
   */
  [[nodiscard]] result<bool, schema_error> values_equal(const value_view& left, const value_view& right,
                                                        schema_work& work, const storage_callbacks& scratch) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_VALUE_EQUALITY_HPP_ */
