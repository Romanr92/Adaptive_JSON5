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
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "schema/instance_domain.hpp"
#include "schema/json_pointer.hpp"
#include "schema/pod_vector.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_work.hpp"

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
        static validation_report make(bool valid, bool truncated, owned_block&& diagnostics, std::size_t count) noexcept
        {
          validation_report report{};
          report.m_valid = valid;
          report.m_truncated = truncated;
          report.m_diagnostics = std::move(diagnostics);
          report.m_count = count;
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
      finish
    };

    /**
     * @brief One (schema node, instance value) evaluation on the explicit stack.
     * @par Requirements
     * P046; J5-048; J5-056.
     * @note suppressed frames evaluate a combinator branch whose own failures are not
     * diagnostics; via_ref marks frames entered through `$ref` for loop detection.
     */
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
    };

    /**
     * @brief A child evaluation requested by a frame.
     * @par Requirements
     * P046.
     */
    struct child_request
    {
        std::size_t node{};
        value_view instance{};
        bool suppressed{};
        bool branch{};
        bool via_ref{};
    };

    /**
     * @brief State of one validation run.
     * @par Requirements
     * P046; J5-063; J5-065; ADR 0006 decisions 7 and 12.
     */
    struct evaluator
    {
        const compiled_schema& schema;
        detail::schema_work work;
        pod_vector<frame> frames;
        pod_vector<schema_diagnostic> diagnostics;
        evaluation_mode mode;
        std::size_t diagnostic_limit;
        bool stop{};
        bool truncated{};

        /**
         * @brief Bind the schema, budget and storages; no allocation.
         * @par Requirements
         * P046.
         * @par ID SWU-SCHEMA-142
         */
        evaluator(const compiled_schema& compiled, const validation_options& options) noexcept
            : schema(compiled), work(options.budget, options.cancellation), frames(options.work_storage),
              diagnostics(options.report_storage), mode(options.mode),
              diagnostic_limit((options.mode == evaluation_mode::fail_fast) ? 1U : options.budget.max_errors)
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
      const schema_node& node = compiled_schema_access::node(run.schema, owner.node);
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
      for (std::size_t index = 0U; index < units.count; ++index)
      {
        const auto unit = name.unit_at(index);
        if (!unit.has_value())
        {
          return result<bool, schema_error>::failure(schema_error::from_storage(*unit.error_if()));
        }
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
      for (std::size_t index = 0U; index < node.properties.count; ++index)
      {
        const auto stepped = run.work.step();
        if (!stepped.has_value())
        {
          return result<std::size_t, schema_error>::failure(*stepped.error_if());
        }
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
     * @brief Advance the applicator phases that evaluate the same instance.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation sections 6.7 and 6.6.
     * @return A child request, an empty outcome to continue with the next phase, or a failure.
     * @par ID SWU-SCHEMA-153
     */
    advance_result advance_combinators(evaluator& run, frame& owner, const schema_node& node,
                                       std::optional<bool>& delivered) noexcept
    {
      const auto edge = [&run](node_range range, std::size_t index)
      {
        return compiled_schema_access::edge(run.schema, range.first + index);
      };
      if (owner.stage == phase::all_of)
      {
        absorb(owner, delivered);
        if (!may_stop(run, owner) && (owner.cursor < node.all_of.count))
        {
          ++owner.cursor;
          return request(edge(node.all_of, owner.cursor - 1U), owner.instance, owner.suppressed, true);
        }
        owner.stage = phase::any_of;
        owner.cursor = 0U;
        owner.passes = 0U;
      }
      else if (owner.stage == phase::any_of)
      {
        if (delivered.has_value())
        {
          owner.passes += *delivered ? 1U : 0U;
          delivered.reset();
        }
        if ((owner.passes == 0U) && (owner.cursor < node.any_of.count))
        {
          ++owner.cursor;
          return request(edge(node.any_of, owner.cursor - 1U), owner.instance, true, true);
        }
        if ((node.any_of.count != 0U) && (owner.passes == 0U))
        {
          const auto failed = fail(run, owner, schema_keyword::any_of);
          if (!failed.has_value())
          {
            return advance_result::failure(*failed.error_if());
          }
        }
        owner.stage = phase::one_of;
        owner.cursor = 0U;
        owner.passes = 0U;
      }
      else if (owner.stage == phase::one_of)
      {
        if (delivered.has_value())
        {
          owner.passes += *delivered ? 1U : 0U;
          delivered.reset();
        }
        if ((owner.passes < 2U) && (owner.cursor < node.one_of.count))
        {
          ++owner.cursor;
          return request(edge(node.one_of, owner.cursor - 1U), owner.instance, true, true);
        }
        if ((node.one_of.count != 0U) && (owner.passes != 1U))
        {
          const auto failed = fail(run, owner, schema_keyword::one_of);
          if (!failed.has_value())
          {
            return advance_result::failure(*failed.error_if());
          }
        }
        owner.stage = phase::not_schema;
        owner.cursor = 0U;
      }
      else if (owner.stage == phase::not_schema)
      {
        if (delivered.has_value())
        {
          const bool matched = *delivered;
          delivered.reset();
          if (matched)
          {
            const auto failed = fail(run, owner, schema_keyword::not_schema);
            if (!failed.has_value())
            {
              return advance_result::failure(*failed.error_if());
            }
          }
        }
        else if (node.not_schema != c_no_node)
        {
          return request(node.not_schema, owner.instance, true, true);
        }
        else
        {
        }
        owner.stage = phase::if_schema;
      }
      else if (owner.stage == phase::if_schema)
      {
        if (delivered.has_value())
        {
          owner.if_result = *delivered;
          delivered.reset();
          owner.stage = phase::then_else;
          owner.cursor = 0U;
        }
        else if (node.if_schema != c_no_node)
        {
          return request(node.if_schema, owner.instance, true, true);
        }
        else
        {
          owner.stage = phase::properties;
        }
      }
      else
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
      }
      if (may_stop(run, owner))
      {
        owner.stage = phase::finish;
      }
      return advance_result::success(advance_outcome{});
    }

    /**
     * @brief Advance properties and items, which descend into instance children.
     * @par Requirements
     * P046; J5-057; Draft-07 Validation sections 6.4.1 and 6.5.4.
     * @par ID SWU-SCHEMA-154
     */
    advance_result advance_descending(evaluator& run, frame& owner, const schema_node& node,
                                      std::optional<bool>& delivered) noexcept
    {
      absorb(owner, delivered);
      const auto kind = kind_of(owner.instance);
      const bool properties = owner.stage == phase::properties;
      const bool applies = properties ? ((kind == value_kind::object) && (node.properties.count != 0U))
                                      : ((kind == value_kind::array) &&
                                         ((node.items_single != c_no_node) || (node.items_array.count != 0U)));
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
        std::size_t target = c_no_node;
        if (properties)
        {
          const auto found = property_for(run, node, child);
          if (!found.has_value())
          {
            return advance_result::failure(*found.error_if());
          }
          target = *found.value_if();
        }
        else if (node.items_single != c_no_node)
        {
          target = node.items_single;
        }
        else if (owner.cursor < node.items_array.count)
        {
          target = compiled_schema_access::edge(run.schema, node.items_array.first + owner.cursor);
        }
        else
        {
          // additionalItems is evaluated from P047 on.
        }
        ++owner.cursor;
        if (target != c_no_node)
        {
          return request(target, child, owner.suppressed, false);
        }
      }
      owner.started = false;
      owner.cursor = 0U;
      owner.stage = (properties && !may_stop(run, owner)) ? phase::items : phase::finish;
      return advance_result::success(advance_outcome{});
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
          owner.stage = phase::all_of;
          owner.cursor = 0U;
          continue;
        }
        if (owner.stage == phase::ref)
        {
          absorb(owner, delivered);
          owner.stage = phase::finish;
          continue;
        }
        const auto advanced = ((owner.stage == phase::properties) || (owner.stage == phase::items))
                                ? advance_descending(run, owner, node, delivered)
                                : advance_combinators(run, owner, node, delivered);
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
      if (child.via_ref)
      {
        for (std::size_t level = 0U; level < run.frames.size(); ++level)
        {
          const frame active = run.frames.get(level);
          if (active.via_ref && (active.node == child.node) && (active.instance_position == position))
          {
            return status::failure(schema_error::of(schema_error_code::nonprogressing_reference));
          }
        }
      }
      if (run.frames.size() >= run.work.budget().max_depth)
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_depth));
      }
      frame next{};
      next.node = child.node;
      next.instance = child.instance;
      next.instance_position = position;
      next.suppressed = child.suppressed;
      next.via_ref = child.via_ref;
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
          run.frames.set(run.frames.size() - 1U, current);
          const auto child = push_child(run, *advanced.value_if()->child);
          if (!child.has_value())
          {
            return result<bool, schema_error>::failure(*child.error_if());
          }
          continue;
        }
        run.frames.pop_back();
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
    return validate_result::success(
      detail::validation_report_access::make(valid, run.truncated, run.diagnostics.release(), count));
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
      for (std::size_t index = 0U; index < units.count; ++index)
      {
        std::uint32_t code_point = compiled_schema_access::unit(schema, units.first + index);
        if ((code_point >= 0xD800U) && (code_point <= 0xDBFFU) && ((index + 1U) < units.count))
        {
          const std::uint32_t low = compiled_schema_access::unit(schema, units.first + index + 1U);
          if ((low >= 0xDC00U) && (low <= 0xDFFFU))
          {
            code_point = 0x10000U + ((code_point - 0xD800U) << 10U) + (low - 0xDC00U);
            ++index;
          }
        }
        char bytes[4] = {};
        std::size_t length = 1U;
        if (code_point < 0x80U)
        {
          bytes[0] = static_cast<char>(code_point);
        }
        else if (code_point < 0x800U)
        {
          bytes[0] = static_cast<char>(0xC0U | (code_point >> 6U));
          bytes[1] = static_cast<char>(0x80U | (code_point & 0x3FU));
          length = 2U;
        }
        else if (code_point < 0x10000U)
        {
          bytes[0] = static_cast<char>(0xE0U | (code_point >> 12U));
          bytes[1] = static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU));
          bytes[2] = static_cast<char>(0x80U | (code_point & 0x3FU));
          length = 3U;
        }
        else
        {
          bytes[0] = static_cast<char>(0xF0U | (code_point >> 18U));
          bytes[1] = static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU));
          bytes[2] = static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU));
          bytes[3] = static_cast<char>(0x80U | (code_point & 0x3FU));
          length = 4U;
        }
        const std::size_t room = (offset < capacity) ? (capacity - offset) : 0U;
        offset += detail::escape_pointer_token(std::string_view{bytes, length},
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
      char digits[24] = {};
      std::size_t count = 0U;
      do
      {
        digits[count] = static_cast<char>('0' + static_cast<int>(value % 10U));
        ++count;
        value /= 10U;
      } while (value != 0U);
      while (count != 0U)
      {
        --count;
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
    std::size_t offset = put("#", buffer, capacity, 0U);
    if (diagnostic.schema_node >= schema.node_count())
    {
      return offset;
    }
    std::size_t depth = 0U;
    for (std::size_t node = diagnostic.schema_node; compiled_schema_access::node(schema, node).parent != c_no_node;
         node = compiled_schema_access::node(schema, node).parent)
    {
      ++depth;
    }
    while (depth != 0U)
    {
      std::size_t node = diagnostic.schema_node;
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
    return static_cast<const schema_diagnostic*>(m_diagnostics.data()) + index;
  }
} // namespace adaptive_json5
