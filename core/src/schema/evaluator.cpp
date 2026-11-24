#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"
#include "adaptive_json5/schema/validate.hpp"
#include "adaptive_json5/schema/validation_options.hpp"
#include "adaptive_json5/schema/validation_report.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/assertions.hpp"
#include "schema/instance_domain.hpp"
#include "schema/json_pointer.hpp"
#include "schema/pod_vector.hpp"
#include "schema/regex.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_work.hpp"
#include "value/document_access.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    /**
     * @brief Construction access to validation_report.
     * @par Requirements
     * P046.
     */
    struct validation_report_access
    {
        /**
         * @brief Build a report from a verdict and its diagnostics.
         * @par Requirements
         * P046; J5-063.
         * @par ID SWU-SCHEMA-141
         */
        static validation_report make(bool valid, bool truncated, owned_block&& diagnostics, std::size_t count,
                                      owned_block&& annotations, std::size_t annotation_count) noexcept
        {
          validation_report report{};
          report.m_valid = valid;
          report.m_truncated = truncated;
          report.m_diagnostics = std::move(diagnostics);
          report.m_count = count;
          report.m_annotations = std::move(annotations);
          report.m_annotation_count = annotation_count;
          return report;
        }
    };
  } // namespace detail

  namespace
  {
    using status = result<void, schema_error>;
    using detail::c_no_node;
    using detail::compiled_schema_access;
    using detail::node_range;
    using detail::pod_vector;
    using detail::schema_node;

    /**
     * @brief Evaluation stage of one frame, in keyword order.
     * @par Requirements
     * P046; J5-057.
     */
    enum class phase : std::uint8_t
    {
      enter,
      ref,
      all_of,
      any_of,
      one_of,
      not_schema,
      if_schema,
      then_else,
      properties,
      items,
      contains,
      dependencies,
      names,
      finish
    };

    /**
     * @brief One (schema node, instance value) evaluation on the explicit stack.
     * @par Requirements
     * P046; J5-048; J5-056.
     * @note suppressed frames evaluate a combinator branch whose own failures are not
     * diagnostics; via_ref marks frames entered through `$ref` for loop detection.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.12)
    struct frame
    {
        std::size_t node{};
        value_view instance{};
        std::size_t instance_position{};
        value_iterator children{};
        std::size_t cursor{};
        std::size_t passes{};
        phase stage{phase::enter};
        bool suppressed{};
        bool via_ref{};
        bool valid{true};
        bool started{};
        bool if_result{};
        value_view member{};           // P051: member whose candidate schemas are being applied.
        std::size_t member_target{};   // P051: next candidate (0 properties, 1..n patterns, n+1 additional).
        bool member_open{};            // P051: member is set and has candidates left.
        bool member_matched{};         // P051: properties or a pattern applied to member.
        bool in_names{};               // P051: instance is a property name copy (propertyNames).
        std::size_t annotation_mark{}; // P053: annotation count when the frame was pushed.
        std::size_t branch_mark{};     // P053: annotation count before the `not` subschema.
    };

    /**
     * @brief A child evaluation requested by a frame.
     * @par Requirements
     * P046.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.11)
    struct child_request
    {
        std::size_t node{};
        value_view instance{};
        bool suppressed{};
        bool branch{};
        bool via_ref{};
        bool in_names{};
    };

    /**
     * @brief State of one validation run.
     * @par Requirements
     * P046; J5-063; J5-065; ADR 0006 decisions 7 and 12.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.10)
    struct evaluator
    {
        const compiled_schema& schema;
        detail::schema_work work;
        pod_vector<frame> frames;
        pod_vector<schema_diagnostic> diagnostics;
        pod_vector<schema_annotation> annotations; // P053: records of passing frames so far.
        storage_callbacks scratch;
        std::optional<document> names{}; // P051: copies of member names for propertyNames.
        evaluation_mode mode;
        std::size_t diagnostic_limit;
        bool stop{};
        bool truncated{};
        bool collect{}; // P053: collect_annotations.

        /**
         * @brief Bind the schema, budget and storages; no allocation.
         * @par Requirements
         * P046.
         * @par ID SWU-SCHEMA-142
         */
        evaluator(const compiled_schema& compiled, const validation_options& options) noexcept
            : schema(compiled), work(options.budget, options.cancellation), frames(options.work_storage),
              diagnostics(options.report_storage), annotations(options.report_storage), scratch(options.work_storage),
              mode(options.mode),
              diagnostic_limit((options.mode == evaluation_mode::fail_fast) ? 1U : options.budget.max_errors),
              collect(options.collect_annotations)
        {
        }
    };

    /**
     * @brief Translate a document access failure.
     * @par Requirements
     * P046; J5-043.
     * @par ID SWU-SCHEMA-143
     */
    status access_failure(const error* failure) noexcept
    {
      return (failure == nullptr) ? status::failure(schema_error::of(schema_error_code::internal_error))
                                  : status::failure(schema_error::from_storage(*failure));
    }

    /**
     * @brief Return a value's kind, or nullopt after an access failure.
     * @par Requirements
     * P046.
     * @par ID SWU-SCHEMA-144
     */
    std::optional<value_kind> kind_of(const value_view& value) noexcept
    {
      const auto kind = value.kind();
      return kind.has_value() ? std::optional<value_kind>{*kind.value_if()} : std::nullopt;
    }

    /**
     * @brief Build the location record of a frame (diagnostics and annotations).
     * @par Requirements
     * P046; P053; J5-064.
     * @par ID SWU-SCHEMA-318
     */
    schema_diagnostic located(const evaluator& run, const frame& owner, schema_keyword keyword) noexcept
    {
      const schema_node& node = compiled_schema_access::node(run.schema, owner.node);
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.11)
      schema_diagnostic entry{};
      entry.keyword = keyword;
      entry.instance_position = owner.instance_position;
      const auto span = owner.instance.source_span();
      if (span.has_value() && span.value_if()->has_value())
      {
        entry.has_instance_span = true;
        entry.instance_span = **span.value_if();
      }
      entry.schema_node = owner.node;
      entry.schema_resource = node.resource;
      entry.has_schema_span = node.has_span;
      entry.schema_span = node.span;
      return entry;
    }

    /**
     * @brief Record a diagnostic for a failed keyword of an unsuppressed frame.
     * @par Requirements
     * P046; J5-063; J5-064; ADR 0006 decisions 12 and 13.
     * @note Fail-fast stops after the first diagnostic; collect-all stops, marking the report
     * truncated, when one more failure arrives after max_errors were kept.
     * @par ID SWU-SCHEMA-145
     */
    status record(evaluator& run, const frame& owner, schema_keyword keyword) noexcept
    {
      if (owner.suppressed)
      {
        return status::success();
      }
      if (run.diagnostics.size() >= run.diagnostic_limit)
      {
        run.truncated = (run.mode == evaluation_mode::collect_all);
        run.stop = true;
        return status::success();
      }
      const schema_diagnostic entry = located(run, owner, keyword);
      const auto pushed = run.diagnostics.push_back(entry);
      if (!pushed.has_value())
      {
        return pushed;
      }
      if (run.mode == evaluation_mode::fail_fast)
      {
        run.stop = true;
      }
      return status::success();
    }

    /**
     * @brief Report whether a failing frame may stop evaluating its remaining keywords.
     * @par Requirements
     * P046; J5-063.
     * @par ID SWU-SCHEMA-146
     */
    bool may_stop(const evaluator& run, const frame& owner) noexcept
    {
      return !owner.valid && (owner.suppressed || run.stop || (run.mode == evaluation_mode::fail_fast));
    }

    /**
     * @brief Mark a frame invalid because keyword failed, recording a diagnostic.
     * @par Requirements
     * P046; J5-063.
     * @par ID SWU-SCHEMA-147
     */
    status fail(evaluator& run, frame& owner, schema_keyword keyword) noexcept
    {
      owner.valid = false;
      return record(run, owner, keyword);
    }

    /**
     * @brief Compare an instance member name with a property name in the unit table.
     * @par Requirements
     * P046; J5-057.
     * @par ID SWU-SCHEMA-148
     */
    result<bool, schema_error> name_matches(const evaluator& run, const decoded_string& name, node_range units) noexcept
    {
      if (name.size() != units.count)
      {
        return result<bool, schema_error>::success(false);
      }
      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
      // call; the counter is only incremented by the loop statement.
      // codeql::misra_deviation_next_line(DEV-105.8)
      for (std::size_t index = 0U; index < units.count; ++index)
      {
        const auto unit = name.unit_at(index);
        if (!unit.has_value())
        {
          return result<bool, schema_error>::failure(schema_error::from_storage(*unit.error_if()));
        }
        // MISRA rule: RULE-7-0-5 cpp/misra/no-signedness-change-from-promotion
        // MISRA justification: Comparisons and arithmetic on std::uint16_t code units promote to int without changing
        // the value; the operands are unsigned values below 0x110000 and the results are used as such.
        // codeql::misra_deviation_next_line(DEV-106.3)
        if (*unit.value_if() != compiled_schema_access::unit(run.schema, units.first + index))
        {
          return result<bool, schema_error>::success(false);
        }
      }
      return result<bool, schema_error>::success(true);
    }

    /**
     * @brief Outcome of advancing a frame: a child to evaluate, or the frame finished.
     * @par Requirements
     * P046.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.9)
    struct advance_outcome
    {
        std::optional<child_request> child{};
    };

    using advance_result = result<advance_outcome, schema_error>;

    /**
     * @brief Ask for a child evaluation.
     * @par Requirements
     * P046.
     * @par ID SWU-SCHEMA-149
     */
    advance_result request(std::size_t node, const value_view& instance, bool suppressed, bool branch,
                           bool via_ref = false) noexcept
    {
      return advance_result::success(advance_outcome{child_request{node, instance, suppressed, branch, via_ref}});
    }

    /**
     * @brief Step to the next instance child of an object or array frame.
     * @par Requirements
     * P046; J5-057.
     * @return The next child value, nullopt at the end, or an access failure.
     * @par ID SWU-SCHEMA-150
     */
    result<std::optional<value_view>, schema_error> next_child(frame& owner) noexcept
    {
      using next_result = result<std::optional<value_view>, schema_error>;
      if (!owner.started)
      {
        auto children = owner.instance.children();
        if (!children.has_value())
        {
          return next_result::failure(*access_failure(children.error_if()).error_if());
        }
        owner.children = *children.value_if();
        owner.started = true;
      }
      const auto next = owner.children.next();
      if (!next.has_value())
      {
        return next_result::failure(*access_failure(next.error_if()).error_if());
      }
      return next_result::success(*next.value_if());
    }

    /**
     * @brief Find the properties entry matching an instance member, or c_no_node.
     * @par Requirements
     * P046; J5-057.
     * @par ID SWU-SCHEMA-151
     */
    result<std::size_t, schema_error> property_for(evaluator& run, const schema_node& node,
                                                   const value_view& member) noexcept
    {
      const auto name = member.member_name();
      if (!name.has_value())
      {
        return result<std::size_t, schema_error>::failure(*access_failure(name.error_if()).error_if());
      }
      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
      // call; the counter is only incremented by the loop statement.
      // codeql::misra_deviation_next_line(DEV-105.7)
      for (std::size_t index = 0U; index < node.properties.count; ++index)
      {
        const auto stepped = run.work.step();
        if (!stepped.has_value())
        {
          return result<std::size_t, schema_error>::failure(*stepped.error_if());
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.34)
        const auto& edge = compiled_schema_access::property(run.schema, node.properties.first + index);
        const auto matches = name_matches(run, **name.value_if(), edge.name);
        if (!matches.has_value())
        {
          return result<std::size_t, schema_error>::failure(*matches.error_if());
        }
        if (*matches.value_if())
        {
          return result<std::size_t, schema_error>::success(edge.node);
        }
      }
      return result<std::size_t, schema_error>::success(c_no_node);
    }

    /**
     * @brief Absorb a non-suppressed child's verdict (allOf, then/else, properties, items, $ref).
     * @par Requirements
     * P046; J5-057.
     * @par ID SWU-SCHEMA-152
     */
    void absorb(frame& owner, std::optional<bool>& delivered) noexcept
    {
      if (delivered.has_value() && !*delivered)
      {
        owner.valid = false;
      }
      delivered.reset();
    }

    /**
     * @brief Return edge index of a node's range.
     * @par Requirements
     * P046.
     * @par ID SWU-SCHEMA-169
     */
    std::size_t edge_at(const evaluator& run, node_range range, std::size_t index) noexcept
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.33)
      return compiled_schema_access::edge(run.schema, range.first + index);
    }

    /**
     * @brief Record a combinator failure, returning a failure outcome only on error.
     * @par Requirements
     * P046; J5-063.
     * @par ID SWU-SCHEMA-170
     */
    std::optional<advance_result> fail_keyword(evaluator& run, frame& owner, schema_keyword keyword) noexcept
    {
      const auto failed = fail(run, owner, keyword);
      if (!failed.has_value())
      {
        return advance_result::failure(*failed.error_if());
      }
      return std::nullopt;
    }

    /**
     * @brief allOf: every subschema must pass; failures are the subschemas' own diagnostics.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation section 6.7.1.
     * @par ID SWU-SCHEMA-171
     */
    std::optional<advance_result> step_all_of(const evaluator& run, frame& owner, const schema_node& node,
                                              std::optional<bool>& delivered) noexcept
    {
      absorb(owner, delivered);
      if (!may_stop(run, owner) && (owner.cursor < node.all_of.count))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.32)
        ++owner.cursor;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.31)
        return request(edge_at(run, node.all_of, owner.cursor - 1U), owner.instance, owner.suppressed, true);
      }
      owner.stage = phase::any_of;
      owner.cursor = 0U;
      owner.passes = 0U;
      return std::nullopt;
    }

    /**
     * @brief anyOf: stop at the first passing subschema; report anyOf when none passes.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation section 6.7.2.
     * @par ID SWU-SCHEMA-172
     */
    std::optional<advance_result> step_any_of(evaluator& run, frame& owner, const schema_node& node,
                                              std::optional<bool>& delivered) noexcept
    {
      if (delivered.has_value())
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.30)
        owner.passes += *delivered ? 1U : 0U;
        delivered.reset();
      }
      if (((owner.passes == 0U) || run.collect) && (owner.cursor < node.any_of.count))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.29)
        ++owner.cursor;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.28)
        return request(edge_at(run, node.any_of, owner.cursor - 1U), owner.instance, true, true);
      }
      owner.stage = phase::one_of;
      owner.cursor = 0U;
      const bool failed = (node.any_of.count != 0U) && (owner.passes == 0U);
      owner.passes = 0U;
      return failed ? fail_keyword(run, owner, schema_keyword::any_of) : std::nullopt;
    }

    /**
     * @brief oneOf: exactly one subschema must pass; stop after the second pass.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation section 6.7.3.
     * @par ID SWU-SCHEMA-173
     */
    std::optional<advance_result> step_one_of(evaluator& run, frame& owner, const schema_node& node,
                                              std::optional<bool>& delivered) noexcept
    {
      if (delivered.has_value())
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.27)
        owner.passes += *delivered ? 1U : 0U;
        delivered.reset();
      }
      if ((owner.passes < 2U) && (owner.cursor < node.one_of.count))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.26)
        ++owner.cursor;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.25)
        return request(edge_at(run, node.one_of, owner.cursor - 1U), owner.instance, true, true);
      }
      owner.stage = phase::not_schema;
      owner.cursor = 0U;
      const bool failed = (node.one_of.count != 0U) && (owner.passes != 1U);
      return failed ? fail_keyword(run, owner, schema_keyword::one_of) : std::nullopt;
    }

    /**
     * @brief not: the subschema must fail.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation section 6.7.4.
     * @par ID SWU-SCHEMA-174
     */
    std::optional<advance_result> step_not(evaluator& run, frame& owner, const schema_node& node,
                                           std::optional<bool>& delivered) noexcept
    {
      if (!delivered.has_value() && (node.not_schema != c_no_node))
      {
        owner.branch_mark = run.annotations.size();
        return request(node.not_schema, owner.instance, true, true);
      }
      if (delivered.has_value())
      {
        run.annotations.truncate(owner.branch_mark);
      }
      const bool matched = delivered.value_or(false);
      delivered.reset();
      owner.stage = phase::if_schema;
      return matched ? fail_keyword(run, owner, schema_keyword::not_schema) : std::nullopt;
    }

    /**
     * @brief if: evaluate the condition without diagnostics, then choose then or else.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation section 6.6.
     * @par ID SWU-SCHEMA-175
     */
    std::optional<advance_result> step_if(frame& owner, const schema_node& node,
                                          std::optional<bool>& delivered) noexcept
    {
      if (delivered.has_value())
      {
        owner.if_result = *delivered;
        delivered.reset();
        owner.stage = phase::then_else;
        owner.cursor = 0U;
        return std::nullopt;
      }
      if (node.if_schema != c_no_node)
      {
        return request(node.if_schema, owner.instance, true, true);
      }
      owner.stage = phase::properties;
      return std::nullopt;
    }

    /**
     * @brief then/else: the chosen branch must pass; its failures are its own diagnostics.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation sections 6.6.2 and 6.6.3.
     * @par ID SWU-SCHEMA-176
     */
    std::optional<advance_result> step_then_else(frame& owner, const schema_node& node,
                                                 std::optional<bool>& delivered) noexcept
    {
      absorb(owner, delivered);
      const std::size_t target = owner.if_result ? node.then_schema : node.else_schema;
      if ((owner.cursor == 0U) && (target != c_no_node))
      {
        owner.cursor = 1U;
        return request(target, owner.instance, owner.suppressed, true);
      }
      owner.stage = phase::properties;
      owner.cursor = 0U;
      return std::nullopt;
    }

    /**
     * @brief Run the current combinator phase of a frame.
     * @par Requirements
     * P046; J5-057.
     * @par ID SWU-SCHEMA-179
     */
    std::optional<advance_result> step_phase(evaluator& run, frame& owner, const schema_node& node,
                                             std::optional<bool>& delivered) noexcept
    {
      if (owner.stage == phase::all_of)
      {
        return step_all_of(run, owner, node, delivered);
      }
      if (owner.stage == phase::any_of)
      {
        return step_any_of(run, owner, node, delivered);
      }
      if (owner.stage == phase::one_of)
      {
        return step_one_of(run, owner, node, delivered);
      }
      if (owner.stage == phase::not_schema)
      {
        return step_not(run, owner, node, delivered);
      }
      if (owner.stage == phase::if_schema)
      {
        return step_if(owner, node, delivered);
      }
      return step_then_else(owner, node, delivered);
    }

    /**
     * @brief Advance the applicator phases that evaluate the same instance.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation sections 6.7 and 6.6.
     * @return A child request, an empty outcome to continue with the next phase, or a failure.
     * @par ID SWU-SCHEMA-153
     */
    advance_result advance_combinators(evaluator& run, frame& owner, const schema_node& node,
                                       std::optional<bool>& delivered) noexcept
    {
      auto outcome = step_phase(run, owner, node, delivered);
      if (outcome.has_value())
      {
        // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
        // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
        // once.
        // codeql::misra_deviation_next_line(DEV-108.2)
        return std::move(*outcome);
      }
      if (may_stop(run, owner))
      {
        owner.stage = phase::finish;
      }
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief Choose the subschema for one array element: items, its tuple entry or additionalItems.
     * @par Requirements
     * P046; P050; J5-057.
     * @return The node, or c_no_node when no keyword applies to the element.
     * @par ID SWU-SCHEMA-177
     */
    std::size_t target_for(const evaluator& run, const frame& owner, const schema_node& node) noexcept
    {
      if (node.items_single != c_no_node)
      {
        return node.items_single;
      }
      if (owner.cursor < node.items_array.count)
      {
        return edge_at(run, node.items_array, owner.cursor);
      }
      // additionalItems applies only after an items tuple (Draft-07 Validation section 6.4.2).
      return node.items_is_array ? node.additional_items : c_no_node;
    }

    /**
     * @brief Advance properties and items, which descend into instance children.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation sections 6.4.1 and 6.5.4.
     * @par ID SWU-SCHEMA-154
     */
    advance_result advance_descending(const evaluator& run, frame& owner, const schema_node& node,
                                      std::optional<bool>& delivered) noexcept
    {
      absorb(owner, delivered);
      const auto kind = kind_of(owner.instance);
      const bool applies = (kind == value_kind::array) && ((node.items_single != c_no_node) || node.items_is_array);
      while (applies && !may_stop(run, owner))
      {
        const auto next = next_child(owner);
        if (!next.has_value())
        {
          return advance_result::failure(*next.error_if());
        }
        if (!next.value_if()->has_value())
        {
          break;
        }
        const value_view child = **next.value_if();
        const std::size_t target = target_for(run, owner, node);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.24)
        ++owner.cursor;
        if (target != c_no_node)
        {
          return request(target, child, owner.suppressed, false);
        }
      }
      owner.started = false;
      owner.cursor = 0U;
      owner.passes = 0U;
      owner.stage = may_stop(run, owner) ? phase::finish : phase::contains;
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief contains: evaluate elements without diagnostics until one matches (P050).
     * @par Requirements
     * P050; J5-057; Draft-07 Validation section 6.4.6.
     * @return A child request, an empty outcome when the phase is finished, or a failure.
     * @par ID SWU-SCHEMA-290
     */
    advance_result advance_contains(evaluator& run, frame& owner, const schema_node& node,
                                    std::optional<bool>& delivered) noexcept
    {
      if (delivered.has_value())
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.87)
        owner.passes += *delivered ? 1U : 0U;
        delivered.reset();
      }
      const auto kind = kind_of(owner.instance);
      const bool applies = (node.contains != c_no_node) && (kind == value_kind::array);
      if (applies && (owner.passes == 0U))
      {
        const auto next = next_child(owner);
        if (!next.has_value())
        {
          return advance_result::failure(*next.error_if());
        }
        if (next.value_if()->has_value())
        {
          return request(node.contains, **next.value_if(), true, true);
        }
        const auto failed = fail(run, owner, schema_keyword::contains);
        if (!failed.has_value())
        {
          return advance_result::failure(*failed.error_if());
        }
      }
      owner.started = false;
      owner.cursor = 0U;
      owner.stage = may_stop(run, owner) ? phase::finish : phase::dependencies;
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief Report whether an object instance has a member whose name equals a unit-table name.
     * @par Requirements
     * P051; J5-057.
     * @par ID SWU-SCHEMA-300
     */
    result<bool, schema_error> instance_has_name(evaluator& run, const value_view& object, node_range name) noexcept
    {
      auto members = object.children();
      if (!members.has_value())
      {
        return result<bool, schema_error>::failure(*access_failure(members.error_if()).error_if());
      }
      for (;;)
      {
        const auto member = members.value_if()->next();
        if (!member.has_value() || !member.value_if()->has_value())
        {
          return member.has_value()
                   ? result<bool, schema_error>::success(false)
                   : result<bool, schema_error>::failure(*access_failure(member.error_if()).error_if());
        }
        const auto stepped = run.work.step();
        const auto member_name = (*member.value_if())->member_name();
        if (!stepped.has_value() || !member_name.has_value())
        {
          return stepped.has_value()
                   ? result<bool, schema_error>::failure(*access_failure(member_name.error_if()).error_if())
                   : result<bool, schema_error>::failure(*stepped.error_if());
        }
        const auto matches = name_matches(run, **member_name.value_if(), name);
        if (!matches.has_value() || *matches.value_if())
        {
          return matches;
        }
      }
    }

    /**
     * @brief Return the next candidate schema for the open member: its properties entry, each
     * matching patternProperties entry, then additionalProperties when none matched.
     * @par Requirements
     * P051; J5-057; Draft-07 Validation sections 6.5.4 to 6.5.6.
     * @return The node, c_no_node when the member has no candidate left, or a failure.
     * @par ID SWU-SCHEMA-301
     */
    result<std::size_t, schema_error> next_member_target(evaluator& run, frame& owner, const schema_node& node) noexcept
    {
      using target_result = result<std::size_t, schema_error>;
      const std::size_t patterns = node.pattern_properties.count;
      while (owner.member_target <= (patterns + 1U))
      {
        const std::size_t candidate = owner.member_target;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.86)
        ++owner.member_target;
        if (candidate == 0U)
        {
          const auto found = property_for(run, node, owner.member);
          if (!found.has_value() || (*found.value_if() != c_no_node))
          {
            owner.member_matched = found.has_value() || owner.member_matched;
            return found;
          }
          continue;
        }
        if (candidate > patterns)
        {
          return target_result::success(owner.member_matched ? c_no_node : node.additional_properties);
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.85)
        const auto& edge = compiled_schema_access::property(run.schema, node.pattern_properties.first + candidate - 1U);
        const auto name = owner.member.member_name();
        if (!name.has_value())
        {
          return target_result::failure(*access_failure(name.error_if()).error_if());
        }
        const auto matched = detail::regex_search(compiled_schema_access::regex(run.schema),
                                                  compiled_schema_access::node(run.schema, edge.node).name_pattern,
                                                  **name.value_if(), run.work, run.scratch);
        if (!matched.has_value())
        {
          return target_result::failure(*matched.error_if());
        }
        if (*matched.value_if())
        {
          owner.member_matched = true;
          return target_result::success(edge.node);
        }
      }
      return target_result::success(c_no_node);
    }

    /**
     * @brief properties, patternProperties and additionalProperties over the object members (P051).
     * @par Requirements
     * P051; J5-057; Draft-07 Validation sections 6.5.4 to 6.5.6.
     * @par ID SWU-SCHEMA-302
     */
    advance_result advance_members(evaluator& run, frame& owner, const schema_node& node,
                                   std::optional<bool>& delivered) noexcept
    {
      absorb(owner, delivered);
      const bool applies = (kind_of(owner.instance) == value_kind::object) &&
                           ((node.properties.count != 0U) || (node.pattern_properties.count != 0U) ||
                            (node.additional_properties != c_no_node));
      while (applies && !may_stop(run, owner))
      {
        if (!owner.member_open)
        {
          const auto next = next_child(owner);
          if (!next.has_value())
          {
            return advance_result::failure(*next.error_if());
          }
          if (!next.value_if()->has_value())
          {
            break;
          }
          owner.member = **next.value_if();
          owner.member_target = 0U;
          owner.member_matched = false;
          owner.member_open = true;
        }
        const auto target = next_member_target(run, owner, node);
        if (!target.has_value())
        {
          return advance_result::failure(*target.error_if());
        }
        if (*target.value_if() != c_no_node)
        {
          return request(*target.value_if(), owner.member, owner.suppressed, false);
        }
        owner.member_open = false;
      }
      owner.started = false;
      owner.member_open = false;
      owner.cursor = 0U;
      owner.stage = may_stop(run, owner) ? phase::finish : phase::items;
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief Schema-form dependencies: apply each schema whose trigger member is present (P051).
     * @par Requirements
     * P051; J5-057; Draft-07 Validation section 6.5.7.
     * @par ID SWU-SCHEMA-303
     */
    advance_result advance_dependencies(evaluator& run, frame& owner, const schema_node& node,
                                        std::optional<bool>& delivered) noexcept
    {
      absorb(owner, delivered);
      const bool applies = (kind_of(owner.instance) == value_kind::object) && (node.dependency_schemas.count != 0U);
      while (applies && !may_stop(run, owner) && (owner.cursor < node.dependency_schemas.count))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.84)
        const auto& edge = compiled_schema_access::property(run.schema, node.dependency_schemas.first + owner.cursor);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.83)
        ++owner.cursor;
        const auto present = instance_has_name(run, owner.instance, edge.name);
        if (!present.has_value())
        {
          return advance_result::failure(*present.error_if());
        }
        if (*present.value_if())
        {
          return request(edge.node, owner.instance, owner.suppressed, true);
        }
      }
      owner.cursor = 0U;
      owner.started = false;
      owner.stage = may_stop(run, owner) ? phase::finish : phase::names;
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief Copy a member name into the names document as a string value (for propertyNames).
     * @par Requirements
     * P051; J5-057.
     * @par ID SWU-SCHEMA-304
     */
    result<value_view, schema_error> name_value(evaluator& run, const value_view& member) noexcept
    {
      using view_result = result<value_view, schema_error>;
      if (!run.names.has_value())
      {
        static_cast<void>(
          run.names.emplace(detail::document_access::create(run.scratch, run.work.budget().max_work_steps)));
      }
      const auto name = member.member_name();
      if (!name.has_value())
      {
        return view_result::failure(*access_failure(name.error_if()).error_if());
      }
      pod_vector<std::uint16_t> units{run.scratch};
      std::size_t index = 0U;
      while (index < (*name.value_if())->size())
      {
        const auto unit = (*name.value_if())->unit_at(index);
        const auto pushed = unit.has_value() ? units.push_back(*unit.value_if()) : access_failure(unit.error_if());
        if (!pushed.has_value())
        {
          return view_result::failure(*pushed.error_if());
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.82)
        ++index;
      }
      auto copy = decoded_string::copy_units(run.scratch, units.writable(), units.size());
      // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
      // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
      // once.
      // codeql::misra_deviation_next_line(DEV-108.6)
      auto made = copy.has_value() ? detail::document_access::make_string(*run.names, std::move(*copy.value_if()))
                                   : result<value_index>::failure(*copy.error_if());
      const auto view =
        made.has_value() ? run.names->view(*made.value_if()) : result<value_view>::failure(*made.error_if());
      return view.has_value() ? view_result::success(*view.value_if())
                              : view_result::failure(*access_failure(view.error_if()).error_if());
    }

    /**
     * @brief propertyNames: every member name, as a string, must match the schema (P051).
     * @par Requirements
     * P051; J5-057; Draft-07 Validation section 6.5.8.
     * @note Name failures are not diagnostics; one propertyNames diagnostic marks the object.
     * @par ID SWU-SCHEMA-305
     */
    advance_result advance_property_names(evaluator& run, frame& owner, const schema_node& node,
                                          std::optional<bool>& delivered) noexcept
    {
      const bool failed_name = delivered.has_value() && !*delivered;
      delivered.reset();
      if (failed_name)
      {
        const auto failed = fail(run, owner, schema_keyword::property_names);
        if (!failed.has_value())
        {
          return advance_result::failure(*failed.error_if());
        }
      }
      const auto kind = kind_of(owner.instance);
      const bool applies = (node.property_names != c_no_node) && (kind == value_kind::object);
      if (applies && !failed_name)
      {
        const auto next = next_child(owner);
        if (!next.has_value())
        {
          return advance_result::failure(*next.error_if());
        }
        if (next.value_if()->has_value())
        {
          const auto name = name_value(run, **next.value_if());
          if (!name.has_value())
          {
            return advance_result::failure(*name.error_if());
          }
          return advance_result::success(
            advance_outcome{child_request{node.property_names, *name.value_if(), true, true, false, true}});
        }
      }
      owner.started = false;
      owner.stage = phase::finish;
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief Run the phases after items: contains, schema dependencies, propertyNames.
     * @par Requirements
     * P050; P051; J5-057.
     * @par ID SWU-SCHEMA-306
     */
    advance_result advance_late_phase(evaluator& run, frame& owner, const schema_node& node,
                                      std::optional<bool>& delivered) noexcept
    {
      if (owner.stage == phase::contains)
      {
        return advance_contains(run, owner, node, delivered);
      }
      if (owner.stage == phase::dependencies)
      {
        return advance_dependencies(run, owner, node, delivered);
      }
      return advance_property_names(run, owner, node, delivered);
    }

    /**
     * @brief Record a frame's annotation keywords when collecting (P053).
     * @par Requirements
     * P053; J5-058; J5-062.
     * @note Frames inside propertyNames are skipped: their instance is a name copy, not a value
     * of the instance document.
     * @par ID SWU-SCHEMA-319
     */
    status note_annotations(evaluator& run, const frame& owner, const schema_node& node) noexcept
    {
      if (!run.collect || !node.has_annotations || owner.in_names)
      {
        return status::success();
      }
      return run.annotations.push_back(located(run, owner, schema_keyword::none));
    }

    /**
     * @brief Evaluate a node's assertion keywords and record each failed one.
     * @par Requirements
     * P047; J5-057; J5-063.
     * @return success (also when keywords failed), or an operational failure.
     * @par ID SWU-SCHEMA-215
     */
    status run_assertions(evaluator& run, frame& owner, const schema_node& node) noexcept
    {
      const auto noted = note_annotations(run, owner, node);
      if (!noted.has_value())
      {
        return noted;
      }
      const auto failed = detail::check_assertions(run.schema, node, owner.instance, run.work, run.scratch);
      if (!failed.has_value())
      {
        return status::failure(*failed.error_if());
      }
      std::size_t index = 0U;
      while ((index < 64U) && !run.stop)
      {
        if ((*failed.value_if() & (detail::c_first_keyword_bit << index)) != 0U)
        {
          const auto recorded = fail(run, owner, static_cast<schema_keyword>(index));
          if (!recorded.has_value())
          {
            return recorded;
          }
        }
        ++index;
      }
      return status::success();
    }

    /**
     * @brief Run the current applicator phase of an object-schema frame.
     * @par Requirements
     * P046; P050; P051; J5-057.
     * @par ID SWU-SCHEMA-307
     */
    advance_result advance_stage(evaluator& run, frame& owner, const schema_node& node,
                                 std::optional<bool>& delivered) noexcept
    {
      if (owner.stage >= phase::contains)
      {
        return advance_late_phase(run, owner, node, delivered);
      }
      if (owner.stage == phase::properties)
      {
        return advance_members(run, owner, node, delivered);
      }
      if (owner.stage == phase::items)
      {
        return advance_descending(run, owner, node, delivered);
      }
      return advance_combinators(run, owner, node, delivered);
    }

    /**
     * @brief Advance one frame until it needs a child or finishes.
     * @par Requirements
     * P046; J5-053; J5-056; J5-057.
     * @param delivered The verdict of the child this frame requested last, if any.
     * @par ID SWU-SCHEMA-155
     */
    advance_result advance(evaluator& run, frame& owner, std::optional<bool> delivered) noexcept
    {
      const schema_node& node = compiled_schema_access::node(run.schema, owner.node);
      while (owner.stage != phase::finish)
      {
        if (owner.stage == phase::enter)
        {
          if (node.is_boolean)
          {
            owner.stage = phase::finish;
            if (!node.boolean_value)
            {
              const auto failed = fail(run, owner, schema_keyword::false_schema);
              if (!failed.has_value())
              {
                return advance_result::failure(*failed.error_if());
              }
            }
            continue;
          }
          if (node.ref != c_no_node)
          {
            owner.stage = phase::ref;
            return request(node.ref, owner.instance, owner.suppressed, false, true);
          }
          const auto asserted = run_assertions(run, owner, node);
          if (!asserted.has_value())
          {
            return advance_result::failure(*asserted.error_if());
          }
          owner.stage = may_stop(run, owner) ? phase::finish : phase::all_of;
          owner.cursor = 0U;
          continue;
        }
        if (owner.stage == phase::ref)
        {
          absorb(owner, delivered);
          owner.stage = phase::finish;
          continue;
        }
        const auto advanced = advance_stage(run, owner, node, delivered);
        if (!advanced.has_value() || advanced.value_if()->child.has_value())
        {
          return advanced;
        }
      }
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief Charge, check and push a requested child frame.
     * @par Requirements
     * P046; J5-048; J5-056; J5-065; ADR 0006 decision 12.
     * @return success, nonprogressing_reference when a `$ref` re-enters an active
     * (node, instance) pair, or a budget/storage failure.
     * @par ID SWU-SCHEMA-156
     */
    status push_child(evaluator& run, const child_request& child) noexcept
    {
      const auto charged =
        child.via_ref ? run.work.reference_step() : (child.branch ? run.work.branch_step() : status::success());
      if (!charged.has_value())
      {
        return charged;
      }
      const auto index = child.instance.index();
      if (!index.has_value())
      {
        return access_failure(index.error_if());
      }
      const std::size_t position = index.value_if()->position();
      const bool in_names =
        child.in_names || ((run.frames.size() != 0U) && run.frames.get(run.frames.size() - 1U).in_names);
      if (child.via_ref)
      {
        // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
        // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
        // call; the counter is only incremented by the loop statement.
        // codeql::misra_deviation_next_line(DEV-105.6)
        for (std::size_t level = 0U; level < run.frames.size(); ++level)
        {
          const frame active = run.frames.get(level);
          if (active.via_ref && (active.node == child.node) && (active.instance_position == position) &&
              (active.in_names == in_names))
          {
            return status::failure(schema_error::of(schema_error_code::nonprogressing_reference));
          }
        }
      }
      if (run.frames.size() >= run.work.budget().max_depth)
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_depth));
      }
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.10)
      frame next{};
      next.node = child.node;
      next.instance = child.instance;
      next.instance_position = position;
      next.suppressed = child.suppressed;
      next.via_ref = child.via_ref;
      next.in_names = in_names;
      next.annotation_mark = run.annotations.size();
      return run.frames.push_back(next);
    }

    /**
     * @brief Evaluate the root schema against the instance root.
     * @par Requirements
     * P046; J5-048; J5-063.
     * @return The root verdict, or an operational failure.
     * @par ID SWU-SCHEMA-157
     */
    result<bool, schema_error> run_frames(evaluator& run, const value_view& root) noexcept
    {
      const auto pushed =
        push_child(run, child_request{compiled_schema_access::root(run.schema), root, false, false, false});
      if (!pushed.has_value())
      {
        return result<bool, schema_error>::failure(*pushed.error_if());
      }
      std::optional<bool> delivered{};
      bool verdict = true;
      while (run.frames.size() != 0U)
      {
        const auto stepped = run.work.step();
        if (!stepped.has_value())
        {
          return result<bool, schema_error>::failure(*stepped.error_if());
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.23)
        frame current = run.frames.get(run.frames.size() - 1U);
        const auto advanced = advance(run, current, delivered);
        delivered.reset();
        if (!advanced.has_value())
        {
          return result<bool, schema_error>::failure(*advanced.error_if());
        }
        if (run.stop)
        {
          return result<bool, schema_error>::success(false);
        }
        if (advanced.value_if()->child.has_value())
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check,
          // the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.22)
          run.frames.set(run.frames.size() - 1U, current);
          const auto child = push_child(run, *advanced.value_if()->child);
          if (!child.has_value())
          {
            return result<bool, schema_error>::failure(*child.error_if());
          }
          continue;
        }
        run.frames.pop_back();
        if (!current.valid)
        {
          run.annotations.truncate(current.annotation_mark);
        }
        delivered = current.valid;
        verdict = current.valid;
      }
      return result<bool, schema_error>::success(verdict);
    }
  } // namespace

  /**
   * @brief Validate a parsed instance against a compiled schema.
   * @par Requirements
   * P046; J5-048; J5-049; J5-050; J5-053; J5-056; J5-059; J5-061; J5-063; J5-065; ADR 0006
   * decisions 3, 4, 5, 6, 7 and 12.
   * @par ID SWU-SCHEMA-139
   */
  result<validation_report, schema_error> validate(const compiled_schema& schema, const document& instance,
                                                   const validation_options& options) noexcept
  {
    using validate_result = result<validation_report, schema_error>;
    evaluator run{schema, options};
    if (schema.node_count() == 0U)
    {
      return validate_result::failure(schema_error::of(schema_error_code::internal_error));
    }
    const auto domain = detail::check_instance_domain(instance, run.work, options.work_storage);
    if (!domain.has_value())
    {
      return validate_result::failure(*domain.error_if());
    }
    const auto root = instance.root();
    if (!root.has_value())
    {
      return validate_result::failure(*access_failure(root.error_if()).error_if());
    }
    const auto verdict = run_frames(run, *root.value_if());
    if (!verdict.has_value())
    {
      return validate_result::failure(*verdict.error_if());
    }
    const std::size_t count = run.diagnostics.size();
    const bool valid = *verdict.value_if() && (count == 0U);
    const std::size_t annotation_count = valid ? run.annotations.size() : 0U;
    return validate_result::success(
      detail::validation_report_access::make(valid, run.truncated, run.diagnostics.release(), count,
                                             valid ? run.annotations.release() : owned_block{}, annotation_count));
  }

  namespace
  {
    /**
     * @brief Write text at offset when it fits and return the offset after it.
     * @par Requirements
     * P046; J5-064.
     * @par ID SWU-SCHEMA-158
     */
    std::size_t put(std::string_view text, char* buffer, std::size_t capacity, std::size_t offset) noexcept
    {
      for (const char c : text)
      {
        if (offset < capacity)
        {
          buffer[offset] = c;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.21)
        ++offset;
      }
      return offset;
    }

    /**
     * @brief Write a UTF-16 name range as an escaped UTF-8 pointer token.
     * @par Requirements
     * P046; J5-064; RFC 6901.
     * @par ID SWU-SCHEMA-159
     */
    std::size_t put_name(const compiled_schema& schema, node_range units, char* buffer, std::size_t capacity,
                         std::size_t offset) noexcept
    {
      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
      // call; the counter is only incremented by the loop statement.
      // codeql::misra_deviation_next_line(DEV-105.5)
      for (std::size_t index = 0U; index < units.count; ++index)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // Comparisons and arithmetic on std::uint16_t code units promote to int without changing the value; the
        // operands are unsigned values below 0x110000 and the results are used as such.
        // codeql::misra_deviation_next_line(DEV-101.20)
        std::uint32_t code_point = compiled_schema_access::unit(schema, units.first + index);
        if ((code_point >= 0xD800U) && (code_point <= 0xDBFFU) && ((index + 1U) < units.count))
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
          // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check,
          // the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
          // Comparisons and arithmetic on std::uint16_t code units promote to int without changing the value; the
          // operands are unsigned values below 0x110000 and the results are used as such.
          // codeql::misra_deviation_next_line(DEV-101.19)
          const std::uint32_t low = compiled_schema_access::unit(schema, units.first + index + 1U);
          if ((low >= 0xDC00U) && (low <= 0xDFFFU))
          {
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check,
            // the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time
            // bounds.
            // codeql::misra_deviation_next_line(DEV-101.18)
            code_point = 0x10000U + ((code_point - 0xD800U) << 10U) + (low - 0xDC00U);
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check,
            // the loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time
            // bounds.
            // codeql::misra_deviation_next_line(DEV-101.17)
            ++index;
          }
        }
        std::array<char, 4> bytes{};
        std::size_t length = 1U;
        if (code_point < 0x80U)
        {
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.11)
          bytes[0] = static_cast<char>(code_point);
        }
        else if (code_point < 0x800U)
        {
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.10)
          bytes[0] = static_cast<char>(0xC0U | (code_point >> 6U));
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.9)
          bytes[1] = static_cast<char>(0x80U | (code_point & 0x3FU));
          length = 2U;
        }
        else if (code_point < 0x10000U)
        {
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.8)
          bytes[0] = static_cast<char>(0xE0U | (code_point >> 12U));
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.7)
          bytes[1] = static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU));
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.6)
          bytes[2] = static_cast<char>(0x80U | (code_point & 0x3FU));
          length = 3U;
        }
        else
        {
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.5)
          bytes[0] = static_cast<char>(0xF0U | (code_point >> 18U));
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.4)
          bytes[1] = static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU));
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.3)
          bytes[2] = static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU));
          // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
          // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters
          // and code units by value, as RFC 3986, RFC 6901 and Unicode define them.
          // codeql::misra_deviation_next_line(DEV-102.2)
          bytes[3] = static_cast<char>(0x80U | (code_point & 0x3FU));
          length = 4U;
        }
        const std::size_t room = (offset < capacity) ? (capacity - offset) : 0U;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.16)
        offset += detail::escape_pointer_token(std::string_view{bytes.data(), length},
                                               buffer + ((offset < capacity) ? offset : 0U), room);
      }
      return offset;
    }

    /**
     * @brief Write a decimal index.
     * @par Requirements
     * P046; J5-064.
     * @par ID SWU-SCHEMA-160
     */
    std::size_t put_index(std::size_t value, char* buffer, std::size_t capacity, std::size_t offset) noexcept
    {
      std::array<char, 24> digits{};
      std::size_t count = 0U;
      do
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
        // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
        // codeql::misra_deviation_next_line(DEV-102.1)
        digits[count] = static_cast<char>('0' + static_cast<int>(value % 10U));
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.15)
        ++count;
        value /= 10U;
      } while (value != 0U);
      while (count != 0U)
      {
        --count;
        // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
        // MISRA justification: Comparisons and arithmetic on std::uint16_t code units promote to int without changing
        // the value; the operands are unsigned values below 0x110000 and the results are used as such.
        // codeql::misra_deviation_next_line(DEV-106.2)
        offset = put(std::string_view{&digits[count], 1U}, buffer, capacity, offset);
      }
      return offset;
    }
  } // namespace

  /**
   * @brief Write a diagnostic's schema location as a URI fragment JSON Pointer.
   * @par Requirements
   * P046; J5-064; RFC 6901; ADR 0006 decision 13.
   * @par ID SWU-SCHEMA-140
   */
  std::size_t format_schema_location(const compiled_schema& schema, const schema_diagnostic& diagnostic, char* buffer,
                                     std::size_t capacity) noexcept
  {
    // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
    // MISRA justification: Comparisons and arithmetic on std::uint16_t code units promote to int without changing the
    // value; the operands are unsigned values below 0x110000 and the results are used as such.
    // codeql::misra_deviation_next_line(DEV-106.1)
    std::size_t offset = put("#", buffer, capacity, 0U);
    if (diagnostic.schema_node >= schema.node_count())
    {
      return offset;
    }
    std::size_t depth = 0U;
    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
    // call; the counter is only incremented by the loop statement.
    // codeql::misra_deviation_next_line(DEV-105.4)
    for (std::size_t node = diagnostic.schema_node; compiled_schema_access::node(schema, node).parent != c_no_node;
         node = compiled_schema_access::node(schema, node).parent)
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.14)
      ++depth;
    }
    while (depth != 0U)
    {
      std::size_t node = diagnostic.schema_node;
      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
      // call; the counter is only incremented by the loop statement.
      // codeql::misra_deviation_next_line(DEV-105.3)
      for (std::size_t up = 1U; up < depth; ++up)
      {
        node = compiled_schema_access::node(schema, node).parent;
      }
      --depth;
      const schema_node& step = compiled_schema_access::node(schema, node);
      offset = put("/", buffer, capacity, offset);
      offset = put(keyword_name(step.step_keyword), buffer, capacity, offset);
      if (step.step == detail::step_kind::name)
      {
        offset = put("/", buffer, capacity, offset);
        offset = put_name(schema, step.step_name, buffer, capacity, offset);
      }
      else if (step.step == detail::step_kind::index)
      {
        offset = put("/", buffer, capacity, offset);
        offset = put_index(step.step_index, buffer, capacity, offset);
      }
      else
      {
      }
    }
    return offset;
  }

  namespace
  {
    /**
     * @brief One open container on the instance-location search path.
     * @par Requirements
     * P046; J5-064.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.8)
    struct path_frame
    {
        value_iterator children{};
        value_view current{};
        std::size_t index{};
        bool object{};
    };

    /**
     * @brief Write one path step: the member name (escaped UTF-8) or the array index.
     * @par Requirements
     * P046; J5-064; RFC 6901.
     * @par ID SWU-SCHEMA-165
     */
    result<std::size_t, schema_error> put_step(const path_frame& step, pod_vector<char>& text, char* buffer,
                                               std::size_t capacity, std::size_t offset) noexcept
    {
      using size_result = result<std::size_t, schema_error>;
      offset = put("/", buffer, capacity, offset);
      if (!step.object)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.13)
        return size_result::success(put_index(step.index - 1U, buffer, capacity, offset));
      }
      const auto name = step.current.member_name();
      if (!name.has_value())
      {
        return size_result::failure(*access_failure(name.error_if()).error_if());
      }
      const auto size = utf8_size(**name.value_if());
      if (!size.has_value())
      {
        return size_result::failure(*access_failure(size.error_if()).error_if());
      }
      text.truncate(0U);
      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
      // call; the counter is only incremented by the loop statement.
      // codeql::misra_deviation_next_line(DEV-105.2)
      for (std::size_t index = 0U; index < *size.value_if(); ++index)
      {
        const auto pushed = text.push_back('\0');
        if (!pushed.has_value())
        {
          return size_result::failure(*pushed.error_if());
        }
      }
      if (*size.value_if() != 0U)
      {
        const auto written = export_utf8(**name.value_if(), text.writable(), text.size());
        if (!written.has_value())
        {
          return size_result::failure(*access_failure(written.error_if()).error_if());
        }
      }
      const std::size_t room = (offset < capacity) ? (capacity - offset) : 0U;
      const std::string_view token =
        (text.size() == 0U) ? std::string_view{} : std::string_view{text.writable(), text.size()};
      return size_result::success(
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.12)
        offset + detail::escape_pointer_token(token, buffer + ((offset < capacity) ? offset : 0U), room));
    }

    /**
     * @brief Write every step of a found search path.
     * @par Requirements
     * P046; J5-064; RFC 6901.
     * @par ID SWU-SCHEMA-178
     */
    result<std::size_t, schema_error> put_path(const pod_vector<path_frame>& path, pod_vector<char>& text, char* buffer,
                                               std::size_t capacity) noexcept
    {
      std::size_t offset = 0U;
      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
      // call; the counter is only incremented by the loop statement.
      // codeql::misra_deviation_next_line(DEV-105.1)
      for (std::size_t level = 0U; level < path.size(); ++level)
      {
        const auto written = put_step(path.get(level), text, buffer, capacity, offset);
        if (!written.has_value())
        {
          return written;
        }
        offset = *written.value_if();
      }
      return result<std::size_t, schema_error>::success(offset);
    }

    /**
     * @brief Return a value's document position.
     * @par Requirements
     * P046; J5-064.
     * @par ID SWU-SCHEMA-166
     */
    result<std::size_t, schema_error> position_of(const value_view& value) noexcept
    {
      const auto index = value.index();
      if (!index.has_value())
      {
        return result<std::size_t, schema_error>::failure(*access_failure(index.error_if()).error_if());
      }
      return result<std::size_t, schema_error>::success(index.value_if()->position());
    }

    /**
     * @brief Open a container on the search path, or do nothing for a scalar.
     * @par Requirements
     * P046; J5-064.
     * @par ID SWU-SCHEMA-167
     */
    status open_container(const value_view& value, pod_vector<path_frame>& path) noexcept
    {
      const auto kind = kind_of(value);
      if ((kind != value_kind::array) && (kind != value_kind::object))
      {
        return status::success();
      }
      auto children = value.children();
      if (!children.has_value())
      {
        return access_failure(children.error_if());
      }
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.9)
      path_frame frame{};
      frame.children = *children.value_if();
      frame.object = kind == value_kind::object;
      return path.push_back(frame);
    }
  } // namespace

  /**
   * @brief Write a diagnostic's instance location as an RFC 6901 JSON Pointer.
   * @par Requirements
   * P046; J5-064; RFC 6901; ADR 0006 decision 13.
   * @par ID SWU-SCHEMA-164
   */
  result<std::size_t, schema_error> format_instance_location(const document& instance,
                                                             const schema_diagnostic& diagnostic, char* buffer,
                                                             std::size_t capacity,
                                                             const storage_callbacks& scratch) noexcept
  {
    using size_result = result<std::size_t, schema_error>;
    const auto root = instance.root();
    if (!root.has_value())
    {
      return size_result::failure(*access_failure(root.error_if()).error_if());
    }
    const auto root_position = position_of(*root.value_if());
    if (!root_position.has_value() || (*root_position.value_if() == diagnostic.instance_position))
    {
      return root_position.has_value() ? size_result::success(0U) : root_position;
    }
    pod_vector<path_frame> path{scratch};
    pod_vector<char> text{scratch};
    const auto opened = open_container(*root.value_if(), path);
    if (!opened.has_value())
    {
      return size_result::failure(*opened.error_if());
    }
    while (path.size() != 0U)
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.11)
      path_frame top = path.get(path.size() - 1U);
      const auto next = top.children.next();
      if (!next.has_value())
      {
        return size_result::failure(*access_failure(next.error_if()).error_if());
      }
      if (!next.value_if()->has_value())
      {
        path.pop_back();
        continue;
      }
      top.current = **next.value_if();
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.10)
      ++top.index;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.9)
      path.set(path.size() - 1U, top);
      const auto position = position_of(top.current);
      if (!position.has_value())
      {
        return size_result::failure(*position.error_if());
      }
      if (*position.value_if() == diagnostic.instance_position)
      {
        return put_path(path, text, buffer, capacity);
      }
      const auto child = open_container(top.current, path);
      if (!child.has_value())
      {
        return size_result::failure(*child.error_if());
      }
    }
    return size_result::failure(schema_error::of(schema_error_code::internal_error));
  }

  /**
   * @brief Take over another report; other becomes an empty valid report.
   * @par Requirements
   * P046.
   * @par ID SWU-SCHEMA-162
   */
  // MISRA rule: RULE-18-4-1 cpp/misra/exception-unfriendly-function-must-be-noexcept
  // MISRA rule: RULE-15-0-2 cpp/misra/invalid-signature-for-special-member-function
  // MISRA justification: Move construction and accessors are noexcept and leave the source empty; members have
  // default member initializers, so none is read before it is set.
  // codeql::misra_deviation_next_line(DEV-109.3)
  validation_report::validation_report(validation_report&& other) noexcept
      : m_diagnostics(std::move(other.m_diagnostics)), m_count(other.m_count), m_valid(other.m_valid),
        m_truncated(other.m_truncated), m_annotations(std::move(other.m_annotations)),
        m_annotation_count(other.m_annotation_count)
  {
    other.m_count = 0U;
    other.m_valid = true;
    other.m_truncated = false;
    other.m_annotation_count = 0U;
  }

  /**
   * @brief Return true when the instance satisfied the schema.
   * @par Requirements
   * P046; J5-063.
   * @par ID SWU-SCHEMA-135
   */
  bool validation_report::valid() const noexcept
  {
    return m_valid;
  }

  /**
   * @brief Return true when collect-all stopped at max_errors.
   * @par Requirements
   * P046; J5-063; ADR 0006 decision 12.
   * @par ID SWU-SCHEMA-136
   */
  bool validation_report::truncated() const noexcept
  {
    return m_truncated;
  }

  /**
   * @brief Return the number of stored diagnostics.
   * @par Requirements
   * P046; J5-063.
   * @par ID SWU-SCHEMA-137
   */
  std::size_t validation_report::diagnostic_count() const noexcept
  {
    return m_count;
  }

  /**
   * @brief Borrow diagnostic index, or nullptr when index >= diagnostic_count().
   * @par Requirements
   * P046; J5-064.
   * @par ID SWU-SCHEMA-138
   */
  const schema_diagnostic* validation_report::diagnostic(std::size_t index) const noexcept
  {
    if (index >= m_count)
    {
      return nullptr;
    }
    // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
    // MISRA justification: Address computation inside an owned block from caller storage, as in the parser storage
    // layer (DEV-002/DEV-003); the offset is within the block size that was allocated.
    // codeql::misra_deviation_next_line(DEV-107.3)
    return static_cast<const schema_diagnostic*>(m_diagnostics.data()) + index;
  }
  /**
   * @brief Return the number of annotation records.
   * @par Requirements
   * P053; J5-058.
   * @par ID SWU-SCHEMA-312
   */
  std::size_t validation_report::annotation_count() const noexcept
  {
    return m_annotation_count;
  }

  /**
   * @brief Borrow annotation record index, or nullptr when index >= annotation_count().
   * @par Requirements
   * P053; J5-058; J5-064.
   * @par ID SWU-SCHEMA-313
   */
  const schema_annotation* validation_report::annotation(std::size_t index) const noexcept
  {
    if (index >= m_annotation_count)
    {
      return nullptr;
    }
    return static_cast<const schema_annotation*>(m_annotations.data()) + index;
  }

  /**
   * @brief Borrow the annotation keywords of an annotation record's schema node.
   * @par Requirements
   * P053; J5-058; J5-062; J5-069.
   * @par ID SWU-SCHEMA-314
   */
  result<value_view, schema_error> annotation_values(const compiled_schema& schema,
                                                     const schema_annotation& annotation) noexcept
  {
    using view_result = result<value_view, schema_error>;
    const document* values = compiled_schema_access::values(schema);
    if ((annotation.schema_node >= schema.node_count()) || (values == nullptr))
    {
      return view_result::failure(schema_error::of(schema_error_code::internal_error));
    }
    const schema_node& node = compiled_schema_access::node(schema, annotation.schema_node);
    if (!node.has_annotations)
    {
      return view_result::failure(schema_error::of(schema_error_code::internal_error));
    }
    const auto view = values->view(node.annotations);
    return view.has_value() ? view_result::success(*view.value_if())
                            : view_result::failure(schema_error::from_storage(*view.error_if()));
  }
} // namespace adaptive_json5
