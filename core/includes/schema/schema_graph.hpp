#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_GRAPH_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_GRAPH_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "schema/regex_program.hpp"

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
      value_index annotations{};    // P053: copied annotation and unknown keywords (an object).
      bool has_annotations{};       // P053: annotations holds at least one member.
      std::uint8_t type_mask{};     // P047: type_bit values; zero when the node has no type keyword.
      bool has_const{};             // P047: const_value holds the copied const value.
      bool has_enum{};              // P047: enum_values holds the copied enum array.
      value_index const_value{};    // Index into compiled_schema_access::values().
      value_index enum_values{};    // Index of the copied enum array.
      std::uint8_t numeric_flags{}; // P048: numeric_bit values of the numeric keywords present.
      value_index minimum{};        // Copied numbers of the P048 keywords, valid when flagged.
      value_index maximum{};
      value_index exclusive_minimum{};
      value_index exclusive_maximum{};
      value_index multiple_of{};
      std::uint8_t string_flags{}; // P049: string_bit values of the string keywords present.
      std::uint8_t format{};       // J5-069: format_kind asserted when string_bit::format is set.
      std::size_t min_length{};    // P049: code points; valid when flagged.
      std::size_t max_length{};
      std::uint8_t array_flags{}; // P050: array_bit values of the array keywords present.
      bool items_is_array{};      // P050: items has its array (tuple) form.
      std::size_t min_items{};    // P050: valid when flagged.
      std::size_t max_items{};
      std::size_t additional_items{c_no_node}; // P050: applies to elements after the items tuple.
      std::size_t contains{c_no_node};         // P050: at least one element must match.
      std::uint8_t object_flags{};             // P051: object_bit values of the object keywords present.
      std::size_t min_properties{};            // P051: valid when flagged.
      std::size_t max_properties{};
      value_index required_names{};          // P051: copied array of required names.
      value_index dependency_names{};        // P051: copied object of the array-form dependencies.
      node_range pattern_properties{0U, 0U}; // P051: property table slice; each node has name_pattern.
      node_range dependency_schemas{0U, 0U}; // P051: property table slice of schema-form dependencies.
      std::size_t additional_properties{c_no_node};
      std::size_t property_names{c_no_node};
      node_range name_pattern{0U, 0U}; // P051: regex of a patternProperties child's member name.
      node_range pattern{0U, 0U};      // P049: program range in the regex instruction table.
  };

  /**
   * @brief Bits of schema_node::numeric_flags.
   * @par Requirements
   * P048; J5-057.
   */
  enum class numeric_bit : std::uint8_t
  {
    minimum = 1U,
    maximum = 2U,
    exclusive_minimum = 4U,
    exclusive_maximum = 8U,
    multiple_of = 16U
  };

  /**
   * @brief Bits of schema_node::string_flags.
   * @par Requirements
   * P049; J5-057.
   */
  enum class string_bit : std::uint8_t
  {
    min_length = 1U,
    max_length = 2U,
    pattern = 4U,
    format = 8U // J5-069: format is asserted (compile_options::assert_format).
  };

  /**
   * @brief Bits of schema_node::array_flags.
   * @par Requirements
   * P050; J5-057.
   */
  enum class array_bit : std::uint8_t
  {
    min_items = 1U,
    max_items = 2U,
    unique_items = 4U
  };

  /**
   * @brief Bits of schema_node::object_flags.
   * @par Requirements
   * P051; J5-057.
   */
  enum class object_bit : std::uint8_t
  {
    min_properties = 1U,
    max_properties = 2U,
    required = 4U,
    dependencies = 8U
  };

  /**
   * @brief Report whether a flag byte contains a bit; the operation is done in std::uint32_t.
   * @par Requirements
   * P047; J5-057.
   * @par ID SWU-SCHEMA-310
   */
  template <typename Bit>
  [[nodiscard]] constexpr bool has_flag(std::uint8_t flags, Bit bit) noexcept
  {
    return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(bit)) != 0U;
  }

  /**
   * @brief Return a flag byte with a bit added; the operation is done in std::uint32_t.
   * @par Requirements
   * P047; J5-057.
   * @par ID SWU-SCHEMA-311
   */
  template <typename Bit>
  [[nodiscard]] constexpr std::uint8_t with_flag(std::uint8_t flags, Bit bit) noexcept
  {
    return static_cast<std::uint8_t>(static_cast<std::uint32_t>(flags) | static_cast<std::uint32_t>(bit));
  }

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
       * @brief Give a compiled schema its copied const/enum values.
       * @par Requirements
       * P047; J5-061; J5-066.
       * @par ID SWU-SCHEMA-191
       */
      static void attach_values(compiled_schema& schema, document&& values) noexcept;

      /**
       * @brief Borrow the copied const/enum values, or nullptr when the schema has none.
       * @par Requirements
       * P047; J5-061.
       * @par ID SWU-SCHEMA-192
       */
      [[nodiscard]] static const document* values(const compiled_schema& schema) noexcept;

      /**
       * @brief Give a compiled schema its regex instruction and range tables.
       * @par Requirements
       * P049; J5-060; J5-066.
       * @par ID SWU-SCHEMA-276
       */
      static void attach_regex(compiled_schema& schema, owned_block&& code, owned_block&& ranges) noexcept;

      /**
       * @brief Borrow the regex instruction and range tables.
       * @par Requirements
       * P049; J5-060.
       * @par ID SWU-SCHEMA-277
       */
      [[nodiscard]] static regex_tables regex(const compiled_schema& schema) noexcept;

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
