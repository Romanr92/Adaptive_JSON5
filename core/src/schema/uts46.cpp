#include "schema/uts46.hpp"

#include "schema/idna.hpp"
#include "schema/normalization.hpp"
#include "schema/pod_vector.hpp"
#include "schema/unicode_properties.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  namespace
  {
    constexpr std::uint32_t c_full_stop = 0x2EU;
    constexpr std::uint32_t c_first_printable = 0x20U; // ' ', the first character of ascii_char's table.
    constexpr std::uint32_t c_ascii_end = 0x80U;

    /**
     * @brief Conversion state: the ASCII form so far and the mapped code points of the current
     * label.
     * @par Requirements
     * J5-069; UTS #46 section 4.
     */
    struct host_conversion
    {
        ascii_host text;
        std::size_t size;
        decomposed_label label;
        std::size_t count;
    };

    /**
     * @brief UTS #46 entry of a code point, or nullptr when it is valid.
     * @par Requirements
     * J5-069; UTS #46 section 5.
     * @par ID SWU-SCHEMA-429
     */
    const uts46_entry* uts46_entry_of(std::uint32_t code_point) noexcept
    {
      std::size_t low = 0U;
      std::size_t high = c_uts46_entries.size();
      while (low < high)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.233)
        const std::size_t middle = low + ((high - low) / 2U);
        const uts46_entry& entry = c_uts46_entries[middle];
        if (code_point < entry.first)
        {
          high = middle;
        }
        else if (code_point > entry.last)
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.234)
          low = middle + 1U;
        }
        else
        {
          return &entry;
        }
      }
      return nullptr;
    }

    /**
     * @brief Append a character to the ASCII form; false when it is full.
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-430
     */
    bool put_char(host_conversion& state, char c) noexcept
    {
      if (state.size >= state.text.size())
      {
        return false;
      }
      state.text[state.size] = c;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
      // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
      // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.235)
      ++state.size;
      return true;
    }

    /**
     * @brief The printable ASCII character of a code point, or nullopt (control characters and
     * non-ASCII), without using character values numerically.
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-431
     */
    std::optional<char> ascii_char(std::uint32_t code_point) noexcept
    {
      constexpr std::string_view c_printable{
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"};
      if ((code_point < c_first_printable) || ((code_point - c_first_printable) >= c_printable.size()))
      {
        return std::nullopt;
      }
      return c_printable[static_cast<std::size_t>(code_point - c_first_printable)];
    }

    /**
     * @brief Report whether the first count code points of label are ASCII.
     * @par Requirements
     * J5-069; RFC 5890 section 2.3.1.
     * @par ID SWU-SCHEMA-432
     */
    bool is_ascii_label(const decomposed_label& label, std::size_t count) noexcept
    {
      std::size_t index = 0U;
      while (index < count)
      {
        if (label[index] >= c_ascii_end)
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.236)
        ++index;
      }
      return true;
    }

    /**
     * @brief Append an ASCII label as it is.
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-433
     */
    bool put_ascii_label(host_conversion& state, std::size_t count) noexcept
    {
      std::size_t index = 0U;
      while (index < count)
      {
        const auto c = ascii_char(state.label[index]);
        if (!c.has_value())
        {
          return false;
        }
        if (!put_char(state, *c))
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.237)
        ++index;
      }
      return true;
    }

    /**
     * @brief Append the A-label of a label with a non-ASCII code point.
     * @par Requirements
     * J5-069; RFC 5891 section 4.4.
     * @par ID SWU-SCHEMA-434
     */
    bool put_a_label(host_conversion& state, std::size_t count) noexcept
    {
      if (count > c_max_label_code_points)
      {
        return false;
      }
      std::array<std::uint32_t, c_max_label_code_points> label{};
      std::size_t index = 0U;
      while (index < count)
      {
        label[index] = state.label[index];
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.238)
        ++index;
      }
      a_label_text encoded{};
      const auto length = to_a_label(label, count, encoded);
      bool ok = length.has_value();
      index = 0U;
      while (ok && (index < *length))
      {
        ok = put_char(state, encoded[index]);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.239)
        ++index;
      }
      return ok;
    }

    /**
     * @brief Normalize the current label to NFC and append its ASCII form; an empty label
     * appends nothing (the host name check rejects it).
     * @par Requirements
     * J5-069; UTS #46 section 4.
     * @par ID SWU-SCHEMA-435
     */
    bool flush_label(host_conversion& state) noexcept
    {
      const auto count = normalize_nfc(state.label, state.count);
      state.count = 0U;
      if (!count.has_value())
      {
        return false;
      }
      return is_ascii_label(state.label, *count) ? put_ascii_label(state, *count) : put_a_label(state, *count);
    }

    /**
     * @brief Add a mapped code point: '.' ends the label, every other one is appended to it.
     * @par Requirements
     * J5-069; UTS #46 section 4.
     * @par ID SWU-SCHEMA-436
     */
    bool push_mapped(host_conversion& state, std::uint32_t code_point) noexcept
    {
      if (code_point == c_full_stop)
      {
        if (!flush_label(state))
        {
          return false;
        }
        return put_char(state, '.');
      }
      if (state.count >= state.label.size())
      {
        return false;
      }
      state.label[state.count] = code_point;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
      // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
      // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.240)
      ++state.count;
      return true;
    }

    /**
     * @brief Map an input code point: valid ones stay, ignored ones are removed, mapped ones are
     * replaced and disallowed ones fail.
     * @par Requirements
     * J5-069; UTS #46 sections 4 and 5.
     * @par ID SWU-SCHEMA-437
     */
    bool push_input(host_conversion& state, std::uint32_t code_point) noexcept
    {
      const uts46_entry* entry = uts46_entry_of(code_point);
      if (entry == nullptr)
      {
        return push_mapped(state, code_point);
      }
      const auto status = static_cast<std::uint32_t>(entry->status);
      if (status == static_cast<std::uint32_t>(uts46_status::ignored))
      {
        return true;
      }
      if (status != static_cast<std::uint32_t>(uts46_status::mapped))
      {
        return false;
      }
      bool ok = true;
      std::size_t index = 0U;
      while (ok && (index < entry->length))
      {
        ok = push_mapped(state, c_uts46_mappings[entry->offset + index]);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.241)
        ++index;
      }
      return ok;
    }
  } // namespace

  /**
   * @brief UTS #46 ToASCII of a host name.
   * @par Requirements
   * J5-069; UTS #46 sections 4 and 4.2; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-428
   */
  std::optional<std::size_t> idn_to_ascii(const pod_vector<std::uint32_t>& text, std::size_t first, std::size_t end,
                                          ascii_host& out) noexcept
  {
    host_conversion state{};
    bool ok = true;
    std::size_t index = first;
    while (ok && (index < end))
    {
      ok = push_input(state, text.get(index));
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
      // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
      // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.242)
      ++index;
    }
    if (!ok)
    {
      return std::nullopt;
    }
    if (!flush_label(state))
    {
      return std::nullopt;
    }
    out = state.text;
    return state.size;
  }
} // namespace adaptive_json5::detail
