#include "schema/normalization.hpp"

#include "schema/idna.hpp"
#include "schema/unicode_properties.hpp"
#include "schema/unicode_property.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5::detail
{
  namespace
  {
    // Unicode section 3.12 conjoining Jamo behavior.
    constexpr std::uint32_t c_hangul_base = 0xAC00U;
    constexpr std::uint32_t c_hangul_last = 0xD7A3U;
    constexpr std::uint32_t c_leading_base = 0x1100U;
    constexpr std::uint32_t c_vowel_base = 0x1161U;
    constexpr std::uint32_t c_trailing_base = 0x11A7U;
    constexpr std::uint32_t c_leading_count = 19U;
    constexpr std::uint32_t c_vowel_count = 21U;
    constexpr std::uint32_t c_trailing_count = 28U;
    constexpr std::uint32_t c_block_count = c_vowel_count * c_trailing_count; // 588
    constexpr std::uint32_t c_blocked = 256U;                                 // Larger than every combining class.
    constexpr std::size_t c_expansion = 4U; // Longest canonical decomposition of one code point.
    constexpr std::size_t c_pending_limit = 8U;

    using normal_buffer = std::array<std::uint32_t, c_max_label_code_points * c_expansion>;

    /**
     * @brief Append a code point to buffer; false when full.
     * @par Requirements
     * J5-069; Unicode UAX #15.
     * @par ID SWU-SCHEMA-404
     */
    bool append(normal_buffer& buffer, std::size_t& size, std::uint32_t code_point) noexcept
    {
      if (size >= buffer.size())
      {
        return false;
      }
      buffer[size] = code_point;
      ++size;
      return true;
    }

    /**
     * @brief Append the algorithmic decomposition of a Hangul syllable (L V, or L V T).
     * @par Requirements
     * J5-069; Unicode section 3.12.
     * @par ID SWU-SCHEMA-405
     */
    bool append_hangul(normal_buffer& buffer, std::size_t& size, std::uint32_t syllable) noexcept
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.224)
      const std::uint32_t index = syllable - c_hangul_base;
      const std::uint32_t trailing = index % c_trailing_count;
      if (!append(buffer, size, c_leading_base + (index / c_block_count)))
      {
        return false;
      }
      if (!append(buffer, size, c_vowel_base + ((index % c_block_count) / c_trailing_count)))
      {
        return false;
      }
      return (trailing == 0U) ? true : append(buffer, size, c_trailing_base + trailing);
    }

    /**
     * @brief Append the full canonical decomposition of one code point, without recursion.
     * @par Requirements
     * J5-069; Unicode UAX #15.
     * @par ID SWU-SCHEMA-406
     */
    bool decompose(normal_buffer& buffer, std::size_t& size, std::uint32_t code_point) noexcept
    {
      std::array<std::uint32_t, c_pending_limit> pending{};
      std::size_t count = 1U;
      pending[0U] = code_point;
      bool fits = true;
      while (fits && (count != 0U))
      {
        --count;
        const std::uint32_t current = pending[count];
        const unicode_decomposition* mapping = canonical_decomposition(current);
        if ((current >= c_hangul_base) && (current <= c_hangul_last))
        {
          fits = append_hangul(buffer, size, current);
        }
        else if (mapping == nullptr)
        {
          fits = append(buffer, size, current);
        }
        else if ((count + 2U) > pending.size())
        {
          fits = false;
        }
        else
        {
          // The second part is processed after the first, so it is pushed first.
          if (mapping->second != 0U)
          {
            pending[count] = mapping->second;
            ++count;
          }
          pending[count] = mapping->first;
          ++count;
        }
      }
      return fits;
    }

    /**
     * @brief Canonical ordering: stable sort of each run of non-starters by combining class.
     * @par Requirements
     * J5-069; Unicode UAX #15 (canonical ordering algorithm).
     * @par ID SWU-SCHEMA-407
     */
    void order(normal_buffer& buffer, std::size_t size) noexcept
    {
      std::size_t index = 1U;
      while (index < size)
      {
        const std::uint32_t current = buffer[index];
        const std::uint32_t current_class = combining_class_of(current);
        std::size_t at = index;
        while ((current_class != 0U) && (at > 0U) && (combining_class_of(buffer[at - 1U]) > current_class))
        {
          buffer[at] = buffer[at - 1U];
          --at;
        }
        buffer[at] = current;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.223)
        ++index;
      }
    }

    /**
     * @brief Composite of a pair, Hangul (L+V, LV+T) included.
     * @par Requirements
     * J5-069; Unicode UAX #15 and section 3.12.
     * @par ID SWU-SCHEMA-408
     */
    std::optional<std::uint32_t> compose_pair(std::uint32_t first, std::uint32_t second) noexcept
    {
      const bool leading = (first >= c_leading_base) && (first < (c_leading_base + c_leading_count));
      const bool vowel = (second >= c_vowel_base) && (second < (c_vowel_base + c_vowel_count));
      if (leading && vowel)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.221)
        const std::uint32_t pair = ((first - c_leading_base) * c_vowel_count) + (second - c_vowel_base);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.222)
        return c_hangul_base + (pair * c_trailing_count);
      }
      const bool lv =
        (first >= c_hangul_base) && (first <= c_hangul_last) && (((first - c_hangul_base) % c_trailing_count) == 0U);
      const bool trailing = (second > c_trailing_base) && (second < (c_trailing_base + c_trailing_count));
      if (lv && trailing)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.220)
        return first + (second - c_trailing_base);
      }
      return canonical_composition(first, second);
    }

    /**
     * @brief Canonical composition in place; returns the composed length.
     * @par Requirements
     * J5-069; Unicode UAX #15 (canonical composition algorithm).
     * @par ID SWU-SCHEMA-409
     */
    std::size_t compose(normal_buffer& buffer, std::size_t size) noexcept
    {
      if (size == 0U)
      {
        return 0U;
      }
      std::size_t starter = 0U;
      const std::uint32_t first_class = combining_class_of(buffer[0U]);
      std::uint32_t last_class = (first_class == 0U) ? 0U : c_blocked;
      std::size_t written = 1U;
      std::size_t index = 1U;
      while (index < size)
      {
        const std::uint32_t current = buffer[index];
        const std::uint32_t current_class = combining_class_of(current);
        const auto composite = compose_pair(buffer[starter], current);
        const bool unblocked = (last_class < current_class) || (last_class == 0U);
        if (composite.has_value() && unblocked)
        {
          buffer[starter] = *composite;
        }
        else
        {
          starter = (current_class == 0U) ? written : starter;
          last_class = current_class;
          buffer[written] = current;
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.219)
          ++written;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.218)
        ++index;
      }
      return written;
    }
  } // namespace

  /**
   * @brief Report whether a label is in Unicode Normalization Form C.
   * @par Requirements
   * J5-069; RFC 5891 section 5.4; Unicode UAX #15; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-403
   */
  bool is_nfc(const std::array<std::uint32_t, c_max_label_code_points>& label, std::size_t count) noexcept
  {
    normal_buffer buffer{};
    std::size_t size = 0U;
    std::size_t index = 0U;
    while (index < count)
    {
      if (!decompose(buffer, size, label[index]))
      {
        return false;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.217)
      ++index;
    }
    order(buffer, size);
    const std::size_t composed = compose(buffer, size);
    if (composed != count)
    {
      return false;
    }
    index = 0U;
    while (index < count)
    {
      if (buffer[index] != label[index])
      {
        return false;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.216)
      ++index;
    }
    return true;
  }
} // namespace adaptive_json5::detail
