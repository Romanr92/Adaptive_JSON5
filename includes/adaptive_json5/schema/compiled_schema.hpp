#ifndef ADAPTIVE_JSON5_SCHEMA_COMPILED_SCHEMA_HPP_
#define ADAPTIVE_JSON5_SCHEMA_COMPILED_SCHEMA_HPP_

#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_budget.hpp"
#include "adaptive_json5/schema/schema_dialect.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/schema_registry.hpp"
#include "adaptive_json5/value/document.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace adaptive_json5
{
  namespace detail
  {
    struct compiled_schema_access;
  } // namespace detail

  /**
   * @brief Compilation settings and storage.
   * @par Requirements
   * P044; J5-046; J5-047; J5-049; ADR 0006 decisions 4 and 5, O7.
   * @note result_storage holds the compiled schema for its whole lifetime; scratch_storage
   * is used during compilation only and fully released before compile_schema() returns.
   * Both must be usable providers.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.20)
  struct [[nodiscard]] compile_options
  {
      schema_compile_limits limits{};
      storage_callbacks result_storage{};
      storage_callbacks scratch_storage{};
  };

  /**
   * @brief Immutable, move-only compiled schema graph.
   * @par Requirements
   * P044; J5-055; J5-066; ADR 0006 decisions 4 and 10.
   * @note Owns a flat node array with every `$ref` resolved to a node index, plus the
   * member names it needs, all in compile_options::result_storage, which must outlive it.
   * It keeps no reference to the registry or the schema texts. Concurrent read-only use
   * (for example validations with separate work storage) is safe.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.19)
  class [[nodiscard]] compiled_schema
  {
    private:
      owned_block m_nodes{};
      owned_block m_edges{};
      owned_block m_properties{};
      owned_block m_units{};
      std::size_t m_node_count{};
      std::size_t m_edge_count{};
      std::size_t m_property_count{};
      std::size_t m_unit_count{};
      std::size_t m_resource_count{};
      std::size_t m_root{};
      std::optional<document> m_values{}; // Copied const/enum values (P047).

      compiled_schema() noexcept = default;

      friend struct detail::compiled_schema_access;

    public:
      /**
       * @brief Take over another compiled schema; other becomes empty.
       * @par Requirements
       * P044; J5-066.
       * @note The moved-from schema reports zero nodes; validate() rejects it with
       * internal_error instead of reading released tables.
       * @par ID SWU-SCHEMA-161
       */
      compiled_schema(compiled_schema&& other) noexcept;

      /**
       * @brief Release the graph through its result storage.
       * @par Requirements
       * P044; J5-040.
       */
      ~compiled_schema() noexcept = default;

      compiled_schema(const compiled_schema&) = delete;
      compiled_schema& operator=(const compiled_schema&) = delete;
      compiled_schema& operator=(compiled_schema&&) = delete;

      /**
       * @brief Return the number of compiled schema nodes.
       * @par Requirements
       * P044; J5-047.
       * @par ID SWU-SCHEMA-078
       */
      [[nodiscard]] std::size_t node_count() const noexcept;

      /**
       * @brief Return the number of registry resources compiled.
       * @par Requirements
       * P044; J5-055.
       * @par ID SWU-SCHEMA-079
       */
      [[nodiscard]] std::size_t resource_count() const noexcept;

      /**
       * @brief Return the total number of unknown keywords across all nodes.
       * @par Requirements
       * P044; J5-062; ADR 0006 decision 10.
       * @note Tooling may warn about likely misspellings; unknown keywords never assert.
       * @par ID SWU-SCHEMA-080
       */
      [[nodiscard]] std::size_t unknown_keyword_count() const noexcept;
  };

  /**
   * @brief Compile every registered resource and select the root schema.
   * @par Requirements
   * P044; J5-047; J5-053; J5-054; J5-055; J5-056; J5-066; J5-067; ADR 0006 decisions 2, 5,
   * 8, 9, 10, O1, O2 and O7.
   * @param registry Registered resources; may be destroyed after the call.
   * @param root_uri Absolute URI of the root schema: a registered base URI or an
   * identifier, optionally with a fragment (JSON Pointer or plain name).
   * @param dialect Explicitly chosen dialect; root `$schema` of every resource must match it.
   * @param options Limits and storage.
   * @return The compiled schema, or (compile phase): unsupported_dialect;
   * invalid_schema (keyword_shape) at the offending value; unresolved_reference at the
   * `$ref` (or for root_uri); nonprogressing_reference for a pure `$ref` cycle;
   * resource_limit (max_schema_nodes or max_compile_steps); storage failures.
   * @note Recognized subschema keywords are compiled so pointers into them resolve.
   * Siblings of `$ref`, including `$id`, are ignored for identifiers and evaluation.
   * Pointer fragments decode `%XX`, `~1` and `~0`. No I/O.
   * @par ID SWU-SCHEMA-081
   */
  [[nodiscard]] result<compiled_schema, schema_error> compile_schema(const schema_registry& registry,
                                                                     std::string_view root_uri, schema_dialect dialect,
                                                                     const compile_options& options) noexcept;
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_COMPILED_SCHEMA_HPP_ */
