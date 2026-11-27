#include "schema/regex.hpp"

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "schema/pod_vector.hpp"
#include "schema/schema_graph.hpp"
#include "schema/schema_work.hpp"
#include "schema/unicode_properties.hpp"
#include "schema/unicode_property.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  namespace
  {
    using status = result<void, schema_error>;

    constexpr std::uint32_t c_max_code_point = 0x10FFFFU;
    constexpr std::uint32_t c_ascii_last = 0x7FU;
    constexpr std::size_t c_unbounded = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t c_max_repeat = 100000U;
    constexpr std::size_t c_count_zero = 0U;
    constexpr std::size_t c_count_one = 1U;
    constexpr std::size_t c_short_hex = 2U;          // \xHH
    constexpr std::size_t c_long_hex = 4U;           // \uHHHH
    constexpr std::uint32_t c_dollar = 0x24U;        // $
    constexpr std::uint32_t c_open_paren = 0x28U;    // (
    constexpr std::uint32_t c_close_paren = 0x29U;   // )
    constexpr std::uint32_t c_asterisk = 0x2AU;      // *
    constexpr std::uint32_t c_plus = 0x2BU;          // +
    constexpr std::uint32_t c_comma = 0x2CU;         // ,
    constexpr std::uint32_t c_hyphen = 0x2DU;        // -
    constexpr std::uint32_t c_full_stop = 0x2EU;     // .
    constexpr std::uint32_t c_digit_zero = 0x30U;    // 0
    constexpr std::uint32_t c_digit_one = 0x31U;     // 1
    constexpr std::uint32_t c_digit_nine = 0x39U;    // 9
    constexpr std::uint32_t c_exclamation = 0x21U;   // !
    constexpr std::uint32_t c_colon = 0x3AU;         // :
    constexpr std::uint32_t c_less = 0x3CU;          // <
    constexpr std::uint32_t c_control_modulus = 32U; // \cX matches X % 32.
    constexpr std::size_t c_max_property_name = 64U; // Longest accepted \p{...} text.
    constexpr std::uint32_t c_equals = 0x3DU;        // =
    constexpr std::uint32_t c_question = 0x3FU;      // ?
    constexpr std::uint32_t c_capital_a = 0x41U;     // A
    constexpr std::uint32_t c_capital_b = 0x42U;     // B
    constexpr std::uint32_t c_capital_d = 0x44U;     // D
    constexpr std::uint32_t c_capital_f = 0x46U;     // F
    constexpr std::uint32_t c_capital_p = 0x50U;     // P
    constexpr std::uint32_t c_capital_s = 0x53U;     // S
    constexpr std::uint32_t c_capital_w = 0x57U;     // W
    constexpr std::uint32_t c_capital_z = 0x5AU;     // Z
    constexpr std::uint32_t c_open_bracket = 0x5BU;  // [
    constexpr std::uint32_t c_backslash = 0x5CU;     // backslash
    constexpr std::uint32_t c_close_bracket = 0x5DU; // ]
    constexpr std::uint32_t c_caret = 0x5EU;         // ^
    constexpr std::uint32_t c_underscore = 0x5FU;    // _
    constexpr std::uint32_t c_small_a = 0x61U;       // a
    constexpr std::uint32_t c_small_b = 0x62U;       // b
    constexpr std::uint32_t c_small_c = 0x63U;       // c
    constexpr std::uint32_t c_small_d = 0x64U;       // d
    constexpr std::uint32_t c_small_f = 0x66U;       // f
    constexpr std::uint32_t c_small_k = 0x6BU;       // k
    constexpr std::uint32_t c_small_n = 0x6EU;       // n
    constexpr std::uint32_t c_small_p = 0x70U;       // p
    constexpr std::uint32_t c_small_r = 0x72U;       // r
    constexpr std::uint32_t c_small_s = 0x73U;       // s
    constexpr std::uint32_t c_small_t = 0x74U;       // t
    constexpr std::uint32_t c_small_u = 0x75U;       // u
    constexpr std::uint32_t c_small_v = 0x76U;       // v
    constexpr std::uint32_t c_small_w = 0x77U;       // w
    constexpr std::uint32_t c_small_x = 0x78U;       // x
    constexpr std::uint32_t c_small_z = 0x7AU;       // z
    constexpr std::uint32_t c_open_brace = 0x7BU;    // {
    constexpr std::uint32_t c_vertical_bar = 0x7CU;  // |
    constexpr std::uint32_t c_close_brace = 0x7DU;   // }

    /**
     * @brief Parse tree node kinds.
     * @par Requirements
     * P049; J5-060.
     */
    enum class ast_kind : std::uint8_t
    {
      empty,
      literal,
      any,
      char_class,
      assertion,
      concat,
      alternation,
      repeat
    };

    /**
     * @brief Parse tree node; first/count index the child list (concat, alternation) or the
     * range table (char_class); repeat keeps its child in first.
     * @par Requirements
     * P049; J5-060.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.38)
    struct ast_node
    {
        ast_kind kind{ast_kind::empty};
        regex_op assertion{regex_op::match};
        bool negated{};
        bool greedy{true};
        std::uint32_t value{};
        std::size_t first{};
        std::size_t count{};
        std::size_t min{};
        std::size_t max{};
    };

    /**
     * @brief An open group: where its alternatives and current terms start.
     * @par Requirements
     * P049; J5-060.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.37)
    struct group_context
    {
        std::size_t alt_first{};
        std::size_t term_first{};
    };

    /**
     * @brief Parser and emitter state of one pattern.
     * @par Requirements
     * P049; J5-060.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.36)
    struct regex_builder
    {
        pod_vector<std::uint32_t> text;
        pod_vector<ast_node> nodes;
        pod_vector<std::size_t> children;
        pod_vector<std::size_t> terms;
        pod_vector<std::size_t> alts;
        pod_vector<group_context> groups;
        pod_vector<regex_inst>& code;
        pod_vector<regex_range>& ranges;
        std::size_t& steps;
        std::size_t step_limit;
        std::optional<source_span> span;
        std::size_t position{};
        bool quantifiable{};

        /**
         * @brief Bind the output tables and scratch storage; no allocation.
         * @par Requirements
         * P049.
         * @par ID SWU-SCHEMA-234
         */
        regex_builder(pod_vector<regex_inst>& out_code, pod_vector<regex_range>& out_ranges,
                      const storage_callbacks& scratch, std::size_t& used_steps, std::size_t limit,
                      std::optional<source_span> where) noexcept
            : text(scratch), nodes(scratch), children(scratch), terms(scratch), alts(scratch), groups(scratch),
              code(out_code), ranges(out_ranges), steps(used_steps), step_limit(limit), span(where)
        {
        }
    };

    /**
     * @brief Return an invalid_pattern or unsupported_pattern failure at the pattern.
     * @par Requirements
     * P049; J5-054; J5-060.
     * @par ID SWU-SCHEMA-235
     */
    status pattern_error(const regex_builder& builder, bool unsupported) noexcept
    {
      return status::failure(schema_error::invalid(unsupported ? invalid_schema_reason::unsupported_pattern
                                                               : invalid_schema_reason::invalid_pattern,
                                                   builder.span));
    }

    /**
     * @brief Charge one compile step.
     * @par Requirements
     * P049; J5-047.
     * @par ID SWU-SCHEMA-236
     */
    status charge(regex_builder& builder) noexcept
    {
      if (builder.steps >= builder.step_limit)
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_compile_steps));
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.149)
      ++builder.steps;
      return status::success();
    }

    /**
     * @brief Append a parse tree node and return its index through index.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-237
     */
    status add_node(regex_builder& builder, const ast_node& node, std::size_t& index) noexcept
    {
      const auto charged = charge(builder);
      if (!charged.has_value())
      {
        return charged;
      }
      index = builder.nodes.size();
      return builder.nodes.push_back(node);
    }

    /**
     * @brief Push a new term (an atom or assertion) to the current alternative.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-238
     */
    status push_term(regex_builder& builder, const ast_node& node, bool quantifiable) noexcept
    {
      std::size_t index = 0U;
      const auto added = add_node(builder, node, index);
      builder.quantifiable = quantifiable;
      return added.has_value() ? builder.terms.push_back(index) : added;
    }

    /**
     * @brief Decode the pattern's UTF-16 units into code points.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-239
     */
    status decode_text(const decoded_string& source, pod_vector<std::uint32_t>& out) noexcept
    {
      std::size_t index = 0U;
      while (index < source.size())
      {
        const auto unit = source.unit_at(index);
        if (!unit.has_value())
        {
          return status::failure(schema_error::from_storage(*unit.error_if()));
        }
        std::uint32_t value = static_cast<std::uint32_t>(*unit.value_if());
        if ((value >= 0xD800U) && (value <= 0xDBFFU) && ((index + 1U) < source.size()))
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.148)
          const auto low = source.unit_at(index + 1U);
          if (low.has_value() && (*low.value_if() >= 0xDC00U) && (*low.value_if() <= 0xDFFFU))
          {
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check,
            // the loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves
            // checked before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
            // codeql::misra_deviation_next_line(DEV-101.147)
            value = 0x10000U + ((value - 0xD800U) << 10U) + (*low.value_if() - 0xDC00U);
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check,
            // the loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves
            // checked before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
            // codeql::misra_deviation_next_line(DEV-101.146)
            ++index;
          }
        }
        const auto pushed = out.push_back(value);
        if (!pushed.has_value())
        {
          return pushed;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.145)
        ++index;
      }
      return status::success();
    }

    /**
     * @brief Shorthand class sets: \d, \w, \s.
     * @par Requirements
     * P049; J5-060; ECMA-262 5.1 section 15.10.2.12.
     */
    constexpr std::array<regex_range, 1> c_digit{{{0x30U, 0x39U}}};
    constexpr std::array<regex_range, 4> c_word{{{0x30U, 0x39U}, {0x41U, 0x5AU}, {0x5FU, 0x5FU}, {0x61U, 0x7AU}}};
    constexpr std::array<regex_range, 10> c_space{{{0x09U, 0x0DU},
                                                   {0x20U, 0x20U},
                                                   {0xA0U, 0xA0U},
                                                   {0x1680U, 0x1680U},
                                                   {0x2000U, 0x200AU},
                                                   {0x2028U, 0x2029U},
                                                   {0x202FU, 0x202FU},
                                                   {0x205FU, 0x205FU},
                                                   {0x3000U, 0x3000U},
                                                   {0xFEFFU, 0xFEFFU}}};

    /**
     * @brief Append a shorthand set, or its complement within [0, 0x10FFFF], to the range table.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-240
     */
    template <std::size_t N>
    status append_set(regex_builder& builder, const std::array<regex_range, N>& set, bool complement) noexcept
    {
      std::uint32_t next = 0U;
      for (const regex_range& range : set)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.144)
        const regex_range piece = complement ? regex_range{next, range.low - 1U} : range;
        const bool emit = !complement || (range.low > next);
        const auto pushed = emit ? builder.ranges.push_back(piece) : status::success();
        if (!pushed.has_value())
        {
          return pushed;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.143)
        next = range.high + 1U;
      }
      return complement ? builder.ranges.push_back(regex_range{next, c_max_code_point}) : status::success();
    }

    /**
     * @brief Append the set of a shorthand escape letter (d D w W s S); false for other letters.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-241
     */
    result<bool, schema_error> append_shorthand(regex_builder& builder, std::uint32_t letter) noexcept
    {
      using bool_result = result<bool, schema_error>;
      const bool digit = (letter == c_small_d) || (letter == c_capital_d);
      const bool word = (letter == c_small_w) || (letter == c_capital_w);
      const bool space = (letter == c_small_s) || (letter == c_capital_s);
      if (!digit && !word && !space)
      {
        return bool_result::success(false);
      }
      const status appended = digit  ? append_set(builder, c_digit, letter == c_capital_d)
                              : word ? append_set(builder, c_word, letter == c_capital_w)
                                     : append_set(builder, c_space, letter == c_capital_s);
      return appended.has_value() ? bool_result::success(true) : bool_result::failure(*appended.error_if());
    }

    /**
     * @brief Return the value of a hexadecimal digit, or nullopt.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-242
     */
    std::optional<std::uint32_t> hex_digit(std::uint32_t c) noexcept
    {
      if ((c >= c_digit_zero) && (c <= c_digit_nine))
      {
        return c - c_digit_zero;
      }
      if ((c >= c_small_a) && (c <= c_small_f))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.142)
        return (c - c_small_a) + 10U;
      }
      if ((c >= c_capital_a) && (c <= c_capital_f))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.141)
        return (c - c_capital_a) + 10U;
      }
      return std::nullopt;
    }

    /**
     * @brief Read count hexadecimal digits after position; nullopt when they are not all there.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-243
     */
    std::optional<std::uint32_t> read_hex(const regex_builder& builder, std::size_t count) noexcept
    {
      std::uint32_t value = 0U;
      std::size_t index = 0U;
      while (index < count)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.140)
        const std::size_t at = builder.position + index;
        const auto digit = (at < builder.text.size()) ? hex_digit(builder.text.get(at)) : std::nullopt;
        if (!digit.has_value())
        {
          return std::nullopt;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.139)
        value = (value * 16U) + *digit;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.138)
        ++index;
      }
      return value;
    }

    /**
     * @brief Result of reading an escape: a code point, a shorthand set (appended to the range
     * table), or an assertion.
     * @par Requirements
     * P049; J5-060.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.35)
    struct escape_value
    {
        std::optional<std::uint32_t> code_point{};
        bool set{};
        regex_op assertion{regex_op::match};
    };

    /**
     * @brief Map a control escape letter (t n v f r) to its code point.
     * @par Requirements
     * P049; J5-060; ECMA-262 5.1 section 15.10.2.10.
     * @par ID SWU-SCHEMA-244
     */
    std::optional<std::uint32_t> control_escape(std::uint32_t letter) noexcept
    {
      constexpr std::array<std::array<std::uint32_t, 2>, 5> c_controls{
        {{c_small_t, 0x09U}, {c_small_n, 0x0AU}, {c_small_v, 0x0BU}, {c_small_f, 0x0CU}, {c_small_r, 0x0DU}}};
      for (const auto& entry : c_controls)
      {
        if (entry[0] == letter)
        {
          return entry[1];
        }
      }
      return std::nullopt;
    }

    /**
     * @brief Report whether an escape letter names an unsupported feature (backreference, octal,
     * named reference, \B in a class).
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-282
     */
    bool is_unsupported_escape(const regex_builder& builder, std::uint32_t letter, bool in_class) noexcept
    {
      const bool next_is_digit = (builder.position < builder.text.size()) &&
                                 (builder.text.get(builder.position) >= c_digit_zero) &&
                                 (builder.text.get(builder.position) <= c_digit_nine);
      const bool reference = (letter >= c_digit_one) && (letter <= c_digit_nine);
      const bool octal = (letter == c_digit_zero) && next_is_digit;
      return reference || octal || (letter == c_small_k) || (in_class && (letter == c_capital_b));
    }

    /**
     * @brief Read the control letter of \c (position is past the c). Without a letter (or, in a
     * class, a digit or '_') the backslash is literal and 'c' is read again (Annex B).
     * @par Requirements
     * P049; J5-060; ECMA-262 sections 22.2.1.1 and B.1.2; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-368
     */
    std::uint32_t control_letter(regex_builder& builder, bool in_class) noexcept
    {
      const std::uint32_t next = (builder.position < builder.text.size()) ? builder.text.get(builder.position) : 0U;
      const bool letter =
        ((next >= c_capital_a) && (next <= c_capital_z)) || ((next >= c_small_a) && (next <= c_small_z));
      const bool class_extra =
        in_class && (((next >= c_digit_zero) && (next <= c_digit_nine)) || (next == c_underscore));
      if (letter || class_extra)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.205)
        ++builder.position;
        return next % c_control_modulus;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.204)
      --builder.position;
      return c_backslash;
    }

    /**
     * @brief Return the code point of a character escape: control letters, \0, \xHH, \uHHHH,
     * or the letter itself (identity escape, Annex B).
     * @par Requirements
     * P049; J5-060; ECMA-262 5.1 section 15.10.2.10 and Annex B.
     * @par ID SWU-SCHEMA-283
     */
    std::uint32_t escaped_character(regex_builder& builder, std::uint32_t letter) noexcept
    {
      const auto control = control_escape(letter);
      if (control.has_value())
      {
        return *control;
      }
      if (letter == c_digit_zero)
      {
        return 0U;
      }
      const std::size_t digits =
        (letter == c_small_x) ? c_short_hex : ((letter == c_small_u) ? c_long_hex : c_count_zero);
      const auto hex = (digits != 0U) ? read_hex(builder, digits) : std::nullopt;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.137)
      builder.position += hex.has_value() ? digits : 0U;
      return hex.has_value() ? *hex : letter;
    }

    /**
     * @brief ECMA-262 binary property names and aliases that \p{...} does not support (only Any,
     * ASCII and Assigned are), so a pattern using them is unsupported rather than malformed.
     * @par Requirements
     * P049; J5-060; ECMA-262 table "Binary Unicode property aliases"; ADR 0006 (issue #187).
     */
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.13)
    constexpr std::array<std::string_view, 95> c_unsupported_binary_properties{{
      "AHex",
      "ASCII_Hex_Digit",
      "Alpha",
      "Alphabetic",
      "Bidi_C",
      "Bidi_Control",
      "Bidi_M",
      "Bidi_Mirrored",
      "CI",
      "CWCF",
      "CWCM",
      "CWKCF",
      "CWL",
      "CWT",
      "CWU",
      "Case_Ignorable",
      "Cased",
      "Changes_When_Casefolded",
      "Changes_When_Casemapped",
      "Changes_When_Lowercased",
      "Changes_When_NFKC_Casefolded",
      "Changes_When_Titlecased",
      "Changes_When_Uppercased",
      "DI",
      "Dash",
      "Default_Ignorable_Code_Point",
      "Dep",
      "Deprecated",
      "Dia",
      "Diacritic",
      "EBase",
      "EComp",
      "EMod",
      "EPres",
      "Emoji",
      "Emoji_Component",
      "Emoji_Modifier",
      "Emoji_Modifier_Base",
      "Emoji_Presentation",
      "Ext",
      "ExtPict",
      "Extended_Pictographic",
      "Extender",
      "Gr_Base",
      "Gr_Ext",
      "Grapheme_Base",
      "Grapheme_Extend",
      "Hex",
      "Hex_Digit",
      "IDC",
      "IDS",
      "IDSB",
      "IDST",
      "IDS_Binary_Operator",
      "IDS_Trinary_Operator",
      "ID_Continue",
      "ID_Start",
      "Ideo",
      "Ideographic",
      "Join_C",
      "Join_Control",
      "LOE",
      "Logical_Order_Exception",
      "Lower",
      "Lowercase",
      "Math",
      "NChar",
      "Noncharacter_Code_Point",
      "Pat_Syn",
      "Pat_WS",
      "Pattern_Syntax",
      "Pattern_White_Space",
      "QMark",
      "Quotation_Mark",
      "RI",
      "Radical",
      "Regional_Indicator",
      "SD",
      "STerm",
      "Sentence_Terminal",
      "Soft_Dotted",
      "Term",
      "Terminal_Punctuation",
      "UIdeo",
      "Unified_Ideograph",
      "Upper",
      "Uppercase",
      "VS",
      "Variation_Selector",
      "White_Space",
      "XIDC",
      "XIDS",
      "XID_Continue",
      "XID_Start",
      "space",
    }};

    /**
     * @brief The code points a \p{...} name selects: a General_Category mask (gaps are Cn) or one
     * script number (gaps are Unknown), or ASCII.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.41)
    struct property_selector
    {
        bool script{};
        bool ascii{};
        std::uint32_t value{};
    };

    /**
     * @brief Report whether a table entry value (category number or script number) is selected.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-369
     */
    bool selects(const property_selector& selector, std::uint32_t entry) noexcept
    {
      return selector.script ? (entry == selector.value) : (((selector.value >> entry) & 1U) != 0U);
    }

    /**
     * @brief Append [from, to] to the ranges of the current set, merging with the previous range
     * of the same set when adjacent; one compile step per new range.
     * @par Requirements
     * P049; J5-047; J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-370
     */
    status add_run(regex_builder& builder, std::size_t first_range, std::uint32_t from, std::uint32_t to) noexcept
    {
      const std::size_t count = builder.ranges.size();
      if (count > first_range)
      {
        const regex_range last = builder.ranges.get(count - 1U);
        if ((last.high + 1U) == from)
        {
          builder.ranges.set(count - 1U, regex_range{last.low, to});
          return status::success();
        }
      }
      const status charged = charge(builder);
      return charged.has_value() ? builder.ranges.push_back(regex_range{from, to}) : charged;
    }

    /**
     * @brief Append the code points of a table selection (or its complement) as merged ranges.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     * @param gap Whether code points missing from the table (Cn, or script Unknown) are selected.
     * @par ID SWU-SCHEMA-371
     */
    template <std::size_t N>
    status append_table(regex_builder& builder, const std::array<unicode_value_range, N>& table,
                        const property_selector& selector, bool gap, bool negated) noexcept
    {
      const std::size_t first_range = builder.ranges.size();
      std::uint32_t next = 0U;
      status added = status::success();
      for (const unicode_value_range& entry : table)
      {
        if (added.has_value() && (entry.first > next) && (gap != negated))
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.203)
          added = add_run(builder, first_range, next, entry.first - 1U);
        }
        if (added.has_value() && (selects(selector, static_cast<std::uint32_t>(entry.value)) != negated))
        {
          added = add_run(builder, first_range, entry.first, entry.last);
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.202)
        next = entry.last + 1U;
      }
      if (added.has_value() && (next <= c_max_code_point) && (gap != negated))
      {
        added = add_run(builder, first_range, next, c_max_code_point);
      }
      return added;
    }

    /**
     * @brief Append the set of a property selector, negated for \P.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-372
     */
    status append_selection(regex_builder& builder, const property_selector& selector, bool negated) noexcept
    {
      if (selector.ascii)
      {
        return negated ? builder.ranges.push_back(regex_range{c_ascii_last + 1U, c_max_code_point})
                       : builder.ranges.push_back(regex_range{0U, c_ascii_last});
      }
      if (selector.script)
      {
        return append_table(builder, c_script_ranges, selector, selector.value == c_unknown_script, negated);
      }
      return append_table(builder, c_general_category_ranges, selector,
                          (selector.value & c_unassigned_category_mask) != 0U, negated);
    }

    /**
     * @brief Result of resolving a \p{...} text: a selector, unsupported or malformed.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     */
    using selector_result = result<property_selector, schema_error>;

    /**
     * @brief Resolve a lone \p{name}: a General_Category value, Any, ASCII or Assigned.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-373
     */
    selector_result lone_property(const regex_builder& builder, std::string_view name) noexcept
    {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.55)
      property_selector selector{};
      const auto mask = general_category_mask(name);
      if (mask.has_value())
      {
        selector.value = *mask;
        return selector_result::success(selector);
      }
      if ((name == "Any") || (name == "Assigned"))
      {
        selector.value = (name == "Any") ? 0xFFFFFFFFU : ~c_unassigned_category_mask;
        return selector_result::success(selector);
      }
      if (name == "ASCII")
      {
        selector.ascii = true;
        return selector_result::success(selector);
      }
      for (const std::string_view binary : c_unsupported_binary_properties)
      {
        if (binary == name)
        {
          return selector_result::failure(*pattern_error(builder, true).error_if());
        }
      }
      return selector_result::failure(*pattern_error(builder, false).error_if());
    }

    /**
     * @brief Resolve \p{property=value}: General_Category or Script.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-374
     */
    selector_result named_property(const regex_builder& builder, std::string_view property,
                                   std::string_view value) noexcept
    {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.54)
      property_selector selector{};
      if ((property == "General_Category") || (property == "gc"))
      {
        const auto mask = general_category_mask(value);
        selector.value = mask.has_value() ? *mask : 0U;
        return mask.has_value() ? selector_result::success(selector)
                                : selector_result::failure(*pattern_error(builder, false).error_if());
      }
      if ((property == "Script") || (property == "sc"))
      {
        const auto number = script_number(value);
        selector.script = true;
        selector.value = number.has_value() ? *number : 0U;
        return number.has_value() ? selector_result::success(selector)
                                  : selector_result::failure(*pattern_error(builder, false).error_if());
      }
      const bool extensions = (property == "Script_Extensions") || (property == "scx");
      return selector_result::failure(
        *pattern_error(builder, extensions && script_number(value).has_value()).error_if());
    }

    /**
     * @brief Read the {...} text of \p or \P (position is past the p) into name; false when it is
     * missing, unterminated, empty, too long or not ASCII.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-375
     */
    bool read_property_text(regex_builder& builder, std::array<char, c_max_property_name>& name,
                            std::size_t& length) noexcept
    {
      length = 0U;
      if ((builder.position >= builder.text.size()) || (builder.text.get(builder.position) != c_open_brace))
      {
        return false;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.201)
      ++builder.position;
      while ((builder.position < builder.text.size()) && (builder.text.get(builder.position) != c_close_brace))
      {
        const std::uint32_t c = builder.text.get(builder.position);
        if ((c > c_ascii_last) || (length >= name.size()))
        {
          return false;
        }
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
        // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
        // codeql::misra_deviation_next_line(DEV-102.36)
        name[length] = static_cast<char>(c);
        ++length;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.200)
        ++builder.position;
      }
      if ((builder.position >= builder.text.size()) || (length == 0U))
      {
        return false;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.199)
      ++builder.position;
      return true;
    }

    /**
     * @brief Parse \p{...} or \P{...} (position is past the p) and append its code points.
     * @par Requirements
     * P049; J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-376
     */
    status append_property(regex_builder& builder, bool negated) noexcept
    {
      std::array<char, c_max_property_name> name{};
      std::size_t length = 0U;
      if (!read_property_text(builder, name, length))
      {
        return pattern_error(builder, false);
      }
      const std::string_view text{name.data(), length};
      const std::size_t equals = text.find('=');
      if (equals == std::string_view::npos)
      {
        const selector_result lone = lone_property(builder, text);
        return lone.has_value() ? append_selection(builder, *lone.value_if(), negated)
                                : status::failure(*lone.error_if());
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.198)
      const std::string_view value = text.substr(equals + 1U);
      const selector_result named = named_property(builder, text.substr(0U, equals), value);
      return named.has_value() ? append_selection(builder, *named.value_if(), negated)
                               : status::failure(*named.error_if());
    }

    /**
     * @brief Read the escape after a backslash (position is past the backslash).
     * @par Requirements
     * P049; J5-060; ECMA-262 5.1 sections 15.10.2.10 to 15.10.2.12 and Annex B.
     * @param in_class Inside a class \b is backspace and \B is unsupported.
     * @par ID SWU-SCHEMA-245
     */
    result<escape_value, schema_error> read_escape(regex_builder& builder, bool in_class) noexcept
    {
      using escape_result = result<escape_value, schema_error>;
      if (builder.position >= builder.text.size())
      {
        return escape_result::failure(*pattern_error(builder, false).error_if());
      }
      const std::uint32_t letter = builder.text.get(builder.position);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.136)
      ++builder.position;
      escape_value value{};
      const auto shorthand = append_shorthand(builder, letter);
      if (!shorthand.has_value() || *shorthand.value_if())
      {
        value.set = true;
        return shorthand.has_value() ? escape_result::success(value) : escape_result::failure(*shorthand.error_if());
      }
      if ((letter == c_small_p) || (letter == c_capital_p))
      {
        const status appended = append_property(builder, letter == c_capital_p);
        value.set = true;
        return appended.has_value() ? escape_result::success(value) : escape_result::failure(*appended.error_if());
      }
      if (letter == c_small_c)
      {
        value.code_point = control_letter(builder, in_class);
        return escape_result::success(value);
      }
      if (is_unsupported_escape(builder, letter, in_class))
      {
        return escape_result::failure(*pattern_error(builder, true).error_if());
      }
      if ((letter == c_small_b) || (letter == c_capital_b))
      {
        value.code_point = in_class ? std::optional<std::uint32_t>{0x08U} : std::nullopt;
        value.assertion = (letter == c_small_b) ? regex_op::word_boundary : regex_op::not_word_boundary;
        return escape_result::success(value);
      }
      value.code_point = escaped_character(builder, letter);
      return escape_result::success(value);
    }

    /**
     * @brief Read one class element: a code point, or a shorthand set appended to the ranges.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-246
     */
    result<escape_value, schema_error> read_class_atom(regex_builder& builder) noexcept
    {
      using escape_result = result<escape_value, schema_error>;
      const std::uint32_t c = builder.text.get(builder.position);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.135)
      ++builder.position;
      if (c != c_backslash)
      {
        escape_value value{};
        value.code_point = c;
        return escape_result::success(value);
      }
      return read_escape(builder, true);
    }

    /**
     * @brief Append one class element: a character, a shorthand set or a range a-b. A range with
     * a shorthand end keeps its characters and '-' literally (Annex B).
     * @par Requirements
     * P049; J5-060; ECMA-262 5.1 section 15.10.2.15 and Annex B.
     * @par ID SWU-SCHEMA-284
     */
    status add_class_element(regex_builder& builder) noexcept
    {
      const auto low = read_class_atom(builder);
      if (!low.has_value())
      {
        return status::failure(*low.error_if());
      }
      const bool dash = ((builder.position + 1U) < builder.text.size()) &&
                        (builder.text.get(builder.position) == c_hyphen) &&
                        (builder.text.get(builder.position + 1U) != c_close_bracket);
      if (low.value_if()->set)
      {
        return status::success();
      }
      const std::uint32_t from = *low.value_if()->code_point;
      if (!dash)
      {
        return builder.ranges.push_back(regex_range{from, from});
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.134)
      ++builder.position;
      const auto high = read_class_atom(builder);
      if (!high.has_value())
      {
        return status::failure(*high.error_if());
      }
      if (high.value_if()->set)
      {
        const auto pushed = builder.ranges.push_back(regex_range{from, from});
        return pushed.has_value() ? builder.ranges.push_back(regex_range{c_hyphen, c_hyphen}) : pushed;
      }
      const std::uint32_t to = *high.value_if()->code_point;
      return (to < from) ? pattern_error(builder, false) : builder.ranges.push_back(regex_range{from, to});
    }

    /**
     * @brief Parse a character class after '[' and push it as a term.
     * @par Requirements
     * P049; J5-060; ECMA-262 5.1 section 15.10.2.13 and Annex B.
     * @par ID SWU-SCHEMA-247
     */
    status parse_class(regex_builder& builder) noexcept
    {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.53)
      ast_node node{};
      node.kind = ast_kind::char_class;
      node.first = builder.ranges.size();
      if ((builder.position < builder.text.size()) && (builder.text.get(builder.position) == c_caret))
      {
        node.negated = true;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.133)
        ++builder.position;
      }
      status parsed = status::success();
      while (parsed.has_value() && (builder.position < builder.text.size()) &&
             (builder.text.get(builder.position) != c_close_bracket))
      {
        parsed = add_class_element(builder);
      }
      if (!parsed.has_value())
      {
        return parsed;
      }
      if (builder.position >= builder.text.size())
      {
        return pattern_error(builder, false);
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.132)
      ++builder.position;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.131)
      node.count = builder.ranges.size() - node.first;
      return push_term(builder, node, true);
    }

    /**
     * @brief Read a decimal number at position (saturating at c_max_repeat + 1); nullopt when none.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-248
     */
    std::optional<std::size_t> read_count(regex_builder& builder) noexcept
    {
      std::optional<std::size_t> value{};
      while ((builder.position < builder.text.size()) && (builder.text.get(builder.position) >= c_digit_zero) &&
             (builder.text.get(builder.position) <= c_digit_nine))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.130)
        const std::size_t digit = static_cast<std::size_t>(builder.text.get(builder.position) - c_digit_zero);
        const std::size_t current = value.value_or(0U);
        value = (current > c_max_repeat) ? current : ((current * 10U) + digit);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.129)
        ++builder.position;
      }
      return value;
    }

    /**
     * @brief Parse "{n}", "{n,}" or "{n,m}" after '{'; nullopt (position restored) when the text
     * is no quantifier, so that '{' is a literal (Annex B).
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-249
     */
    std::optional<std::array<std::size_t, 2>> read_braces(regex_builder& builder) noexcept
    {
      const std::size_t start = builder.position;
      const auto low = read_count(builder);
      std::optional<std::size_t> high = low;
      if (low.has_value() && (builder.position < builder.text.size()) &&
          (builder.text.get(builder.position) == c_comma))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.128)
        ++builder.position;
        high = read_count(builder);
        high = high.has_value() ? high : std::optional<std::size_t>{c_unbounded};
      }
      if (!low.has_value() || (builder.position >= builder.text.size()) ||
          (builder.text.get(builder.position) != c_close_brace))
      {
        builder.position = start;
        return std::nullopt;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.127)
      ++builder.position;
      return std::array<std::size_t, 2>{*low, *high};
    }

    /**
     * @brief Apply a quantifier to the last term of the current alternative.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-250
     */
    status apply_quantifier(regex_builder& builder, std::size_t min, std::size_t max) noexcept
    {
      if (!builder.quantifiable)
      {
        return pattern_error(builder, false);
      }
      if ((max != c_unbounded) && (min > max))
      {
        return pattern_error(builder, false);
      }
      if ((min > c_max_repeat) || ((max != c_unbounded) && (max > c_max_repeat)))
      {
        return status::failure(schema_error::limit(schema_limit_reason::max_compile_steps));
      }
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.52)
      ast_node node{};
      node.kind = ast_kind::repeat;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.126)
      node.first = builder.terms.get(builder.terms.size() - 1U);
      node.min = min;
      node.max = max;
      if ((builder.position < builder.text.size()) && (builder.text.get(builder.position) == c_question))
      {
        node.greedy = false;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.125)
        ++builder.position;
      }
      std::size_t index = 0U;
      const auto added = add_node(builder, node, index);
      if (!added.has_value())
      {
        return added;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.124)
      builder.terms.set(builder.terms.size() - 1U, index);
      builder.quantifiable = false;
      return status::success();
    }

    /**
     * @brief Close the current alternative: its terms become one concat node in the alternatives.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-251
     */
    status close_alternative(regex_builder& builder) noexcept
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.123)
      const group_context group = builder.groups.get(builder.groups.size() - 1U);
      const std::size_t count = builder.terms.size() - group.term_first;
      std::size_t index = 0U;
      status built = status::success();
      if (count == 1U)
      {
        index = builder.terms.get(group.term_first);
      }
      else
      {
        // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
        // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its
        // context; the value is zero or the stated constant and no conversion changes it.
        // codeql::misra_deviation_next_line(DEV-103.51)
        ast_node node{};
        node.kind = (count == 0U) ? ast_kind::empty : ast_kind::concat;
        node.first = builder.children.size();
        node.count = count;
        std::size_t term = group.term_first;
        while (built.has_value() && (term < builder.terms.size()))
        {
          built = builder.children.push_back(builder.terms.get(term));
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.122)
          ++term;
        }
        built = built.has_value() ? add_node(builder, node, index) : built;
      }
      builder.terms.truncate(group.term_first);
      return built.has_value() ? builder.alts.push_back(index) : built;
    }

    /**
     * @brief Close the current group: its alternatives become one node, returned through index.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-252
     */
    status close_group(regex_builder& builder, std::size_t& index) noexcept
    {
      const auto closed = close_alternative(builder);
      if (!closed.has_value())
      {
        return closed;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.121)
      const group_context group = builder.groups.get(builder.groups.size() - 1U);
      const std::size_t count = builder.alts.size() - group.alt_first;
      status built = status::success();
      if (count == 1U)
      {
        index = builder.alts.get(group.alt_first);
      }
      else
      {
        // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
        // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its
        // context; the value is zero or the stated constant and no conversion changes it.
        // codeql::misra_deviation_next_line(DEV-103.50)
        ast_node node{};
        node.kind = ast_kind::alternation;
        node.first = builder.children.size();
        node.count = count;
        std::size_t alt = group.alt_first;
        while (built.has_value() && (alt < builder.alts.size()))
        {
          built = builder.children.push_back(builder.alts.get(alt));
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.120)
          ++alt;
        }
        built = built.has_value() ? add_node(builder, node, index) : built;
      }
      builder.alts.truncate(group.alt_first);
      builder.groups.pop_back();
      return built;
    }

    /**
     * @brief Report whether the text after "(?" is ECMA-262 syntax this engine does not support:
     * lookahead "(?=" / "(?!", lookbehind "(?<=" / "(?<!" or a named group "(?<name>".
     * @par Requirements
     * P049; J5-060; ECMA-262 2018 sections 21.2.1 and 21.2.2.
     * @par ID SWU-SCHEMA-358
     */
    bool is_ecma_group_prefix(const regex_builder& builder) noexcept
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.182)
      const std::size_t at = builder.position + 1U;
      const std::uint32_t kind = (at < builder.text.size()) ? builder.text.get(at) : c_question;
      if ((kind == c_equals) || (kind == c_exclamation))
      {
        return true;
      }
      if ((kind != c_less) || ((at + 1U) >= builder.text.size()))
      {
        return false;
      }
      const std::uint32_t next = builder.text.get(at + 1U);
      const bool letter =
        ((next >= c_capital_a) && (next <= c_capital_z)) || ((next >= c_small_a) && (next <= c_small_z));
      return (next == c_equals) || (next == c_exclamation) || letter || (next == c_underscore) || (next == c_dollar) ||
             (next > c_ascii_last);
    }

    /**
     * @brief Handle '(' : open a group, or report lookaround and named groups as unsupported.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-253
     */
    status open_group(regex_builder& builder) noexcept
    {
      if ((builder.position < builder.text.size()) && (builder.text.get(builder.position) == c_question))
      {
        const bool plain =
          ((builder.position + 1U) < builder.text.size()) && (builder.text.get(builder.position + 1U) == c_colon);
        if (!plain)
        {
          // Lookahead, lookbehind and named groups are ECMA-262 syntax this engine does not support;
          // anything else after "(?" (for example "(?i)", "(?P<n>" or "(?#") is malformed.
          return pattern_error(builder, is_ecma_group_prefix(builder));
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.119)
        builder.position += 2U;
      }
      builder.quantifiable = false;
      return builder.groups.push_back(group_context{builder.alts.size(), builder.terms.size()});
    }

    /**
     * @brief Handle ')' : close the group and push it as a quantifiable term.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-254
     */
    status end_group(regex_builder& builder) noexcept
    {
      if (builder.groups.size() <= 1U)
      {
        return pattern_error(builder, false);
      }
      std::size_t index = 0U;
      const auto closed = close_group(builder, index);
      builder.quantifiable = true;
      return closed.has_value() ? builder.terms.push_back(index) : closed;
    }

    /**
     * @brief Handle a backslash outside a class.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-255
     */
    status parse_escape_term(regex_builder& builder) noexcept
    {
      const std::size_t first_range = builder.ranges.size();
      const auto escape = read_escape(builder, false);
      if (!escape.has_value())
      {
        return status::failure(*escape.error_if());
      }
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.49)
      ast_node node{};
      if (escape.value_if()->set)
      {
        node.kind = ast_kind::char_class;
        node.first = first_range;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.118)
        node.count = builder.ranges.size() - first_range;
        return push_term(builder, node, true);
      }
      if (!escape.value_if()->code_point.has_value())
      {
        node.kind = ast_kind::assertion;
        node.assertion = escape.value_if()->assertion;
        return push_term(builder, node, false);
      }
      node.kind = ast_kind::literal;
      node.value = *escape.value_if()->code_point;
      return push_term(builder, node, true);
    }

    /**
     * @brief Handle a quantifier character (* + ? {) at position - 1; '{' may be a literal.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-256
     */
    status parse_quantifier(regex_builder& builder, std::uint32_t c) noexcept
    {
      if (c == c_asterisk)
      {
        return apply_quantifier(builder, c_count_zero, c_unbounded);
      }
      if (c == c_plus)
      {
        return apply_quantifier(builder, c_count_one, c_unbounded);
      }
      if (c == c_question)
      {
        return apply_quantifier(builder, c_count_zero, c_count_one);
      }
      const auto braces = read_braces(builder);
      if (braces.has_value())
      {
        return apply_quantifier(builder, (*braces)[0], (*braces)[1]);
      }
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.48)
      ast_node node{};
      node.kind = ast_kind::literal;
      node.value = c_open_brace;
      return push_term(builder, node, true);
    }

    /**
     * @brief Parse one pattern character at position.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-257
     */
    status parse_item(regex_builder& builder) noexcept
    {
      const std::uint32_t c = builder.text.get(builder.position);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.117)
      ++builder.position;
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.47)
      ast_node node{};
      switch (c)
      {
      case c_open_paren:
        return open_group(builder);
      case c_close_paren:
        return end_group(builder);
      case c_vertical_bar:
        builder.quantifiable = false;
        return close_alternative(builder);
      case c_open_bracket:
        return parse_class(builder);
      case c_backslash:
        return parse_escape_term(builder);
      case c_asterisk:
      case c_plus:
      case c_question:
      case c_open_brace:
        return parse_quantifier(builder, c);
      case c_full_stop:
        node.kind = ast_kind::any;
        return push_term(builder, node, true);
      case c_caret:
      case c_dollar:
        node.kind = ast_kind::assertion;
        node.assertion = (c == c_caret) ? regex_op::line_start : regex_op::line_end;
        return push_term(builder, node, false);
      default:
        node.kind = ast_kind::literal;
        node.value = c;
        return push_term(builder, node, true);
      }
    }

    /**
     * @brief Parse the whole pattern into a tree; root receives the root node.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-258
     */
    status parse_pattern(regex_builder& builder, std::size_t& root) noexcept
    {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.46)
      auto parsed = builder.groups.push_back(group_context{});
      while (parsed.has_value() && (builder.position < builder.text.size()))
      {
        parsed = charge(builder);
        parsed = parsed.has_value() ? parse_item(builder) : parsed;
      }
      if (!parsed.has_value())
      {
        return parsed;
      }
      if (builder.groups.size() != 1U)
      {
        return pattern_error(builder, false);
      }
      return close_group(builder, root);
    }

    /**
     * @brief Emission work item.
     * @par Requirements
     * P049; J5-060.
     */
    enum class task_kind : std::uint8_t
    {
      emit,
      concat_step,
      alt_step,
      alt_after,
      alt_end,
      repeat_step,
      star_end,
      optional_end
    };

    /**
     * @brief One pending emission task.
     * @par Requirements
     * P049; J5-060.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.34)
    struct emit_task
    {
        task_kind kind{task_kind::emit};
        std::size_t node{};
        std::size_t index{};
        std::size_t mark{};
        std::size_t split{};
    };

    /**
     * @brief Emission state: tasks and pending jump patches.
     * @par Requirements
     * P049; J5-060.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.33)
    struct emitter
    {
        pod_vector<emit_task> tasks;
        pod_vector<std::size_t> patches;

        /**
         * @brief Bind scratch storage; no allocation.
         * @par Requirements
         * P049.
         * @par ID SWU-SCHEMA-259
         */
        explicit emitter(const storage_callbacks& scratch) noexcept : tasks(scratch), patches(scratch) {}
    };

    /**
     * @brief Append an instruction, charging one compile step.
     * @par Requirements
     * P049; J5-047.
     * @par ID SWU-SCHEMA-260
     */
    status emit(regex_builder& builder, const regex_inst& inst) noexcept
    {
      const auto charged = charge(builder);
      return charged.has_value() ? builder.code.push_back(inst) : charged;
    }

    /**
     * @brief Push two tasks so that first runs before second.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-261
     */
    status push_tasks(emitter& state, const emit_task& first, const emit_task& second) noexcept
    {
      const auto pushed = state.tasks.push_back(second);
      return pushed.has_value() ? state.tasks.push_back(first) : pushed;
    }

    /**
     * @brief Set the not-taken target of a split to the current end of code.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-262
     */
    void patch_exit(const regex_builder& builder, std::size_t split, bool greedy) noexcept
    {
      regex_inst inst = builder.code.get(split);
      if (greedy)
      {
        inst.second = builder.code.size();
      }
      else
      {
        inst.first = builder.code.size();
      }
      builder.code.set(split, inst);
    }

    /**
     * @brief Emit a repeat: mandatory copies, then a star loop or optional copies.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-263
     */
    status run_repeat(regex_builder& builder, emitter& state, const emit_task& task) noexcept
    {
      const ast_node node = builder.nodes.get(task.node);
      const emit_task body{task_kind::emit, node.first, 0U, 0U, 0U};
      if (task.index < node.min)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.116)
        return push_tasks(state, body, emit_task{task_kind::repeat_step, task.node, task.index + 1U, 0U, 0U});
      }
      if ((node.max != c_unbounded) && (task.index >= node.max))
      {
        return status::success();
      }
      const std::size_t split = builder.code.size();
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.115)
      const std::size_t body_start = split + 1U;
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.45)
      regex_inst inst{};
      inst.op = regex_op::split;
      inst.first = node.greedy ? body_start : 0U;
      inst.second = node.greedy ? 0U : body_start;
      const auto emitted = emit(builder, inst);
      if (!emitted.has_value())
      {
        return emitted;
      }
      const task_kind after = (node.max == c_unbounded) ? task_kind::star_end : task_kind::optional_end;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.114)
      return push_tasks(state, body, emit_task{after, task.node, task.index + 1U, 0U, split});
    }

    /**
     * @brief Emit an alternation step: a split before every alternative but the last.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-264
     */
    status run_alternative(regex_builder& builder, emitter& state, const emit_task& task) noexcept
    {
      const ast_node node = builder.nodes.get(task.node);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.113)
      const emit_task body{task_kind::emit, builder.children.get(node.first + task.index), 0U, 0U, 0U};
      if ((task.index + 1U) == node.count)
      {
        return push_tasks(state, body, emit_task{task_kind::alt_end, task.node, 0U, task.mark, 0U});
      }
      const std::size_t split = builder.code.size();
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.44)
      regex_inst inst{};
      inst.op = regex_op::split;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.112)
      inst.first = split + 1U;
      const auto emitted = emit(builder, inst);
      if (!emitted.has_value())
      {
        return emitted;
      }
      return push_tasks(state, body, emit_task{task_kind::alt_after, task.node, task.index, task.mark, split});
    }

    /**
     * @brief Emit a single-instruction node or expand a composite one into tasks.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-265
     */
    status run_emit(regex_builder& builder, emitter& state, const emit_task& task) noexcept
    {
      const ast_node node = builder.nodes.get(task.node);
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.43)
      regex_inst inst{};
      switch (node.kind)
      {
      case ast_kind::literal:
        inst.op = regex_op::literal;
        inst.value = node.value;
        return emit(builder, inst);
      case ast_kind::any:
        inst.op = regex_op::any;
        return emit(builder, inst);
      case ast_kind::char_class:
        inst.op = regex_op::char_class;
        inst.negated = node.negated;
        inst.first = node.first;
        inst.second = node.count;
        return emit(builder, inst);
      case ast_kind::assertion:
        inst.op = node.assertion;
        return emit(builder, inst);
      case ast_kind::concat:
        return state.tasks.push_back(emit_task{task_kind::concat_step, task.node, 0U, 0U, 0U});
      case ast_kind::alternation:
        return state.tasks.push_back(emit_task{task_kind::alt_step, task.node, 0U, state.patches.size(), 0U});
      case ast_kind::repeat:
        return state.tasks.push_back(emit_task{task_kind::repeat_step, task.node, 0U, 0U, 0U});
      case ast_kind::empty:
      default:
        return status::success();
      }
    }

    /**
     * @brief Finish an alternative (jump to the end, patch its split) or the whole alternation.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-266
     */
    status run_alternative_end(regex_builder& builder, emitter& state, const emit_task& task) noexcept
    {
      if (task.kind == task_kind::alt_after)
      {
        const std::size_t jump = builder.code.size();
        // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
        // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its
        // context; the value is zero or the stated constant and no conversion changes it.
        // codeql::misra_deviation_next_line(DEV-103.42)
        regex_inst inst{};
        inst.op = regex_op::jump;
        auto done = emit(builder, inst);
        done = done.has_value() ? state.patches.push_back(jump) : done;
        patch_exit(builder, task.split, true);
        return done.has_value()
                 // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
                 // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding
                 // check, the loop condition, a container size or the Unicode range (code points <= 0x10FFFF,
                 // surrogate halves checked before combining), so it cannot wrap; CodeQL does not follow these
                 // run-time bounds.
                 // codeql::misra_deviation_next_line(DEV-101.111)
                 ? state.tasks.push_back(emit_task{task_kind::alt_step, task.node, task.index + 1U, task.mark, 0U})
                 : done;
      }
      std::size_t index = task.mark;
      while (index < state.patches.size())
      {
        regex_inst inst = builder.code.get(state.patches.get(index));
        inst.first = builder.code.size();
        builder.code.set(state.patches.get(index), inst);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.110)
        ++index;
      }
      state.patches.truncate(task.mark);
      return status::success();
    }

    /**
     * @brief Run one emission task.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-267
     */
    status run_task(regex_builder& builder, emitter& state, const emit_task& task) noexcept
    {
      const ast_node node = builder.nodes.get(task.node);
      switch (task.kind)
      {
      case task_kind::emit:
        return run_emit(builder, state, task);
      case task_kind::concat_step:
        return (task.index < node.count)
                 ? push_tasks(state,
                              // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
                              // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a
                              // preceding check, the loop condition, a container size or the Unicode range (code
                              // points <= 0x10FFFF, surrogate halves checked before combining), so it cannot wrap;
                              // CodeQL does not follow these run-time bounds.
                              // codeql::misra_deviation_next_line(DEV-101.109)
                              emit_task{task_kind::emit, builder.children.get(node.first + task.index), 0U, 0U, 0U},
                              // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
                              // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a
                              // preceding check, the loop condition, a container size or the Unicode range (code
                              // points <= 0x10FFFF, surrogate halves checked before combining), so it cannot wrap;
                              // CodeQL does not follow these run-time bounds.
                              // codeql::misra_deviation_next_line(DEV-101.108)
                              emit_task{task_kind::concat_step, task.node, task.index + 1U, 0U, 0U})
                 : status::success();
      case task_kind::alt_step:
        return run_alternative(builder, state, task);
      case task_kind::alt_after:
      case task_kind::alt_end:
        return run_alternative_end(builder, state, task);
      case task_kind::repeat_step:
        return run_repeat(builder, state, task);
      case task_kind::star_end:
      {
        // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
        // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its
        // context; the value is zero or the stated constant and no conversion changes it.
        // codeql::misra_deviation_next_line(DEV-103.41)
        regex_inst inst{};
        inst.op = regex_op::jump;
        inst.first = task.split;
        const auto emitted = emit(builder, inst);
        patch_exit(builder, task.split, node.greedy);
        return emitted;
      }
      case task_kind::optional_end:
      default:
        patch_exit(builder, task.split, node.greedy);
        return state.tasks.push_back(emit_task{task_kind::repeat_step, task.node, task.index, 0U, 0U});
      }
    }

    /**
     * @brief Emit the program for the tree under root, followed by a match instruction.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-268
     */
    status emit_program(regex_builder& builder, std::size_t root, const storage_callbacks& scratch) noexcept
    {
      emitter state{scratch};
      auto emitted = state.tasks.push_back(emit_task{task_kind::emit, root, 0U, 0U, 0U});
      while (emitted.has_value() && (state.tasks.size() != 0U))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.107)
        const emit_task task = state.tasks.get(state.tasks.size() - 1U);
        state.tasks.pop_back();
        emitted = run_task(builder, state, task);
      }
      if (!emitted.has_value())
      {
        return emitted;
      }
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization (`{}`) or a literal whose type is fixed by its context;
      // the value is zero or the stated constant and no conversion changes it.
      // codeql::misra_deviation_next_line(DEV-103.40)
      regex_inst done{};
      done.op = regex_op::match;
      return emit(builder, done);
    }

    /**
     * @brief Matcher state: thread lists, visit marks and the text as code points.
     * @par Requirements
     * P049; J5-060; J5-065.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.32)
    struct matcher
    {
        const regex_tables& tables;
        node_range program;
        pod_vector<std::uint32_t> text;
        pod_vector<std::size_t> current;
        pod_vector<std::size_t> next;
        pod_vector<std::size_t> marks;
        pod_vector<std::size_t> pending;
        std::size_t generation{};
        bool matched{};

        /**
         * @brief Bind the program and scratch storage; no allocation.
         * @par Requirements
         * P049.
         * @par ID SWU-SCHEMA-269
         */
        matcher(const regex_tables& source, node_range code, const storage_callbacks& scratch) noexcept
            : tables(source), program(code), text(scratch), current(scratch), next(scratch), marks(scratch),
              pending(scratch)
        {
        }
    };

    /**
     * @brief Report whether c is an ECMA-262 word character [0-9A-Za-z_].
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-270
     */
    bool is_word(std::uint32_t c) noexcept
    {
      return ((c >= c_digit_zero) && (c <= c_digit_nine)) || ((c >= c_capital_a) && (c <= c_capital_z)) ||
             ((c >= c_small_a) && (c <= c_small_z)) || (c == c_underscore);
    }

    /**
     * @brief Evaluate a zero-width assertion at position.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-271
     */
    bool assertion_holds(const matcher& state, regex_op op, std::size_t position) noexcept
    {
      if (op == regex_op::line_start)
      {
        return position == 0U;
      }
      if (op == regex_op::line_end)
      {
        return position == state.text.size();
      }
      const bool before = (position != 0U) && is_word(state.text.get(position - 1U));
      const bool after = (position < state.text.size()) && is_word(state.text.get(position));
      return (op == regex_op::word_boundary) ? (before != after) : (before == after);
    }

    /**
     * @brief Add a thread at pc and its epsilon closure at position to list (explicit stack).
     * @par Requirements
     * P049; J5-060; J5-065.
     * @par ID SWU-SCHEMA-272
     */
    status add_thread(matcher& state, pod_vector<std::size_t>& list, std::size_t pc, std::size_t position) noexcept
    {
      state.pending.truncate(0U);
      auto added = state.pending.push_back(pc);
      while (added.has_value() && (state.pending.size() != 0U))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.106)
        const std::size_t at = state.pending.get(state.pending.size() - 1U);
        state.pending.pop_back();
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.105)
        const std::size_t slot = at - state.program.first;
        if (state.marks.get(slot) == state.generation)
        {
          continue;
        }
        state.marks.set(slot, state.generation);
        const regex_inst& inst = state.tables.code[at];
        if (inst.op == regex_op::jump)
        {
          added = state.pending.push_back(inst.first);
        }
        else if (inst.op == regex_op::split)
        {
          added = state.pending.push_back(inst.second);
          added = added.has_value() ? state.pending.push_back(inst.first) : added;
        }
        else if (inst.op == regex_op::match)
        {
          state.matched = true;
        }
        else if ((inst.op == regex_op::literal) || (inst.op == regex_op::any) || (inst.op == regex_op::char_class))
        {
          added = list.push_back(at);
        }
        else if (assertion_holds(state, inst.op, position))
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.104)
          added = state.pending.push_back(at + 1U);
        }
        else
        {
        }
      }
      return added;
    }

    /**
     * @brief Report whether a consuming instruction accepts code point c.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-273
     */
    bool consumes(const regex_tables& tables, const regex_inst& inst, std::uint32_t c) noexcept
    {
      if (inst.op == regex_op::literal)
      {
        return inst.value == c;
      }
      if (inst.op == regex_op::any)
      {
        return (c != 0x0AU) && (c != 0x0DU) && (c != 0x2028U) && (c != 0x2029U);
      }
      bool inside = false;
      std::size_t index = 0U;
      while (!inside && (index < inst.second))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.103)
        const regex_range& range = tables.ranges[inst.first + index];
        inside = (c >= range.low) && (c <= range.high);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.102)
        ++index;
      }
      return inside != inst.negated;
    }

    /**
     * @brief Advance all threads over the code point at position into the next list.
     * @par Requirements
     * P049; J5-060; J5-065.
     * @par ID SWU-SCHEMA-274
     */
    status step_threads(matcher& state, std::size_t position, schema_work& work) noexcept
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.101)
      const auto charged = work.regex_steps(state.current.size() + 1U);
      if (!charged.has_value())
      {
        return charged;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.100)
      ++state.generation;
      state.next.truncate(0U);
      const std::uint32_t c = state.text.get(position);
      status stepped = status::success();
      std::size_t index = 0U;
      while (stepped.has_value() && (index < state.current.size()))
      {
        const std::size_t pc = state.current.get(index);
        if (consumes(state.tables, state.tables.code[pc], c))
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.99)
          stepped = add_thread(state, state.next, pc + 1U, position + 1U);
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.98)
        ++index;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.97)
      stepped = stepped.has_value() ? add_thread(state, state.next, state.program.first, position + 1U) : stepped;
      state.current.truncate(0U);
      std::size_t moved = 0U;
      while (stepped.has_value() && (moved < state.next.size()))
      {
        stepped = state.current.push_back(state.next.get(moved));
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.96)
        ++moved;
      }
      return stepped;
    }

    /**
     * @brief Prepare the matcher: decode the text and size the visit marks.
     * @par Requirements
     * P049; J5-060.
     * @par ID SWU-SCHEMA-275
     */
    status prepare(matcher& state, const decoded_string& source) noexcept
    {
      auto prepared = decode_text(source, state.text);
      std::size_t index = 0U;
      while (prepared.has_value() && (index < state.program.count))
      {
        prepared = state.marks.push_back(0U);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.95)
        ++index;
      }
      state.generation = 1U;
      return prepared.has_value() ? add_thread(state, state.current, state.program.first, c_count_zero) : prepared;
    }
  } // namespace

  /**
   * @brief Compile an ECMA-262 pattern (the Draft-07 regex dialect) into a Pike VM program.
   * @par Requirements
   * P049; J5-054; J5-060; ADR 0006 (owner decision 2026-11-22: own linear-time engine).
   * @par ID SWU-SCHEMA-232
   */
  result<node_range, schema_error> compile_regex(const decoded_string& pattern, pod_vector<regex_inst>& code,
                                                 pod_vector<regex_range>& ranges, const storage_callbacks& scratch,
                                                 std::size_t& steps, std::size_t step_limit,
                                                 std::optional<source_span> span) noexcept
  {
    using range_result = result<node_range, schema_error>;
    regex_builder builder{code, ranges, scratch, steps, step_limit, span};
    const std::size_t start = code.size();
    std::size_t root = 0U;
    auto built = decode_text(pattern, builder.text);
    built = built.has_value() ? parse_pattern(builder, root) : built;
    built = built.has_value() ? emit_program(builder, root, scratch) : built;
    if (!built.has_value())
    {
      return range_result::failure(*built.error_if());
    }
    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
    // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
    // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
    // codeql::misra_deviation_next_line(DEV-101.94)
    return range_result::success(node_range{start, code.size() - start});
  }

  /**
   * @brief Report whether a compiled pattern matches anywhere in text (unanchored search).
   * @par Requirements
   * P049; J5-057; J5-060; J5-065.
   * @par ID SWU-SCHEMA-233
   */
  result<bool, schema_error> regex_search(const regex_tables& tables, node_range program, const decoded_string& text,
                                          schema_work& work, const storage_callbacks& scratch) noexcept
  {
    using bool_result = result<bool, schema_error>;
    matcher state{tables, program, scratch};
    auto searched = prepare(state, text);
    std::size_t position = 0U;
    while (searched.has_value() && !state.matched && (position < state.text.size()))
    {
      searched = step_threads(state, position, work);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.93)
      ++position;
    }
    if (!searched.has_value())
    {
      return bool_result::failure(*searched.error_if());
    }
    return bool_result::success(state.matched);
  }
} // namespace adaptive_json5::detail
