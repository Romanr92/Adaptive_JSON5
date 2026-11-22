#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_GRAPH_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_GRAPH_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace adaptive_json5::detail
{
  /**
   * @brief Marker for an absent node index.
   * @par Requirements
   * P044; ADR 0006 decision 10.
   */
  inline constexpr std::size_t c_no_node = std::numeric_limits<std::size_t>::max();

  /**
   * @brief How the step from a parent node to a child is spelled.
   * @par Requirements
   * P044; J5-064; ADR 0006 decision 10.
   */
  enum class step_kind : std::uint8_t
  {
    none, // Schema root, or a keyword holding one schema (not, items, ...).
    name, // Keyword holding a map of schemas (properties, definitions, ...).
    index // Keyword holding an array of schemas (allOf, items, ...).
  };

  /**
   * @brief Consecutive range in one of the graph's tables.
   * @par Requirements
   * P044.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.4)
  struct node_range
  {
      std::size_t first{};
      std::size_t count{};
  };

  /**
   * @brief One compiled schema (boolean or object).
   * @par Requirements
   * P044; P046; J5-055; J5-056; J5-064; ADR 0006 decision 10.
   * @note Direct children are allocated consecutively (first_child, child_count). The
   * evaluation fields (ref, combinator ranges, ...) are left empty on a `$ref` node,
   * whose siblings are ignored. Names are UTF-16 ranges in the unit table.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.3)
  struct schema_node
  {
      std::size_t parent{c_no_node};
      std::size_t first_child{};
      std::size_t child_count{};
      schema_keyword step_keyword{schema_keyword::none};
      step_kind step{step_kind::none};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.8)
      node_range step_name{};
      std::size_t step_index{};
      std::size_t resource{};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.7)
      source_span span{};
      bool has_span{};
      bool is_boolean{};
      bool boolean_value{};
      std::size_t ref{c_no_node};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.6)
      node_range all_of{};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.5)
      node_range any_of{};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.4)
      node_range one_of{};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.3)
      node_range items_array{};
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.2)
      node_range properties{};
      std::size_t not_schema{c_no_node};
      std::size_t if_schema{c_no_node};
      std::size_t then_schema{c_no_node};
      std::size_t else_schema{c_no_node};
      std::size_t items_single{c_no_node};
      std::size_t unknown_keywords{};
  };

  /**
   * @brief A `properties` entry: member name (UTF-16 range) and its schema node.
   * @par Requirements
   * P046; J5-057.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
  // they need; the implicitly provided members are the intended ones.
  // codeql::misra_deviation_next_line(DEV-104.2)
  struct property_edge
  {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.1)
      node_range name{};
      std::size_t node{c_no_node};
  };

  /**
   * @brief Construction and read access to a compiled schema's tables.
   * @par Requirements
   * P044; P046.
   */
  struct compiled_schema_access
  {
      /**
       * @brief Build a compiled schema from finished tables.
       * @par Requirements
       * P044; J5-066.
       * @par ID SWU-SCHEMA-082
       */
      [[nodiscard]] static compiled_schema make(owned_block&& nodes, std::size_t node_count, owned_block&& edges,
                                                std::size_t edge_count, owned_block&& properties,
                                                std::size_t property_count, owned_block&& units, std::size_t unit_count,
                                                std::size_t resource_count, std::size_t root) noexcept;

      /**
       * @brief Borrow node index; requires index < node_count().
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-083
       */
      [[nodiscard]] static const schema_node& node(const compiled_schema& schema, std::size_t index) noexcept;

      /**
       * @brief Return edge table entry index (a node index).
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-084
       */
      [[nodiscard]] static std::size_t edge(const compiled_schema& schema, std::size_t index) noexcept;

      /**
       * @brief Borrow property table entry index.
       * @par Requirements
       * P046.
       * @par ID SWU-SCHEMA-085
       */
      [[nodiscard]] static const property_edge& property(const compiled_schema& schema, std::size_t index) noexcept;

      /**
       * @brief Return UTF-16 unit index of the name table.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-086
       */
      [[nodiscard]] static std::uint16_t unit(const compiled_schema& schema, std::size_t index) noexcept;

      /**
       * @brief Return the root node index.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-087
       */
      [[nodiscard]] static std::size_t root(const compiled_schema& schema) noexcept;
  };
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_GRAPH_HPP_ */
