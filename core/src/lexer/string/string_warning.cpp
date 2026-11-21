#include "lexer/string/string_warning.hpp"

#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "lexer/string/string_boundary.hpp"
#include "unicode/utf16.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  namespace
  {
    constexpr std::uint32_t c_high_surrogate_min = 0xD800;
    constexpr std::uint32_t c_high_surrogate_max = 0xDBFF;
    constexpr std::uint32_t c_low_surrogate_min = 0xDC00;
    constexpr std::uint32_t c_low_surrogate_max = 0xDFFF;

    /**
     * @brief Append one warning to a pairing step's result.
     * @par Requirements
     * P041; J5-017; J5-018.
     * @note Callers never exceed capacity: one pending warning plus two units per escape.
     * @par ID SWU-LEXER-042
     */
    void add(detail::surrogate_warnings& found, const string_warning& warning) noexcept
    {
      if (found.size < detail::surrogate_warnings::c_capacity)
      {
        found.items[found.size] = warning;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: The check above keeps found.size below c_capacity, so the increment reaches at
        // most c_capacity.
        // codeql::misra_deviation_next_line(DEV-004.20)
        ++found.size;
      }
    }
  } // namespace

  namespace detail
  {
    /**
     * @brief Record the units decoded from one escape sequence.
     * @par Requirements
     * P041; J5-017; J5-018.
     * @note A pending high surrogate pairs with the next decoded low unit.
     * An escape producing no units (a line continuation) leaves pairing pending.
     * @par ID SWU-LEXER-043
     */
    surrogate_warnings surrogate_pairing::escape(const encoded_utf16& units, std::size_t byte_offset,
                                                 std::size_t byte_length) noexcept
    {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.02)
      surrogate_warnings found{};
      const string_warning current{string_warning_code::unpaired_surrogate_escape, byte_offset, byte_length};

      // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
      // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
      // bound that the body does not modify, and changed only in the increment expression. The counter is
      // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
      // codeql::misra_deviation_next_line(DEV-017.03)
      for (std::size_t index{}; index < units.size; ++index)
      {
        const auto unit = static_cast<std::uint32_t>(units.units[index]);
        const bool high = (unit >= c_high_surrogate_min) && (unit <= c_high_surrogate_max);
        const bool low = (unit >= c_low_surrogate_min) && (unit <= c_low_surrogate_max);

        if (m_pending_high.has_value())
        {
          if (low)
          {
            // The pending high surrogate and this unit form a valid pair.
            m_pending_high.reset();
            continue;
          }

          add(found, *m_pending_high);
          m_pending_high.reset();
        }

        if (high)
        {
          m_pending_high = current;
        }
        else if (low)
        {
          add(found, current);
        }
        else
        {
          // Neither surrogate half: there is nothing to pair or report.
        }
      }

      return found;
    }

    /**
     * @brief Record a raw source character, which ends any pending high surrogate.
     * @par Requirements
     * P041; J5-017; J5-018.
     * @par ID SWU-LEXER-044
     */
    surrogate_warnings surrogate_pairing::character() noexcept
    {
      return finish();
    }

    /**
     * @brief Record the closing quote, which ends any pending high surrogate.
     * @par Requirements
     * P041; J5-017; J5-018.
     * @par ID SWU-LEXER-045
     */
    surrogate_warnings surrogate_pairing::finish() noexcept
    {
      // MISRA rule: RULE-5-13-4 cpp/misra/unsigned-integer-literals-not-appropriately-suffixed
      // MISRA justification: Empty-brace value-initialization contains no integer literal. The tool reports the
      // implicit zero of the initialization as an unsuffixed octal literal.
      // codeql::misra_deviation_next_line(DEV-010.01)
      surrogate_warnings found{};

      if (m_pending_high.has_value())
      {
        add(found, *m_pending_high);
        m_pending_high.reset();
      }

      return found;
    }
  } // namespace detail

  /**
   * @brief Describe a warning for a raw JSON5 string separator, if present.
   * @par Requirements
   * P020; J5-017.
   * @param piece An original source piece returned by consume_string_piece.
   * @return Warning for a raw JSON5 U+2028/U+2029 character, otherwise nullopt.
   * @note Does not change content, cursor state or parsing success.
   * Call only for source pieces, never reconstructed decoded escape values.
   * Strict JSON receives no JSON5 separator advisory.
   * No allocation or callback is performed.
   * @par ID SWU-LEXER-046
   */
  std::optional<string_warning> raw_separator_warning_for(const string_piece& piece, syntax_mode mode) noexcept
  {
    constexpr std::uint32_t c_line_separator = 0x2028;
    constexpr std::uint32_t c_paragraph_separator = 0x2029;

    if ((mode != syntax_mode::json5) || (piece.kind != string_piece_kind::character))
    {
      return std::nullopt;
    }

    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value;
    // every path assigns the real value before use. For constants and results, the value is read later in the
    // same function or template instantiation, which the tool does not track.
    // codeql::misra_deviation_next_line(DEV-007.18)
    string_warning_code code{};

    if (piece.scalar == c_line_separator)
    {
      code = string_warning_code::raw_line_separator;
    }
    else if (piece.scalar == c_paragraph_separator)
    {
      code = string_warning_code::raw_paragraph_separator;
    }
    else
    {
      return std::nullopt;
    }

    return string_warning{code, piece.byte_offset, piece.byte_length};
  }
} // namespace adaptive_json5
