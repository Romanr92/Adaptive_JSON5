#include "schema/unicode_property.hpp"

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
    /**
     * @brief Binary search of a sorted name table.
     * @par Requirements
     * J5-060; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-366
     */
    template <std::size_t N>
    std::optional<std::uint32_t> find_name(const std::array<unicode_value_name, N>& names,
                                           std::string_view name) noexcept
    {
      std::size_t low = 0U;
      std::size_t high = N;
      while (low < high)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.209)
        const std::size_t middle = low + ((high - low) / 2U);
        const unicode_value_name& entry = names[middle];
        if (entry.name == name)
        {
          return entry.value;
        }
        if (entry.name < name)
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.208)
          low = middle + 1U;
        }
        else
        {
          high = middle;
        }
      }
      return std::nullopt;
    }

    /**
     * @brief Binary search of a sorted, disjoint range table; the entry containing code_point.
     * @par Requirements
     * J5-060; J5-069; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-367
     */
    template <std::size_t N>
    const unicode_value_range* find_range(const std::array<unicode_value_range, N>& ranges,
                                          std::uint32_t code_point) noexcept
    {
      std::size_t low = 0U;
      std::size_t high = N;
      while (low < high)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.207)
        const std::size_t middle = low + ((high - low) / 2U);
        const unicode_value_range& entry = ranges[middle];
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
          // codeql::misra_deviation_next_line(DEV-101.206)
          low = middle + 1U;
        }
        else
        {
          return &entry;
        }
      }
      return nullptr;
    }
  } // namespace

  /**
   * @brief Category bit mask of a General_Category value name or alias.
   * @par Requirements
   * J5-060; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-359
   */
  std::optional<std::uint32_t> general_category_mask(std::string_view name) noexcept
  {
    return find_name(c_general_category_names, name);
  }

  /**
   * @brief Script number of a script name or alias.
   * @par Requirements
   * J5-060; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-360
   */
  std::optional<std::uint32_t> script_number(std::string_view name) noexcept
  {
    return find_name(c_script_names, name);
  }

  /**
   * @brief Category bit of a code point.
   * @par Requirements
   * J5-060; J5-069; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-361
   */
  std::uint32_t general_category_bit(std::uint32_t code_point) noexcept
  {
    const unicode_value_range* entry = find_range(c_general_category_ranges, code_point);
    return (entry == nullptr) ? c_unassigned_category_mask : (1U << static_cast<std::uint32_t>(entry->value));
  }

  /**
   * @brief Script number of a code point.
   * @par Requirements
   * J5-060; J5-069; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-362
   */
  std::uint32_t script_of(std::uint32_t code_point) noexcept
  {
    const unicode_value_range* entry = find_range(c_script_ranges, code_point);
    return (entry == nullptr) ? c_unknown_script : static_cast<std::uint32_t>(entry->value);
  }

  /**
   * @brief Joining_Type of a code point.
   * @par Requirements
   * J5-069; RFC 5892 Appendix A.2; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-363
   */
  std::optional<joining_type> joining_type_of(std::uint32_t code_point) noexcept
  {
    const unicode_value_range* entry = find_range(c_joining_type_ranges, code_point);
    return (entry == nullptr) ? std::nullopt : std::optional<joining_type>{static_cast<joining_type>(entry->value)};
  }

  /**
   * @brief Report whether a code point is a Virama.
   * @par Requirements
   * J5-069; RFC 5892 Appendix A.1 and A.2; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-364
   */
  bool is_virama(std::uint32_t code_point) noexcept
  {
    return find_range(c_virama_ranges, code_point) != nullptr;
  }

  /**
   * @brief IDNA2008 derived property of a code point.
   * @par Requirements
   * J5-069; RFC 5892 section 3; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-365
   */
  std::optional<idna_property> idna_property_of(std::uint32_t code_point) noexcept
  {
    const unicode_value_range* entry = find_range(c_idna_ranges, code_point);
    return (entry == nullptr) ? std::nullopt : std::optional<idna_property>{static_cast<idna_property>(entry->value)};
  }
} // namespace adaptive_json5::detail
