#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_budget.hpp"
#include "adaptive_json5/schema/schema_dialect.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"
#include "adaptive_json5/schema/schema_registry.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/json_pointer.hpp"
#include "schema/pod_vector.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_registry_access.hpp"
#include "schema/uri.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    using status = result<void, schema_error>;
    using detail::c_no_node;
    using detail::node_range;
    using detail::pod_vector;
    using detail::property_edge;
    using detail::schema_node;
    using detail::step_kind;

    constexpr std::size_t c_keyword_count = static_cast<std::size_t>(schema_keyword::not_schema) + 1U;
    constexpr std::size_t c_first_named_keyword = static_cast<std::size_t>(schema_keyword::schema);

    /**
     * @brief An identifier or plain-name key and the node it names.
     * @par Requirements
     * P044; J5-055; ADR 0006 O1.
     */
    struct key_entry
    {
        node_range text{};
        std::size_t node{};
    };

    /**
     * @brief A `$ref` waiting for resolution.
     * @par Requirements
     * P044; J5-055.
     */
    struct ref_entry
    {
        std::size_t node{};
        node_range text{};
        node_range base{};
        source_span span{};
        bool has_span{};
    };

    /**
     * @brief A schema value whose node exists but whose keywords are not processed yet.
     * @par Requirements
     * P044; J5-048.
     */
    struct pending_item
    {
        value_view value{};
        std::size_t node{};
        node_range base{};
        bool resource_root{};
    };

    /**
     * @brief Tables and counters of one compilation.
     * @par Requirements
     * P044; J5-047; J5-066; ADR 0006 decisions 5 and 10, O7.
     * @note nodes, edges, properties and units use result storage; everything else uses
     * scratch storage and is released when the state is destroyed.
     */
    struct compile_state
    {
        pod_vector<schema_node> nodes;
        pod_vector<std::size_t> edges;
        pod_vector<property_edge> properties;
        pod_vector<std::uint16_t> units;
        pod_vector<pending_item> pending;
        pod_vector<char> bases;
        pod_vector<key_entry> keys;
        pod_vector<char> key_text;
        pod_vector<ref_entry> refs;
        pod_vector<char> ref_text;
        pod_vector<char> text;
        pod_vector<char> resolved;
        pod_vector<char> token;
        storage_callbacks scratch;
        schema_compile_limits limits;
        schema_dialect dialect;
        std::size_t steps{};

        /**
         * @brief Bind every table to its storage; no allocation.
         * @par Requirements
         * P044; ADR 0006 O7.
         * @par ID SWU-SCHEMA-122
         */
        compile_state(const compile_options& options, schema_dialect selected) noexcept
            : nodes(options.result_storage), edges(options.result_storage), properties(options.result_storage),
              units(options.result_storage), pending(options.scratch_storage), bases(options.scratch_storage),
              keys(options.scratch_storage), key_text(options.scratch_storage), refs(options.scratch_storage),
              ref_text(options.scratch_storage), text(options.scratch_storage), resolved(options.scratch_storage),
              token(options.scratch_storage), scratch(options.scratch_storage), limits(options.limits),
              dialect(selected)
        {
        }
    };

    /**
     * @brief Charge compile work against max_compile_steps.
     * @par Requirements
     * P044; J5-047; ADR 0006 decision 5.
     * @par ID SWU-SCHEMA-089
     */
    status charge(compile_state& state, std::size_t amount = 1U) noexcept
    {
      if (amount > (state.limits.max_compile_steps - state.steps))
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_compile_steps));
      }
      state.steps += amount;
      return status::success();
    }

    /**
     * @brief Return a value's source span, or nullopt when unknown.
     * @par Requirements
     * P044; J5-064.
     * @par ID SWU-SCHEMA-090
     */
    std::optional<source_span> span_of(const value_view& value) noexcept
    {
      const auto span = value.source_span();
      return span.has_value() ? *span.value_if() : std::nullopt;
    }

    /**
     * @brief Return a keyword-shape failure at value.
     * @par Requirements
     * P044; J5-054.
     * @par ID SWU-SCHEMA-091
     */
    status shape_error(const value_view& value) noexcept
    {
      return status::failure(schema_error::invalid(invalid_schema_reason::keyword_shape, span_of(value)));
    }

    /**
     * @brief Translate a document access failure.
     * @par Requirements
     * P044; J5-043.
     * @par ID SWU-SCHEMA-092
     */
    status access_error(const error* failure) noexcept
    {
      return (failure == nullptr) ? status::failure(schema_error::of(schema_error_code::internal_error))
                                  : status::failure(schema_error::from_storage(*failure));
    }

    /**
     * @brief Return a value's kind, or nullopt after an access failure.
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-093
     */
    std::optional<value_kind> kind_of(const value_view& value) noexcept
    {
      const auto kind = value.kind();
      return kind.has_value() ? std::optional<value_kind>{*kind.value_if()} : std::nullopt;
    }

    /**
     * @brief Report whether a decoded name spells an ASCII keyword.
     * @par Requirements
     * P044; J5-062.
     * @par ID SWU-SCHEMA-094
     */
    bool spells(const decoded_string& name, std::string_view keyword) noexcept
    {
      if (name.size() != keyword.size())
      {
        return false;
      }
      for (std::size_t index = 0U; index < keyword.size(); ++index)
      {
        const auto unit = name.unit_at(index);
        if (!unit.has_value() || (*unit.value_if() != static_cast<std::uint16_t>(keyword[index])))
        {
          return false;
        }
      }
      return true;
    }

    /**
     * @brief Classify a member name as a known keyword.
     * @par Requirements
     * P044; J5-062.
     * @return The keyword, or nullopt for an unknown keyword.
     * @par ID SWU-SCHEMA-095
     */
    std::optional<schema_keyword> classify(const decoded_string& name) noexcept
    {
      for (std::size_t index = c_first_named_keyword; index < c_keyword_count; ++index)
      {
        const auto keyword = static_cast<schema_keyword>(index);
        if (spells(name, keyword_name(keyword)))
        {
          return keyword;
        }
      }
      return std::nullopt;
    }

    /**
     * @brief Copy a decoded string's UTF-16 units into the name table.
     * @par Requirements
     * P044; J5-066.
     * @par ID SWU-SCHEMA-096
     */
    status copy_units(compile_state& state, const decoded_string& name, node_range& out) noexcept
    {
      out.first = state.units.size();
      out.count = name.size();
      for (std::size_t index = 0U; index < name.size(); ++index)
      {
        const auto unit = name.unit_at(index);
        if (!unit.has_value())
        {
          return access_error(unit.error_if());
        }
        const auto pushed = state.units.push_back(*unit.value_if());
        if (!pushed.has_value())
        {
          return pushed;
        }
      }
      return status::success();
    }

    /**
     * @brief Replace out with the UTF-8 form of a string value.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-097
     */
    status string_utf8(const value_view& value, pod_vector<char>& out) noexcept
    {
      const auto text = value.as_string();
      if (!text.has_value())
      {
        return shape_error(value);
      }
      const auto size = utf8_size(**text.value_if());
      if (!size.has_value())
      {
        return access_error(size.error_if());
      }
      out.truncate(0U);
      for (std::size_t index = 0U; index < *size.value_if(); ++index)
      {
        const auto pushed = out.push_back('\0');
        if (!pushed.has_value())
        {
          return pushed;
        }
      }
      if (*size.value_if() == 0U)
      {
        return status::success();
      }
      const auto written = export_utf8(**text.value_if(), out.writable(), out.size());
      if (!written.has_value())
      {
        return access_error(written.error_if());
      }
      return status::success();
    }

    /**
     * @brief Append text to a pool and return its range.
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-098
     */
    status store_text(pod_vector<char>& pool, std::string_view text, node_range& out) noexcept
    {
      out.first = pool.size();
      out.count = text.size();
      return detail::append_text(pool, text);
    }

    /**
     * @brief Register an identifier or plain-name key for node.
     * @par Requirements
     * P044; J5-055; ADR 0006 O1.
     * @note The first key registered for a URI wins on lookup.
     * @par ID SWU-SCHEMA-099
     */
    status add_key(compile_state& state, std::string_view text, std::size_t node) noexcept
    {
      key_entry entry{};
      entry.node = node;
      const auto stored = store_text(state.key_text, text, entry.text);
      if (!stored.has_value())
      {
        return stored;
      }
      return state.keys.push_back(entry);
    }

    /**
     * @brief Append a node, enforcing max_schema_nodes.
     * @par Requirements
     * P044; J5-047; ADR 0006 decision 5.
     * @par ID SWU-SCHEMA-100
     */
    status add_node(compile_state& state, const schema_node& node, std::size_t& index) noexcept
    {
      if (state.nodes.size() >= state.limits.max_schema_nodes)
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_schema_nodes));
      }
      index = state.nodes.size();
      return state.nodes.push_back(node);
    }

    /**
     * @brief Create a child node for value and queue it for processing.
     * @par Requirements
     * P044; J5-048; J5-064.
     * @par ID SWU-SCHEMA-101
     */
    status add_child(compile_state& state, const pending_item& parent, schema_keyword keyword, step_kind step,
                     node_range name, std::size_t position, const value_view& value, std::size_t& index) noexcept
    {
      schema_node child{};
      child.parent = parent.node;
      child.step_keyword = keyword;
      child.step = step;
      child.step_name = name;
      child.step_index = position;
      child.resource = state.nodes.get(parent.node).resource;
      const auto span = span_of(value);
      child.has_span = span.has_value();
      child.span = span.value_or(source_span{});
      const auto added = add_node(state, child, index);
      if (!added.has_value())
      {
        return added;
      }
      return state.pending.push_back(pending_item{value, index, parent.base, false});
    }

    /**
     * @brief Apply a non-`$ref` node's `$id`: new base URI and identifier keys.
     * @par Requirements
     * P044; J5-055; ADR 0006 decision 8, O1.
     * @par ID SWU-SCHEMA-102
     */
    status apply_id(compile_state& state, const value_view& id, std::size_t node, node_range& base) noexcept
    {
      const auto text = string_utf8(id, state.text);
      if (!text.has_value())
      {
        return text;
      }
      const std::string_view id_text = detail::text_of(state.text, 0U, state.text.size());
      const detail::uri_parts parts = detail::split_uri(id_text);
      const bool plain_name = !parts.has_scheme && !parts.has_authority && parts.path.empty() && !parts.has_query &&
                              parts.has_fragment && !parts.fragment.empty() && (parts.fragment[0U] != '/');

      state.resolved.truncate(0U);
      const auto resolved = detail::resolve_uri(detail::text_of(state.bases, base.first, base.count), id_text,
                                                state.resolved, state.scratch);
      if (!resolved.has_value())
      {
        return resolved;
      }
      const std::string_view target = detail::text_of(state.resolved, 0U, state.resolved.size());
      if (plain_name)
      {
        return add_key(state, target, node);
      }

      const std::string_view document = detail::without_fragment(target);
      const auto stored = store_text(state.bases, document, base);
      if (!stored.has_value())
      {
        return stored;
      }
      const auto keyed = add_key(state, document, node);
      if (!keyed.has_value())
      {
        return keyed;
      }
      const std::size_t hash = target.find('#');
      if ((hash != std::string_view::npos) && ((hash + 1U) < target.size()) && (target[hash + 1U] != '/'))
      {
        return add_key(state, target, node);
      }
      return status::success();
    }

    /**
     * @brief Queue the elements of an array keyword (allOf, anyOf, oneOf, items).
     * @par Requirements
     * P044; J5-054.
     * @param require_nonempty True for the combinators, which need at least one schema.
     * @param edges Receives the children as an edge range.
     * @par ID SWU-SCHEMA-103
     */
    status add_array_children(compile_state& state, const pending_item& parent, schema_keyword keyword,
                              const value_view& value, bool require_nonempty, node_range& edges) noexcept
    {
      const auto size = value.container_size();
      if ((kind_of(value) != value_kind::array) || !size.has_value() || (require_nonempty && (*size.value_if() == 0U)))
      {
        return shape_error(value);
      }
      edges.first = state.edges.size();
      edges.count = *size.value_if();
      auto elements = value.children();
      if (!elements.has_value())
      {
        return access_error(elements.error_if());
      }
      for (std::size_t position = 0U;; ++position)
      {
        const auto element = elements.value_if()->next();
        if (!element.has_value())
        {
          return access_error(element.error_if());
        }
        if (!element.value_if()->has_value())
        {
          return status::success();
        }
        std::size_t child = 0U;
        const auto added =
          add_child(state, parent, keyword, step_kind::index, node_range{}, position, **element.value_if(), child);
        if (!added.has_value())
        {
          return added;
        }
        const auto pushed = state.edges.push_back(child);
        if (!pushed.has_value())
        {
          return pushed;
        }
      }
    }

    /**
     * @brief Queue the member schemas of a map keyword (properties, definitions, ...).
     * @par Requirements
     * P044; J5-054.
     * @param property_table True for properties, whose entries also go to the property table.
     * @param skip_arrays True for dependencies, whose array values are not schemas.
     * @par ID SWU-SCHEMA-104
     */
    status add_map_children(compile_state& state, const pending_item& parent, schema_keyword keyword,
                            const value_view& value, bool property_table, bool skip_arrays,
                            node_range& properties) noexcept
    {
      if (kind_of(value) != value_kind::object)
      {
        return shape_error(value);
      }
      properties.first = state.properties.size();
      auto members = value.children();
      if (!members.has_value())
      {
        return access_error(members.error_if());
      }
      for (;;)
      {
        const auto member = members.value_if()->next();
        if (!member.has_value())
        {
          return access_error(member.error_if());
        }
        if (!member.value_if()->has_value())
        {
          properties.count = state.properties.size() - properties.first;
          return status::success();
        }
        const value_view& entry = **member.value_if();
        if (skip_arrays && (kind_of(entry) == value_kind::array))
        {
          continue;
        }
        const auto name = entry.member_name();
        if (!name.has_value())
        {
          return access_error(name.error_if());
        }
        node_range units{};
        const auto copied = copy_units(state, **name.value_if(), units);
        if (!copied.has_value())
        {
          return copied;
        }
        std::size_t child = 0U;
        const auto added = add_child(state, parent, keyword, step_kind::name, units, 0U, entry, child);
        if (!added.has_value())
        {
          return added;
        }
        if (property_table)
        {
          const auto pushed = state.properties.push_back(property_edge{units, child});
          if (!pushed.has_value())
          {
            return pushed;
          }
        }
      }
    }

    /**
     * @brief Collect an object schema's members by keyword and count unknown keywords.
     * @par Requirements
     * P044; J5-062.
     * @par ID SWU-SCHEMA-105
     */
    status collect_keywords(compile_state& state, const value_view& object,
                            std::array<std::optional<value_view>, c_keyword_count>& found,
                            std::size_t& unknown) noexcept
    {
      auto members = object.children();
      if (!members.has_value())
      {
        return access_error(members.error_if());
      }
      for (;;)
      {
        const auto member = members.value_if()->next();
        if (!member.has_value())
        {
          return access_error(member.error_if());
        }
        if (!member.value_if()->has_value())
        {
          return status::success();
        }
        const auto charged = charge(state);
        if (!charged.has_value())
        {
          return charged;
        }
        const value_view& entry = **member.value_if();
        const auto name = entry.member_name();
        if (!name.has_value())
        {
          return access_error(name.error_if());
        }
        const auto keyword = classify(**name.value_if());
        if (keyword.has_value())
        {
          found[static_cast<std::size_t>(*keyword)] = entry;
        }
        else
        {
          ++unknown;
        }
      }
    }

    /**
     * @brief Check a resource root's `$schema` against the selected dialect.
     * @par Requirements
     * P044; J5-053; ADR 0006 decision 2.
     * @par ID SWU-SCHEMA-106
     */
    status check_dialect(compile_state& state, const value_view& value) noexcept
    {
      if (kind_of(value) != value_kind::string)
      {
        return shape_error(value);
      }
      const auto text = string_utf8(value, state.text);
      if (!text.has_value())
      {
        return text;
      }
      if (!matches_dialect_uri(state.dialect, detail::text_of(state.text, 0U, state.text.size())))
      {
        return status::failure(schema_error::of(schema_error_code::unsupported_dialect, span_of(value)));
      }
      return status::success();
    }

    /**
     * @brief Queue the schemas of every single-schema keyword present.
     * @par Requirements
     * P044; P046; J5-054.
     * @par ID SWU-SCHEMA-107
     */
    status add_single_children(compile_state& state, const pending_item& item,
                               const std::array<std::optional<value_view>, c_keyword_count>& found, bool evaluate,
                               schema_node& node) noexcept
    {
      constexpr std::array<schema_keyword, 8> c_single{
        schema_keyword::additional_items, schema_keyword::additional_properties,
        schema_keyword::contains,         schema_keyword::property_names,
        schema_keyword::if_schema,        schema_keyword::then_schema,
        schema_keyword::else_schema,      schema_keyword::not_schema};
      for (const schema_keyword keyword : c_single)
      {
        const auto& value = found[static_cast<std::size_t>(keyword)];
        if (!value.has_value())
        {
          continue;
        }
        std::size_t child = 0U;
        const auto added = add_child(state, item, keyword, step_kind::none, node_range{}, 0U, *value, child);
        if (!added.has_value())
        {
          return added;
        }
        if (!evaluate)
        {
          continue;
        }
        if (keyword == schema_keyword::if_schema)
        {
          node.if_schema = child;
        }
        else if (keyword == schema_keyword::then_schema)
        {
          node.then_schema = child;
        }
        else if (keyword == schema_keyword::else_schema)
        {
          node.else_schema = child;
        }
        else if (keyword == schema_keyword::not_schema)
        {
          node.not_schema = child;
        }
        else
        {
          // Evaluated from P047 on; compiled now so pointers into them resolve.
        }
      }
      return status::success();
    }

    /**
     * @brief Queue array- and map-valued subschema keywords.
     * @par Requirements
     * P044; P046; J5-054.
     * @par ID SWU-SCHEMA-108
     */
    status add_collection_children(compile_state& state, const pending_item& item,
                                   const std::array<std::optional<value_view>, c_keyword_count>& found, bool evaluate,
                                   schema_node& node) noexcept
    {
      const auto& items = found[static_cast<std::size_t>(schema_keyword::items)];
      if (items.has_value())
      {
        if (kind_of(*items) == value_kind::array)
        {
          node_range range{};
          const auto added = add_array_children(state, item, schema_keyword::items, *items, false, range);
          if (!added.has_value())
          {
            return added;
          }
          node.items_array = evaluate ? range : node_range{};
        }
        else
        {
          std::size_t child = 0U;
          const auto added =
            add_child(state, item, schema_keyword::items, step_kind::none, node_range{}, 0U, *items, child);
          if (!added.has_value())
          {
            return added;
          }
          node.items_single = evaluate ? child : c_no_node;
        }
      }

      constexpr std::array<schema_keyword, 3> c_arrays{schema_keyword::all_of, schema_keyword::any_of,
                                                       schema_keyword::one_of};
      for (const schema_keyword keyword : c_arrays)
      {
        const auto& value = found[static_cast<std::size_t>(keyword)];
        if (!value.has_value())
        {
          continue;
        }
        node_range range{};
        const auto added = add_array_children(state, item, keyword, *value, true, range);
        if (!added.has_value())
        {
          return added;
        }
        if (evaluate)
        {
          node_range& target = (keyword == schema_keyword::all_of)   ? node.all_of
                               : (keyword == schema_keyword::any_of) ? node.any_of
                                                                     : node.one_of;
          target = range;
        }
      }

      constexpr std::array<schema_keyword, 4> c_maps{schema_keyword::definitions, schema_keyword::properties,
                                                     schema_keyword::pattern_properties, schema_keyword::dependencies};
      for (const schema_keyword keyword : c_maps)
      {
        const auto& value = found[static_cast<std::size_t>(keyword)];
        if (!value.has_value())
        {
          continue;
        }
        const bool property_table = evaluate && (keyword == schema_keyword::properties);
        node_range range{};
        const auto added = add_map_children(state, item, keyword, *value, property_table,
                                            keyword == schema_keyword::dependencies, range);
        if (!added.has_value())
        {
          return added;
        }
        if (property_table)
        {
          node.properties = range;
        }
      }
      return status::success();
    }

    /**
     * @brief Compile one queued schema value into its node.
     * @par Requirements
     * P044; J5-053; J5-054; J5-055; J5-062; ADR 0006 decisions 2, 10, O1 and O2.
     * @par ID SWU-SCHEMA-109
     */
    status process(compile_state& state, const pending_item& item) noexcept
    {
      const auto charged = charge(state);
      if (!charged.has_value())
      {
        return charged;
      }
      schema_node node = state.nodes.get(item.node);
      const auto kind = kind_of(item.value);
      if (kind == value_kind::boolean)
      {
        const auto value = item.value.as_boolean();
        if (!value.has_value())
        {
          return access_error(value.error_if());
        }
        node.is_boolean = true;
        node.boolean_value = *value.value_if();
        state.nodes.set(item.node, node);
        return status::success();
      }
      if (kind != value_kind::object)
      {
        return shape_error(item.value);
      }

      std::array<std::optional<value_view>, c_keyword_count> found{};
      std::size_t unknown = 0U;
      const auto collected = collect_keywords(state, item.value, found, unknown);
      if (!collected.has_value())
      {
        return collected;
      }
      node.unknown_keywords = unknown;

      const auto& dialect = found[static_cast<std::size_t>(schema_keyword::schema)];
      if (item.resource_root && dialect.has_value())
      {
        const auto checked = check_dialect(state, *dialect);
        if (!checked.has_value())
        {
          return checked;
        }
      }

      pending_item children = item;
      const auto& ref = found[static_cast<std::size_t>(schema_keyword::ref)];
      const auto& id = found[static_cast<std::size_t>(schema_keyword::id)];
      if (ref.has_value())
      {
        ref_entry entry{};
        entry.node = item.node;
        entry.base = item.base;
        const auto span = span_of(*ref);
        entry.has_span = span.has_value();
        entry.span = span.value_or(source_span{});
        const auto text = string_utf8(*ref, state.text);
        if (!text.has_value())
        {
          return text;
        }
        const auto stored = store_text(state.ref_text, detail::text_of(state.text, 0U, state.text.size()), entry.text);
        if (!stored.has_value())
        {
          return stored;
        }
        const auto pushed = state.refs.push_back(entry);
        if (!pushed.has_value())
        {
          return pushed;
        }
      }
      else if (id.has_value())
      {
        const auto applied = apply_id(state, *id, item.node, children.base);
        if (!applied.has_value())
        {
          return applied;
        }
      }

      const bool evaluate = !ref.has_value();
      node.first_child = state.nodes.size();
      const auto single = add_single_children(state, children, found, evaluate, node);
      if (!single.has_value())
      {
        return single;
      }
      const auto collection = add_collection_children(state, children, found, evaluate, node);
      if (!collection.has_value())
      {
        return collection;
      }
      node.child_count = state.nodes.size() - node.first_child;
      state.nodes.set(item.node, node);
      return status::success();
    }

    /**
     * @brief Find the node registered under an exact key.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-110
     */
    result<std::size_t, schema_error> find_key(compile_state& state, std::string_view text) noexcept
    {
      for (std::size_t index = 0U; index < state.keys.size(); ++index)
      {
        const auto charged = charge(state);
        if (!charged.has_value())
        {
          return result<std::size_t, schema_error>::failure(*charged.error_if());
        }
        const key_entry entry = state.keys.get(index);
        if (detail::text_of(state.key_text, entry.text.first, entry.text.count) == text)
        {
          return result<std::size_t, schema_error>::success(entry.node);
        }
      }
      return result<std::size_t, schema_error>::failure(schema_error::of(schema_error_code::unresolved_reference));
    }

    /**
     * @brief Decode one UTF-8 code point; nullopt for malformed input.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-111
     */
    std::optional<std::uint32_t> next_code_point(std::string_view text, std::size_t& position) noexcept
    {
      const auto lead = static_cast<unsigned char>(text[position]);
      std::size_t extra = 0U;
      std::uint32_t value = 0U;
      if (lead < 0x80U)
      {
        value = lead;
      }
      else if ((lead & 0xE0U) == 0xC0U)
      {
        extra = 1U;
        value = lead & 0x1FU;
      }
      else if ((lead & 0xF0U) == 0xE0U)
      {
        extra = 2U;
        value = lead & 0x0FU;
      }
      else if ((lead & 0xF8U) == 0xF0U)
      {
        extra = 3U;
        value = lead & 0x07U;
      }
      else
      {
        return std::nullopt;
      }
      if ((extra != 0U) && ((position + extra) >= text.size()))
      {
        return std::nullopt;
      }
      for (std::size_t index = 1U; index <= extra; ++index)
      {
        const auto next = static_cast<unsigned char>(text[position + index]);
        if ((next & 0xC0U) != 0x80U)
        {
          return std::nullopt;
        }
        value = (value << 6U) | (next & 0x3FU);
      }
      position += extra + 1U;
      return value;
    }

    /**
     * @brief Compare UTF-8 text with a UTF-16 range of the name table.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-112
     */
    bool utf8_equals_units(const compile_state& state, std::string_view text, node_range units) noexcept
    {
      std::size_t position = 0U;
      std::size_t unit = 0U;
      while (position < text.size())
      {
        const auto code_point = next_code_point(text, position);
        if (!code_point.has_value())
        {
          return false;
        }
        if (*code_point < 0x10000U)
        {
          if ((unit >= units.count) || (state.units.get(units.first + unit) != *code_point))
          {
            return false;
          }
          ++unit;
        }
        else
        {
          const std::uint32_t offset = *code_point - 0x10000U;
          const auto high = static_cast<std::uint16_t>(0xD800U + (offset >> 10U));
          const auto low = static_cast<std::uint16_t>(0xDC00U + (offset & 0x3FFU));
          if (((unit + 1U) >= units.count) || (state.units.get(units.first + unit) != high) ||
              (state.units.get(units.first + unit + 1U) != low))
          {
            return false;
          }
          unit += 2U;
        }
      }
      return unit == units.count;
    }

    /**
     * @brief Find the child of parent reached by keyword (and name or index), or c_no_node.
     * @par Requirements
     * P044; J5-055.
     * @param keyword_token Decoded pointer token naming the keyword.
     * @param second Decoded second token for map and array keywords; nullopt to match only
     * the keyword. kind receives the step kind of the matched keyword.
     * @par ID SWU-SCHEMA-113
     */
    result<std::size_t, schema_error> find_child(compile_state& state, std::size_t parent,
                                                 std::string_view keyword_token, std::optional<std::string_view> second,
                                                 step_kind& kind) noexcept
    {
      using found_result = result<std::size_t, schema_error>;
      const schema_node owner = state.nodes.get(parent);
      for (std::size_t child = owner.first_child; child < (owner.first_child + owner.child_count); ++child)
      {
        const auto charged = charge(state);
        if (!charged.has_value())
        {
          return found_result::failure(*charged.error_if());
        }
        const schema_node candidate = state.nodes.get(child);
        if (keyword_name(candidate.step_keyword) != keyword_token)
        {
          continue;
        }
        kind = candidate.step;
        if (!second.has_value())
        {
          return found_result::success(child);
        }
        const bool matches = (candidate.step == step_kind::name)
                               ? utf8_equals_units(state, *second, candidate.step_name)
                               : (detail::pointer_index(*second) == std::optional<std::size_t>{candidate.step_index});
        if (matches)
        {
          return found_result::success(child);
        }
      }
      return found_result::success(c_no_node);
    }

    /**
     * @brief Follow a JSON Pointer fragment through compiled child steps.
     * @par Requirements
     * P044; J5-055; RFC 6901.
     * @return The target node, or unresolved_reference when a step names no compiled schema.
     * @par ID SWU-SCHEMA-114
     */
    result<std::size_t, schema_error> walk_pointer(compile_state& state, std::size_t start,
                                                   std::string_view pointer) noexcept
    {
      using found_result = result<std::size_t, schema_error>;
      std::string_view rest = pointer;
      std::size_t current = start;
      while (!rest.empty())
      {
        const auto first = detail::next_pointer_token(rest, state.token);
        if (!first.has_value())
        {
          return found_result::failure(*first.error_if());
        }
        // The keyword token is copied: the second token reuses the token buffer.
        state.text.truncate(0U);
        const auto copied = detail::append_text(state.text, detail::text_of(state.token, 0U, state.token.size()));
        if (!copied.has_value())
        {
          return found_result::failure(*copied.error_if());
        }
        const std::string_view keyword = detail::text_of(state.text, 0U, state.text.size());
        step_kind kind = step_kind::none;
        const auto probe = find_child(state, current, keyword, std::nullopt, kind);
        if (!probe.has_value())
        {
          return probe;
        }
        if (*probe.value_if() == c_no_node)
        {
          return found_result::failure(schema_error::of(schema_error_code::unresolved_reference));
        }
        if (kind == step_kind::none)
        {
          current = *probe.value_if();
          continue;
        }
        if (rest.empty())
        {
          return found_result::failure(schema_error::of(schema_error_code::unresolved_reference));
        }
        const auto second = detail::next_pointer_token(rest, state.token);
        if (!second.has_value())
        {
          return found_result::failure(*second.error_if());
        }
        const auto child =
          find_child(state, current, keyword, detail::text_of(state.token, 0U, state.token.size()), kind);
        if (!child.has_value())
        {
          return child;
        }
        if (*child.value_if() == c_no_node)
        {
          return found_result::failure(schema_error::of(schema_error_code::unresolved_reference));
        }
        current = *child.value_if();
      }
      return found_result::success(current);
    }

    /**
     * @brief Find the node an absolute, normalized URI names.
     * @par Requirements
     * P044; J5-055; ADR 0006 decision 8, O1.
     * @note uri must not live in state.text or state.token.
     * @par ID SWU-SCHEMA-115
     */
    result<std::size_t, schema_error> resolve_target(compile_state& state, std::string_view uri) noexcept
    {
      const std::size_t hash = uri.find('#');
      const std::string_view fragment = (hash == std::string_view::npos) ? std::string_view{} : uri.substr(hash + 1U);
      if (fragment.empty())
      {
        return find_key(state, detail::without_fragment(uri));
      }
      if (fragment[0U] != '/')
      {
        return find_key(state, uri);
      }
      const auto document = find_key(state, detail::without_fragment(uri));
      if (!document.has_value())
      {
        return document;
      }
      return walk_pointer(state, *document.value_if(), fragment);
    }

    /**
     * @brief Return an unresolved_reference failure at an optional range.
     * @par Requirements
     * P044; J5-055; J5-064.
     * @par ID SWU-SCHEMA-116
     */
    schema_error located(schema_error_code code, const ref_entry& entry) noexcept
    {
      return schema_error::of(code, entry.has_span ? std::optional<source_span>{entry.span} : std::nullopt);
    }

    /**
     * @brief Resolve every recorded `$ref` to a node index.
     * @par Requirements
     * P044; J5-055; ADR 0006 decision 10.
     * @par ID SWU-SCHEMA-117
     */
    status resolve_refs(compile_state& state) noexcept
    {
      for (std::size_t index = 0U; index < state.refs.size(); ++index)
      {
        const ref_entry entry = state.refs.get(index);
        state.resolved.truncate(0U);
        const auto resolved = detail::resolve_uri(detail::text_of(state.bases, entry.base.first, entry.base.count),
                                                  detail::text_of(state.ref_text, entry.text.first, entry.text.count),
                                                  state.resolved, state.scratch);
        if (!resolved.has_value())
        {
          return resolved;
        }
        const auto target = resolve_target(state, detail::text_of(state.resolved, 0U, state.resolved.size()));
        if (!target.has_value())
        {
          const schema_error& failure = *target.error_if();
          if (failure.code() == schema_error_code::unresolved_reference)
          {
            return status::failure(located(schema_error_code::unresolved_reference, entry));
          }
          return status::failure(failure);
        }
        schema_node node = state.nodes.get(entry.node);
        node.ref = *target.value_if();
        state.nodes.set(entry.node, node);
      }
      return status::success();
    }

    /**
     * @brief Reject pure `$ref` cycles, which can never make progress.
     * @par Requirements
     * P044; J5-056; ADR 0006 decision 3.
     * @par ID SWU-SCHEMA-118
     */
    status reject_pure_cycles(compile_state& state) noexcept
    {
      for (std::size_t index = 0U; index < state.refs.size(); ++index)
      {
        const ref_entry entry = state.refs.get(index);
        std::size_t target = state.nodes.get(entry.node).ref;
        for (std::size_t hops = 0U; (target != c_no_node) && (hops <= state.refs.size()); ++hops)
        {
          const auto charged = charge(state);
          if (!charged.has_value())
          {
            return charged;
          }
          if (target == entry.node)
          {
            return status::failure(located(schema_error_code::nonprogressing_reference, entry));
          }
          target = state.nodes.get(target).ref;
        }
      }
      return status::success();
    }

    /**
     * @brief Create, queue and process the root node of one registered resource.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-119
     */
    status add_resource(compile_state& state, const schema_registry& registry, std::size_t resource) noexcept
    {
      const auto& entry = detail::schema_registry_access::entry(registry, resource);
      const auto root = entry.parsed.values.root();
      if (!root.has_value())
      {
        return access_error(root.error_if());
      }
      schema_node node{};
      node.resource = resource;
      const auto span = span_of(*root.value_if());
      node.has_span = span.has_value();
      node.span = span.value_or(source_span{});
      std::size_t index = 0U;
      const auto added = add_node(state, node, index);
      if (!added.has_value())
      {
        return added;
      }
      const std::string_view uri = detail::schema_registry_access::uri(entry);
      node_range base{};
      auto stored = store_text(state.bases, uri, base);
      if (stored.has_value())
      {
        stored = add_key(state, uri, index);
      }
      if (stored.has_value())
      {
        stored = state.pending.push_back(pending_item{*root.value_if(), index, base, true});
      }
      if (!stored.has_value())
      {
        return stored;
      }
      while (state.pending.size() != 0U)
      {
        const pending_item item = state.pending.get(state.pending.size() - 1U);
        state.pending.pop_back();
        const auto processed = process(state, item);
        if (!processed.has_value())
        {
          return processed;
        }
      }
      return status::success();
    }

    /**
     * @brief Find the root schema named by root_uri.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-120
     */
    result<std::size_t, schema_error> find_root(compile_state& state, std::string_view root_uri) noexcept
    {
      if (!detail::split_uri(root_uri).has_scheme)
      {
        return result<std::size_t, schema_error>::failure(schema_error::of(schema_error_code::unresolved_reference));
      }
      state.resolved.truncate(0U);
      const auto normalized = detail::resolve_uri(root_uri, root_uri, state.resolved, state.scratch);
      if (!normalized.has_value())
      {
        return result<std::size_t, schema_error>::failure(*normalized.error_if());
      }
      return resolve_target(state, detail::text_of(state.resolved, 0U, state.resolved.size()));
    }
  } // namespace

  /**
   * @brief Compile every registered resource and select the root schema.
   * @par Requirements
   * P044; J5-047; J5-053; J5-054; J5-055; J5-056; J5-066; J5-067; ADR 0006 decisions 2, 5,
   * 8, 9, 10, O1, O2 and O7.
   * @par ID SWU-SCHEMA-081
   */
  result<compiled_schema, schema_error> compile_schema(const schema_registry& registry, std::string_view root_uri,
                                                       schema_dialect dialect, const compile_options& options) noexcept
  {
    using compile_result = result<compiled_schema, schema_error>;
    compile_state state{options, dialect};

    for (std::size_t resource = 0U; resource < registry.size(); ++resource)
    {
      const auto added = add_resource(state, registry, resource);
      if (!added.has_value())
      {
        return compile_result::failure(*added.error_if());
      }
    }
    const auto refs = resolve_refs(state);
    if (!refs.has_value())
    {
      return compile_result::failure(*refs.error_if());
    }
    const auto cycles = reject_pure_cycles(state);
    if (!cycles.has_value())
    {
      return compile_result::failure(*cycles.error_if());
    }
    const auto root = find_root(state, root_uri);
    if (!root.has_value())
    {
      return compile_result::failure(*root.error_if());
    }

    const std::size_t node_count = state.nodes.size();
    const std::size_t edge_count = state.edges.size();
    const std::size_t property_count = state.properties.size();
    const std::size_t unit_count = state.units.size();
    return compile_result::success(detail::compiled_schema_access::make(
      state.nodes.release(), node_count, state.edges.release(), edge_count, state.properties.release(), property_count,
      state.units.release(), unit_count, registry.size(), *root.value_if()));
  }

  /**
   * @brief Take over another compiled schema; other becomes empty.
   * @par Requirements
   * P044; J5-066.
   * @par ID SWU-SCHEMA-161
   */
  compiled_schema::compiled_schema(compiled_schema&& other) noexcept
      : m_nodes(std::move(other.m_nodes)), m_edges(std::move(other.m_edges)),
        m_properties(std::move(other.m_properties)), m_units(std::move(other.m_units)),
        m_node_count(other.m_node_count), m_edge_count(other.m_edge_count), m_property_count(other.m_property_count),
        m_unit_count(other.m_unit_count), m_resource_count(other.m_resource_count), m_root(other.m_root)
  {
    other.m_node_count = 0U;
    other.m_edge_count = 0U;
    other.m_property_count = 0U;
    other.m_unit_count = 0U;
    other.m_resource_count = 0U;
    other.m_root = 0U;
  }

  /**
   * @brief Return the number of compiled schema nodes.
   * @par Requirements
   * P044; J5-047.
   * @par ID SWU-SCHEMA-078
   */
  std::size_t compiled_schema::node_count() const noexcept
  {
    return m_node_count;
  }

  /**
   * @brief Return the number of registry resources compiled.
   * @par Requirements
   * P044; J5-055.
   * @par ID SWU-SCHEMA-079
   */
  std::size_t compiled_schema::resource_count() const noexcept
  {
    return m_resource_count;
  }

  /**
   * @brief Return the total number of unknown keywords across all nodes.
   * @par Requirements
   * P044; J5-062; ADR 0006 decision 10.
   * @par ID SWU-SCHEMA-080
   */
  std::size_t compiled_schema::unknown_keyword_count() const noexcept
  {
    std::size_t total = 0U;
    for (std::size_t index = 0U; index < m_node_count; ++index)
    {
      total += detail::compiled_schema_access::node(*this, index).unknown_keywords;
    }
    return total;
  }

  namespace detail
  {
    /**
     * @brief Build a compiled schema from finished tables.
     * @par Requirements
     * P044; J5-066.
     * @par ID SWU-SCHEMA-082
     */
    compiled_schema compiled_schema_access::make(owned_block&& nodes, std::size_t node_count, owned_block&& edges,
                                                 std::size_t edge_count, owned_block&& properties,
                                                 std::size_t property_count, owned_block&& units,
                                                 std::size_t unit_count, std::size_t resource_count,
                                                 std::size_t root) noexcept
    {
      compiled_schema schema{};
      schema.m_nodes = std::move(nodes);
      schema.m_edges = std::move(edges);
      schema.m_properties = std::move(properties);
      schema.m_units = std::move(units);
      schema.m_node_count = node_count;
      schema.m_edge_count = edge_count;
      schema.m_property_count = property_count;
      schema.m_unit_count = unit_count;
      schema.m_resource_count = resource_count;
      schema.m_root = root;
      return schema;
    }

    /**
     * @brief Borrow node index; requires index < node_count().
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-083
     */
    const schema_node& compiled_schema_access::node(const compiled_schema& schema, std::size_t index) noexcept
    {
      return static_cast<const schema_node*>(schema.m_nodes.data())[index];
    }

    /**
     * @brief Return edge table entry index (a node index).
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-084
     */
    std::size_t compiled_schema_access::edge(const compiled_schema& schema, std::size_t index) noexcept
    {
      return static_cast<const std::size_t*>(schema.m_edges.data())[index];
    }

    /**
     * @brief Borrow property table entry index.
     * @par Requirements
     * P046.
     * @par ID SWU-SCHEMA-085
     */
    const property_edge& compiled_schema_access::property(const compiled_schema& schema, std::size_t index) noexcept
    {
      return static_cast<const property_edge*>(schema.m_properties.data())[index];
    }

    /**
     * @brief Return UTF-16 unit index of the name table.
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-086
     */
    std::uint16_t compiled_schema_access::unit(const compiled_schema& schema, std::size_t index) noexcept
    {
      return static_cast<const std::uint16_t*>(schema.m_units.data())[index];
    }

    /**
     * @brief Return the root node index.
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-087
     */
    std::size_t compiled_schema_access::root(const compiled_schema& schema) noexcept
    {
      return schema.m_root;
    }
  } // namespace detail
} // namespace adaptive_json5
