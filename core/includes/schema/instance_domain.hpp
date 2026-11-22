#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_INSTANCE_DOMAIN_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_INSTANCE_DOMAIN_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/document.hpp"
#include "schema/schema_work.hpp"

namespace adaptive_json5::detail
{
  /**
   * @brief Check that an instance is a JSON-compatible value before any assertion runs.
   * @par Requirements
   * P043; J5-048; J5-059; J5-061; J5-065; ADR 0006 decision 6.
   * @return success, or unsupported_instance with the reason and the source range of the
   * first offending item in document order: a repeated decoded member name
   * (duplicate_name, range of the repeated name), a NaN/Infinity number
   * (non_finite_number) or a string or member name with an unpaired surrogate
   * (unpaired_surrogate). Operational failures: resource_limit (max_depth) when containers
   * nest deeper than budget().max_depth, cancelled and resource_limit (max_work_steps) from
   * work.step(), and storage failures translated by schema_error::from_storage().
   * @note Iterative, with an explicit container stack allocated from work_storage and
   * released before returning; no allocation for a scalar root. One step is charged per
   * visited value and per member-name comparison. Duplicate detection compares every name
   * pair of an object (quadratic, bounded by max_work_steps). The document is not changed.
   * @par ID SWU-SCHEMA-020
   */
  [[nodiscard]] result<void, schema_error> check_instance_domain(const document& instance, schema_work& work,
                                                                 const storage_callbacks& work_storage) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_INSTANCE_DOMAIN_HPP_ */
