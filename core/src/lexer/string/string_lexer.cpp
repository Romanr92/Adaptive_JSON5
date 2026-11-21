#include "lexer/string/string_lexer.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/security_reason.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "common/checked_size.hpp"
#include "input/cursor.hpp"
#include "lexer/string/string_boundary.hpp"
#include "lexer/string/string_checks.hpp"
#include "lexer/string/string_escape.hpp"
#include "lexer/string/string_token.hpp"
#include "lexer/string/string_warning.hpp"
#include "memory/decoded_string_builder.hpp"
#include "unicode/utf16.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    // Allowance for callers that do not limit warnings: a string cannot produce this many,
    // so excess_warning stays empty.
    constexpr std::size_t c_no_warning_allowance = std::numeric_limits<std::size_t>::max();

    // Secure-mode limits, compared as std::uint32_t like string_boundary.cpp.
    constexpr std::uint32_t c_zero_unit = 0x00U;   // '\0'
    constexpr std::uint32_t c_control_max = 0x1FU; // '\x1F'

    /**
     * @brief Totals produced by one walk over a string body.
     * @par Requirements
     * P021; J5-017; J5-029; J5-046.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Audit result only: the class declares no special members of the listed kind, so the
    // compiler provides them implicitly and consistently (rule of zero). CodeQL cannot analyse them because
    // implicit members that are never odr-used are not in its database.
    // codeql::misra_deviation_next_line(DEV-020.33)
    struct string_walk
    {
        std::size_t units{};                            // Decoded UTF-16 units.
        std::size_t warning_count{};                    // All warnings in the body.
        std::optional<string_warning> first_warning{};  // First warning, if any.
        std::optional<string_warning> excess_warning{}; // First warning past warning_allowance, if any.
    };

    struct string_body_state
    {
        std::size_t max_units;
        std::size_t warning_allowance;
        decoded_string_builder* output;
        const string_warning_sink* sink;
        string_checks checks;
        // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
        // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
        // implicit zero of the initialization as an unsuffixed octal literal.
        // codeql::misra_deviation_next_line(DEV-010.14)
        string_walk walk{};
        detail::surrogate_pairing pairing{};
        std::optional<error> violation{};

        /**
         * @brief Count a warning, retain the first and first excess warning, and notify the supplied sink.
         * @par Requirements
         * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
         * @par ID SWU-LEXER-066
         */
        void record(const string_warning& warning) noexcept
        {
          if (!walk.first_warning.has_value())
          {
            walk.first_warning = warning;
          }

          // warning_count is still this warning's zero-based index, so it equals the allowance exactly once.
          if (walk.warning_count == warning_allowance)
          {
            walk.excess_warning = warning;
          }

          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Counts warnings of one string, at most one per character of the input, so it
          // cannot wrap std::size_t.
          // codeql::misra_deviation_next_line(DEV-004.76)
          ++walk.warning_count;

          // Recording during this walk replaces a separate re-scan of the string (ADR 0003, decision 10, D).
          if (sink != nullptr)
          {
            sink->record(sink->context, warning);
          }
        }

        /**
         * @brief Retain the first security violation for reporting when the string closes.
         * @par Requirements
         * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
         * @par ID SWU-LEXER-067
         */
        void reject(security_reason reason, source_span span) noexcept
        {
          if (!violation.has_value())
          {
            violation = error::security_at_range(span, reason);
          }
        }

        /**
         * @brief Report unpaired surrogates as warnings or security violations according to the string policy.
         * @par Requirements
         * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
         * @par ID SWU-LEXER-068
         */
        void handle_pairing(const detail::surrogate_warnings& found) noexcept
        {
          // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
          // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
          // bound that the body does not modify, and changed only in the increment expression. The counter is
          // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
          // codeql::misra_deviation_next_line(DEV-017.19)
          for (std::size_t index{}; index < found.size; ++index)
          {
            const string_warning& warning = found.items[index];
            if (checks.reject_unpaired_surrogate)
            {
              reject(security_reason::unpaired_surrogate, source_span{warning.byte_offset, warning.byte_length});
            }
            else
            {
              record(warning);
            }
          }
        }

        /**
         * @brief Check the first decoded unit against the secure string and member-name policies.
         * @par Requirements
         * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
         * @par ID SWU-LEXER-069
         */
        void check_units(const encoded_utf16& units, source_span span) noexcept
        {
          if (units.size == std::size_t{})
          {
            return;
          }

          // Compare as std::uint32_t, like string_boundary, so no character is used as a number.
          const auto unit = static_cast<std::uint32_t>(units.units[encoded_utf16::c_first_unit_index]);
          if (checks.reject_zero_bytes && (unit == c_zero_unit))
          {
            reject(security_reason::zero_byte, span);
          }
          else if (checks.reject_control_characters && (unit <= c_control_max))
          {
            reject(security_reason::name_control_character, span);
          }
          else
          {
            // Any other unit is allowed.
          }
        }

        /**
         * @brief Account for decoded units and append them when a destination builder is present.
         * @par Requirements
         * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
         * @par ID SWU-LEXER-070
         */
        status_result emit(const encoded_utf16& units, std::size_t offset) noexcept
        {
          const auto total = checked_add(walk.units, units.size, resource_limit_reason::string);
          if ((!total.has_value()) || (*total.value_if() > max_units))
          {
            return status_result::failure(error::limit_at(offset, resource_limit_reason::string));
          }

          if (output != nullptr)
          {
            // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
            // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with
            // a bound that the body does not modify, and changed only in the increment expression. The counter is
            // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
            // codeql::misra_deviation_next_line(DEV-017.18)
            for (std::size_t index{}; index < units.size; ++index)
            {
              const auto appended = output->append_unit(units.units[index]);
              if (!appended.has_value())
              {
                // Keep the builder's code and reason, located at the character being appended.
                return status_result::failure(appended.error_if()->relocated(offset));
              }
            }
          }

          walk.units = *total.value_if();
          return status_result::success();
        }

        /**
         * @brief Decode an escape, emit its units and apply surrogate and security checks to its source span.
         * @par Requirements
         * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
         * @par ID SWU-LEXER-071
         */
        status_result read_escape(cursor& source, syntax_mode mode, std::size_t piece_offset) noexcept
        {
          const auto escaped = consume_string_escape(source, mode);
          if (!escaped.has_value())
          {
            return status_result::failure(*escaped.error_if());
          }

          const auto emitted = emit(*escaped.value_if(), piece_offset);
          if (!emitted.has_value())
          {
            return status_result::failure(*emitted.error_if());
          }

          // The cursor only moves forward, so the escape's end is never before its backslash.
          const std::size_t escape_end = source.offset();
          if (escape_end < piece_offset)
          {
            return status_result::failure(error::internal_fault());
          }
          const std::size_t escape_length = escape_end - piece_offset;

          handle_pairing(pairing.escape(*escaped.value_if(), piece_offset, escape_length));
          check_units(*escaped.value_if(), source_span{piece_offset, escape_length});
          return status_result::success();
        }

        /**
         * @brief Emit a raw character and report surrogate, security and separator findings in source order.
         * @par Requirements
         * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
         * @par ID SWU-LEXER-072
         */
        status_result read_character(const string_piece& piece, syntax_mode mode) noexcept
        {
          const auto piece_offset = piece.byte_offset;
          // An invalid scalar keeps the encoder's code and reason, relocated to its character.
          const auto encoded = encode_utf16(piece.scalar);
          if (!encoded.has_value())
          {
            return status_result::failure(encoded.error_if()->relocated(piece_offset));
          }

          const auto emitted = emit(*encoded.value_if(), piece_offset);
          if (!emitted.has_value())
          {
            return status_result::failure(*emitted.error_if());
          }

          handle_pairing(pairing.character());
          check_units(*encoded.value_if(), source_span{piece_offset, piece.byte_length});

          const auto warning = raw_separator_warning_for(piece, mode);
          if (warning.has_value())
          {
            record(*warning);
          }
          return status_result::success();
        }
    };

    /**
     * @brief Walk a string body from after its opening quote through its closing quote.
     * @par Requirements
     * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-029; J5-046.
     * @param source Cursor after the opening quote; after success, after the closing quote.
     * @param start Original offset of the opening quote, for unterminated-string errors.
     * @param quote Quote character that closes the string.
     * @param mode Selected input syntax.
     * @param max_units Maximum decoded UTF-16 units.
     * @param warning_allowance Warnings that still fit; the first one past it is kept as
     * excess_warning. Callers that do not limit warnings pass c_no_warning_allowance.
     * @param output Null to count only; otherwise a builder reserved for this string.
     * @param sink Optional parser sink that records each warning; used only while decoding.
     * @param checks Secure-mode checks (CSR-07).
     * @return Unit and warning totals, or a located error.
     * @note The first secure-mode violation is kept and reported at the closing quote, so a
     * syntax, encoding or limit error in the same string takes precedence.
     * @note With a null output, performs no allocation (ADR 0003, decision 2).
     * resource_limit (string) is reported at the piece that first exceeds max_units; the
     * engine names total_string instead when that limit was the smaller one.
     * @par ID SWU-LEXER-038
     */
    result<string_walk> walk_string_body(cursor& source, std::size_t start, string_quote quote, syntax_mode mode,
                                         std::size_t max_units, std::size_t warning_allowance,
                                         decoded_string_builder* output, const string_warning_sink* sink,
                                         string_checks checks) noexcept
    {
      using walk_result = result<string_walk>;

      string_body_state state{max_units, warning_allowance, output, sink, checks};

      while (true)
      {
        const auto piece = consume_string_piece(source, quote, mode);
        if (!piece.has_value())
        {
          // Running out of input means the string is unterminated; cover it from its opening quote.
          if (source.is_at_end())
          {
            // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
            // MISRA justification: A cursor only moves forward, so the later offset is never below the earlier
            // offset it started from, and the difference is the number of bytes consumed.
            // codeql::misra_deviation_next_line(DEV-004.75)
            const source_span open_string{start, source.offset() - start};

            return walk_result::failure(
              error::syntax_at_range(source.offset(), open_string, syntax_error_reason::unterminated_string));
          }

          return walk_result::failure(*piece.error_if());
        }

        const std::size_t piece_offset = piece.value_if()->byte_offset;

        if (piece.value_if()->kind == string_piece_kind::closing_quote)
        {
          state.handle_pairing(state.pairing.finish());
          if (state.violation.has_value())
          {
            return walk_result::failure(*state.violation);
          }
          return walk_result::success(state.walk);
        }

        const auto consumed = piece.value_if()->kind == string_piece_kind::escape_start
                                ? state.read_escape(source, mode, piece_offset)
                                : state.read_character(*piece.value_if(), mode);
        if (!consumed.has_value())
        {
          return walk_result::failure(*consumed.error_if());
        }
      }
    }
  } // namespace

  namespace detail
  {
    /**
     * @brief Deliver warnings from an already validated string body.
     * @par Requirements
     * P021; J5-017.
     * @param source Copy of the cursor immediately after the opening quote.
     * @note Requires the same unchanged input validated by the first pass.
     * The observer must satisfy string_warning_observer's lifetime contract.
     * Performs no allocation and does not modify the caller's cursor.
     * @par ID SWU-LEXER-039
     */
    status_result notify_string_warnings(cursor source, string_quote quote, syntax_mode mode,
                                         const string_warning_observer& observer) noexcept
    {
      surrogate_pairing pairing{};

      // Deliver warnings in the same order the first pass counted them.
      const auto deliver = [&observer](const surrogate_warnings& found) noexcept
      {
        // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
        // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
        // bound that the body does not modify, and changed only in the increment expression. The counter is
        // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
        // codeql::misra_deviation_next_line(DEV-017.17)
        for (std::size_t index{}; index < found.size; ++index)
        {
          if (observer.notify != nullptr)
          {
            observer.notify(observer.context, found.items[index]);
          }
        }
      };

      while (true)
      {
        const auto piece = consume_string_piece(source, quote, mode);
        if (!piece.has_value())
        {
          return status_result::failure(*piece.error_if());
        }

        if (piece.value_if()->kind == string_piece_kind::closing_quote)
        {
          deliver(pairing.finish());
          return status_result::success();
        }

        if (piece.value_if()->kind == string_piece_kind::escape_start)
        {
          // Follow the same escape boundaries without rebuilding decoded storage.
          const auto escaped = consume_string_escape(source, mode);
          if (!escaped.has_value())
          {
            return status_result::failure(*escaped.error_if());
          }

          const std::size_t escape_offset = piece.value_if()->byte_offset;
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: A cursor only moves forward, so the later offset is never below the earlier
          // offset it started from, and the difference is the number of bytes consumed.
          // codeql::misra_deviation_next_line(DEV-004.74)
          deliver(pairing.escape(*escaped.value_if(), escape_offset, source.offset() - escape_offset));
        }
        else
        {
          deliver(pairing.character());

          const auto warning = raw_separator_warning_for(*piece.value_if(), mode);

          if (warning.has_value() && (observer.notify != nullptr))
          {
            observer.notify(observer.context, *warning);
          }
        }
      }
    }
  } // namespace detail

  /**
   * @brief Validate one complete quoted string and count its units without allocating.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-041; J5-046; J5-047.
   * @param source Cursor positioned at a possible opening quote.
   * @param mode Selected input syntax.
   * @param max_units Maximum decoded UTF-16 units.
   * @param warning_allowance Warnings that still fit the caller's diagnostic budget; the default never limits.
   * @param checks Secure-mode checks; the default applies none (CSR-07).
   * @return The measurement, nullopt for nonquote input or EOF, or a located failure.
   * @note Performs no allocation (ADR 0003, decisions 2 and 7).
   * Too many warnings are not a failure here: the caller reports excess_warning.
   * @par ID SWU-LEXER-040
   */
  result<std::optional<string_measure>> measure_string(cursor& source, syntax_mode mode, std::size_t max_units,
                                                       std::size_t warning_allowance, string_checks checks) noexcept
  {
    using measure_result = result<std::optional<string_measure>>;

    const std::size_t start = source.offset();

    // Keep all token consumption private until the complete operation succeeds.
    cursor candidate = source;
    const auto opening = consume_string_opening(candidate, mode);

    if (!opening.has_value())
    {
      return measure_result::failure(*opening.error_if());
    }

    if (!opening.value_if()->has_value())
    {
      return measure_result::success(std::nullopt);
    }

    // A null builder makes the walk validate and count only.
    const auto counted = walk_string_body(candidate, start, **opening.value_if(), mode, max_units, warning_allowance,
                                          nullptr, nullptr, checks);

    if (!counted.has_value())
    {
      return measure_result::failure(*counted.error_if());
    }

    const string_walk& totals = *counted.value_if();
    const string_measure measured{
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: A cursor only moves forward, so the later offset is never below the earlier offset
      // it started from, and the difference is the number of bytes consumed.
      // codeql::misra_deviation_next_line(DEV-004.73)
      totals.units,         start, candidate.offset() - start, totals.warning_count, totals.first_warning,
      totals.excess_warning};

    source = candidate;
    return measure_result::success(measured);
  }

  /**
   * @brief Consume one complete quoted string into owned decoded storage.
   * @par Requirements
   * P021; J5-014; J5-015; J5-016; J5-017; J5-018; J5-041; J5-046.
   * @param source Cursor positioned at a possible opening quote.
   * @param mode Selected input syntax.
   * @param storage Caller-owned fallible storage provider.
   * @param max_units Maximum decoded UTF-16 unit capacity.
   * @param observer Optional observer for all successful-string warnings.
   * @param sink Optional parser sink: reserve after the string is allocated, record during decoding.
   * @param checks Secure-mode checks; the default applies none (CSR-07). Applied in the
   * measuring pass, so a violation is reported before any allocation.
   * @return Owned token, successful nullopt before nonquote input or EOF,
   * or an explicit located failure.
   * @note Success consumes through the matching closing quote only.
   * No-match and failure leave the caller's cursor unchanged.
   * Measures with measure_string, then makes one allocation of exactly that size (ADR 0003, decision 2).
   * Partial owned storage is released on failure.
   * No warning callbacks occur for a string that fails validation or storage.
   * Input must remain unchanged throughout both passes and callbacks.
   * Observer callbacks must satisfy string_warning_observer's contract.
   * Does not skip trivia or validate input following the closing quote.
   * max_units is a component limit, not complete parser memory accounting.
   * @par ID SWU-LEXER-041
   */
  result<std::optional<string_token>> consume_string(cursor& source, syntax_mode mode, const storage_callbacks& storage,
                                                     std::size_t max_units, string_warning_observer observer,
                                                     const string_warning_sink* sink, string_checks checks) noexcept
  {
    using string_result = result<std::optional<string_token>>;

    // First pass: the same measurement the parser's pre-pass uses, so both agree by construction.
    cursor counting = source;
    const auto measured = measure_string(counting, mode, max_units, c_no_warning_allowance, checks);

    if (!measured.has_value())
    {
      return string_result::failure(*measured.error_if());
    }

    if (!measured.value_if()->has_value())
    {
      return string_result::success(std::nullopt);
    }

    const string_measure& totals = **measured.value_if();
    const std::size_t start = totals.byte_offset;

    // Both quote characters are one byte, so the body starts right after the opening quote.
    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: Tests whether the byte equals one specific character. An equality comparison between
    // two character operands does not use the numeric value; the tool reports the implicit promotion of both
    // operands to int. The parser compares UTF-8 input bytes with character literals, so it relies on an ASCII-
    // compatible execution character set, where these characters are contiguous ranges.
    // codeql::misra_deviation_next_line(DEV-015.30)
    const string_quote quote = (*source.peek() == '\'') ? string_quote::single : string_quote::double_quote;
    cursor body = source;
    body.advance(1);

    // One allocation of exactly the counted size; an empty string makes none.
    decoded_string_builder builder(storage, max_units);
    const auto reserved = builder.reserve_exact(totals.units);
    if (!reserved.has_value())
    {
      // Keep the builder's code and reason, e.g. a string limit, located at the token start.
      return string_result::failure(reserved.error_if()->relocated(start));
    }

    // The parser reserves units, diagnostics and warning slots after the string's own allocation, as before.
    if (sink != nullptr)
    {
      const auto sink_reserved = sink->reserve(sink->context, totals.units, totals.warning_count);

      if (!sink_reserved.has_value())
      {
        return string_result::failure(*sink_reserved.error_if());
      }
    }

    // Second pass: write the units into the reservation.
    cursor decoding = body;
    const auto decoded =
      walk_string_body(decoding, start, quote, mode, max_units, c_no_warning_allowance, &builder, sink, checks);
    if (!decoded.has_value())
    {
      return string_result::failure(*decoded.error_if());
    }

    // Both passes read the same bytes with the same code, so any difference is an internal error.
    if ((decoding.offset() != counting.offset()) || (decoded.value_if()->units != totals.units))
    {
      return string_result::failure(error::internal_fault());
    }

    string_token token{builder.finish(), start, totals.byte_length, totals.warning_count, totals.first_warning};

    // Construct the complete result before publishing any warning callbacks.
    auto completed = string_result::success(std::optional<string_token>{std::move(token)});

    if ((observer.notify != nullptr) && (totals.warning_count != std::size_t{}))
    {
      const auto notified = detail::notify_string_warnings(body, quote, mode, observer);
      if (!notified.has_value())
      {
        return string_result::failure(*notified.error_if());
      }
    }

    source = counting;
    return completed;
  }
} // namespace adaptive_json5
