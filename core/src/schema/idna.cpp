#include "schema/idna.hpp"

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
    using label_buffer = std::array<std::uint32_t, c_max_label_code_points>;

    // RFC 3492 section 5 parameter values for Punycode.
    constexpr std::uint32_t c_base = 36U;
    constexpr std::uint32_t c_tmin = 1U;
    constexpr std::uint32_t c_tmax = 26U;
    constexpr std::uint32_t c_skew = 38U;
    constexpr std::uint32_t c_damp = 700U;
    constexpr std::uint32_t c_initial_bias = 72U;
    constexpr std::uint32_t c_initial_n = 0x80U;
    constexpr std::uint32_t c_max_value = std::numeric_limits<std::uint32_t>::max();
    constexpr std::uint32_t c_max_code_point = 0x10FFFFU;
    constexpr std::uint32_t c_hyphen = 0x2DU;
    constexpr std::uint32_t c_small_l = 0x6CU;
    constexpr std::uint32_t c_middle_dot = 0xB7U;
    constexpr std::uint32_t c_greek_keraia = 0x375U;
    constexpr std::uint32_t c_hebrew_geresh = 0x5F3U;
    constexpr std::uint32_t c_hebrew_gershayim = 0x5F4U;
    constexpr std::uint32_t c_arabic_indic_first = 0x660U;
    constexpr std::uint32_t c_arabic_indic_last = 0x669U;
    constexpr std::uint32_t c_extended_arabic_indic_first = 0x6F0U;
    constexpr std::uint32_t c_extended_arabic_indic_last = 0x6F9U;
    constexpr std::uint32_t c_zero_width_non_joiner = 0x200CU;
    constexpr std::uint32_t c_zero_width_joiner = 0x200DU;
    constexpr std::uint32_t c_katakana_middle_dot = 0x30FBU;
    constexpr std::size_t c_ace_prefix_length = 4U; // "xn--"
    constexpr std::size_t c_no_basic = 0U;

    /**
     * @brief Value of a Punycode digit (a-z, A-Z: 0-25; 0-9: 26-35), or nullopt.
     * @par Requirements
     * J5-069; RFC 3492 section 5.
     * @par ID SWU-SCHEMA-379
     */
    std::optional<std::uint32_t> digit_value(char c) noexcept
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.35)
      const auto unit = static_cast<std::uint32_t>(static_cast<unsigned char>(c));
      if ((unit >= 0x61U) && (unit <= 0x7AU))
      {
        return unit - 0x61U;
      }
      if ((unit >= 0x41U) && (unit <= 0x5AU))
      {
        return unit - 0x41U;
      }
      if ((unit >= 0x30U) && (unit <= 0x39U))
      {
        return (unit - 0x30U) + 26U;
      }
      return std::nullopt;
    }

    /**
     * @brief Bias adaptation after each decoded code point.
     * @par Requirements
     * J5-069; RFC 3492 section 6.1.
     * @par ID SWU-SCHEMA-380
     */
    std::uint32_t adapt(std::uint32_t delta, std::uint32_t points, bool first) noexcept
    {
      std::uint32_t scaled = first ? (delta / c_damp) : (delta / 2U);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.197)
      scaled += scaled / points;
      std::uint32_t k = 0U;
      while (scaled > (((c_base - c_tmin) * c_tmax) / 2U))
      {
        scaled /= c_base - c_tmin;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.196)
        k += c_base;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.195)
      return k + (((c_base - c_tmin + 1U) * scaled) / (scaled + c_skew));
    }

    /**
     * @brief Threshold t of digit position k (RFC 3492 section 6.2).
     * @par Requirements
     * J5-069; RFC 3492 section 6.2.
     * @par ID SWU-SCHEMA-381
     */
    std::uint32_t threshold(std::uint32_t k, std::uint32_t bias) noexcept
    {
      if (k <= bias)
      {
        return c_tmin;
      }
      return (k >= (bias + c_tmax)) ? c_tmax : (k - bias);
    }

    /**
     * @brief Read one generalized variable-length integer starting at position into i.
     * @par Requirements
     * J5-069; RFC 3492 section 6.2.
     * @return false for a missing or invalid digit, or overflow.
     * @par ID SWU-SCHEMA-382
     */
    bool read_delta(std::string_view input, std::size_t& position, std::uint32_t bias, std::uint32_t& i) noexcept
    {
      std::uint32_t weight = 1U;
      std::uint32_t k = c_base;
      for (;;)
      {
        const auto digit = (position < input.size()) ? digit_value(input[position]) : std::nullopt;
        if (!digit.has_value() || (*digit > ((c_max_value - i) / weight)))
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.194)
        ++position;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.193)
        i += *digit * weight;
        const std::uint32_t t = threshold(k, bias);
        if (*digit < t)
        {
          return true;
        }
        if (weight > (c_max_value / (c_base - t)))
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.192)
        weight *= c_base - t;
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.191)
        k += c_base;
      }
    }

    /**
     * @brief Insert code point at index of the first count entries of output.
     * @par Requirements
     * J5-069; RFC 3492 section 6.2.
     * @par ID SWU-SCHEMA-383
     */
    void insert_at(label_buffer& output, std::size_t count, std::size_t index, std::uint32_t code_point) noexcept
    {
      std::size_t at = count;
      while (at > index)
      {
        output[at] = output[at - 1U];
        --at;
      }
      output[index] = code_point;
    }

    /**
     * @brief Copy the basic code points before the last '-' into output.
     * @par Requirements
     * J5-069; RFC 3492 section 6.2.
     * @return The count copied and the position of the first encoded digit, or nullopt when there
     * are too many.
     * @par ID SWU-SCHEMA-384
     */
    std::optional<std::size_t> copy_basic(std::string_view input, label_buffer& output, std::size_t& start) noexcept
    {
      const std::size_t delimiter = input.rfind('-');
      start = 0U;
      if (delimiter == std::string_view::npos)
      {
        return c_no_basic;
      }
      if (delimiter > output.size())
      {
        return std::nullopt;
      }
      std::size_t index = 0U;
      while (index < delimiter)
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
        // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
        // codeql::misra_deviation_next_line(DEV-102.34)
        output[index] = static_cast<std::uint32_t>(static_cast<unsigned char>(input[index]));
        ++index;
      }
      start = delimiter + 1U;
      return delimiter;
    }

    /**
     * @brief Report whether a code point is a surrogate or beyond the Unicode range.
     * @par Requirements
     * J5-069; RFC 3492 section 6.2.
     * @par ID SWU-SCHEMA-385
     */
    bool is_invalid_code_point(std::uint32_t code_point) noexcept
    {
      return (code_point > c_max_code_point) || ((code_point >= 0xD800U) && (code_point <= 0xDFFFU));
    }

    /**
     * @brief Report whether the label contains a code point of one of the scripts.
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.7.
     * @par ID SWU-SCHEMA-386
     */
    bool has_script(const label_buffer& label, std::size_t count, std::uint32_t first, std::uint32_t second,
                    std::uint32_t third) noexcept
    {
      std::size_t index = 0U;
      while (index < count)
      {
        const std::uint32_t script = script_of(label[index]);
        if ((script == first) || (script == second) || (script == third))
        {
          return true;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.190)
        ++index;
      }
      return false;
    }

    /**
     * @brief Report whether the label contains a code point in [low, high].
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.8 and A.9.
     * @par ID SWU-SCHEMA-387
     */
    bool has_range(const label_buffer& label, std::size_t count, std::uint32_t low, std::uint32_t high) noexcept
    {
      std::size_t index = 0U;
      while (index < count)
      {
        if ((label[index] >= low) && (label[index] <= high))
        {
          return true;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.189)
        ++index;
      }
      return false;
    }

    /**
     * @brief Report whether the code point at index has the given script.
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.4 to A.6.
     * @par ID SWU-SCHEMA-388
     */
    bool script_at(const label_buffer& label, std::size_t count, std::size_t index, std::string_view script) noexcept
    {
      const auto number = script_number(script);
      return (index < count) && number.has_value() && (script_of(label[index]) == *number);
    }

    /**
     * @brief Report whether a code point joins on the given side (L/D before, R/D after).
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.1.
     * @par ID SWU-SCHEMA-389
     */
    bool joins(std::uint32_t code_point, joining_type side) noexcept
    {
      const auto type = joining_type_of(code_point);
      return type.has_value() && ((*type == side) || (*type == joining_type::d_type));
    }

    /**
     * @brief CONTEXTJ rule of ZERO WIDTH NON-JOINER: after a Virama, or between a left-joining and a
     * right-joining code point with only transparent ones in between.
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.1.
     * @par ID SWU-SCHEMA-390
     */
    bool zero_width_non_joiner_allowed(const label_buffer& label, std::size_t count, std::size_t index) noexcept
    {
      if ((index > 0U) && is_virama(label[index - 1U]))
      {
        return true;
      }
      std::size_t before = index;
      while ((before > 0U) && (joining_type_of(label[before - 1U]) == joining_type::t_type))
      {
        --before;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.188)
      std::size_t after = index + 1U;
      while ((after < count) && (joining_type_of(label[after]) == joining_type::t_type))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.187)
        ++after;
      }
      return (before > 0U) && joins(label[before - 1U], joining_type::l_type) && (after < count) &&
             joins(label[after], joining_type::r_type);
    }

    /**
     * @brief CONTEXTJ rule of the code point at index (RFC 5892 Appendix A.1 and A.2).
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.1 and A.2.
     * @par ID SWU-SCHEMA-391
     */
    bool contextj_allowed(const label_buffer& label, std::size_t count, std::size_t index) noexcept
    {
      if (label[index] == c_zero_width_non_joiner)
      {
        return zero_width_non_joiner_allowed(label, count, index);
      }
      return (label[index] == c_zero_width_joiner) && (index > 0U) && is_virama(label[index - 1U]);
    }

    /**
     * @brief CONTEXTO rules of the digits: Arabic-Indic and Extended Arabic-Indic digits must not
     * be mixed in one label.
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.8 and A.9.
     * @par ID SWU-SCHEMA-395
     */
    bool digit_context_allowed(const label_buffer& label, std::size_t count, std::uint32_t code_point) noexcept
    {
      if ((code_point >= c_arabic_indic_first) && (code_point <= c_arabic_indic_last))
      {
        return !has_range(label, count, c_extended_arabic_indic_first, c_extended_arabic_indic_last);
      }
      return (code_point >= c_extended_arabic_indic_first) && (code_point <= c_extended_arabic_indic_last) &&
             !has_range(label, count, c_arabic_indic_first, c_arabic_indic_last);
    }

    /**
     * @brief CONTEXTO rule of KATAKANA MIDDLE DOT: the label has a Hiragana, Katakana or Han code
     * point.
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.7.
     * @par ID SWU-SCHEMA-396
     */
    bool katakana_dot_allowed(const label_buffer& label, std::size_t count) noexcept
    {
      const auto hiragana = script_number("Hiragana");
      const auto katakana = script_number("Katakana");
      const auto han = script_number("Han");
      return hiragana.has_value() && katakana.has_value() && han.has_value() &&
             has_script(label, count, *hiragana, *katakana, *han);
    }

    /**
     * @brief CONTEXTO rule of the code point at index (RFC 5892 Appendix A.3 to A.9).
     * @par Requirements
     * J5-069; RFC 5892 Appendix A.3 to A.9.
     * @par ID SWU-SCHEMA-392
     */
    bool contexto_allowed(const label_buffer& label, std::size_t count, std::size_t index) noexcept
    {
      const std::uint32_t code_point = label[index];
      if (code_point == c_middle_dot)
      {
        return (index > 0U) && (label[index - 1U] == c_small_l) && ((index + 1U) < count) &&
               (label[index + 1U] == c_small_l);
      }
      if (code_point == c_greek_keraia)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.186)
        return script_at(label, count, index + 1U, "Greek");
      }
      if ((code_point == c_hebrew_geresh) || (code_point == c_hebrew_gershayim))
      {
        return (index > 0U) && script_at(label, count, index - 1U, "Hebrew");
      }
      if (code_point == c_katakana_middle_dot)
      {
        return katakana_dot_allowed(label, count);
      }
      return digit_context_allowed(label, count, code_point);
    }

    /**
     * @brief U-label shape: not all ASCII, no hyphen at either end or in positions 3 and 4, and
     * no combining mark first.
     * @par Requirements
     * J5-069; RFC 5891 sections 4.2.3.1, 4.2.3.2 and 5.4.
     * @par ID SWU-SCHEMA-397
     */
    bool has_u_label_shape(const label_buffer& label, std::size_t count) noexcept
    {
      constexpr std::uint32_t c_no_category = 0U;
      const auto mark_mask = general_category_mask("M");
      const bool hyphens = (label[0U] == c_hyphen) || (label[count - 1U] == c_hyphen) ||
                           ((count >= 4U) && (label[2U] == c_hyphen) && (label[3U] == c_hyphen));
      const bool leading_mark =
        !mark_mask.has_value() || ((general_category_bit(label[0U]) & *mark_mask) != c_no_category);
      return !hyphens && !leading_mark && has_range(label, count, 0x80U, c_max_code_point);
    }

    /**
     * @brief Report whether the code point at index is PVALID or satisfies its CONTEXTJ/CONTEXTO
     * rule.
     * @par Requirements
     * J5-069; RFC 5892 section 3 and Appendix A.
     * @par ID SWU-SCHEMA-398
     */
    bool code_point_allowed(const label_buffer& label, std::size_t count, std::size_t index) noexcept
    {
      const auto property = idna_property_of(label[index]);
      if (!property.has_value())
      {
        return false;
      }
      if (*property == idna_property::contextj)
      {
        return contextj_allowed(label, count, index);
      }
      return (*property == idna_property::pvalid) || contexto_allowed(label, count, index);
    }

    /**
     * @brief Report whether a decoded label is a valid U-label (RFC 5891 section 5.4).
     * @par Requirements
     * J5-069; RFC 5891 sections 4.2.3 and 5.4; RFC 5892 section 3.
     * @par ID SWU-SCHEMA-393
     */
    bool is_u_label(const label_buffer& label, std::size_t count) noexcept
    {
      if (!has_u_label_shape(label, count))
      {
        return false;
      }
      std::size_t index = 0U;
      while (index < count)
      {
        if (!code_point_allowed(label, count, index))
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.185)
        ++index;
      }
      return true;
    }
  } // namespace

  /**
   * @brief Decode the Punycode part of an A-label.
   * @par Requirements
   * J5-069; RFC 3492 section 6.2; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-377
   */
  std::optional<std::size_t> punycode_decode(std::string_view input, label_buffer& output) noexcept
  {
    std::size_t position = 0U;
    const auto basic = copy_basic(input, output, position);
    if (!basic.has_value())
    {
      return std::nullopt;
    }
    std::size_t count = *basic;
    std::uint32_t n = c_initial_n;
    std::uint32_t i = 0U;
    std::uint32_t bias = c_initial_bias;
    while (position < input.size())
    {
      const std::uint32_t old_i = i;
      if (!read_delta(input, position, bias, i) || (count >= output.size()))
      {
        return std::nullopt;
      }
      const auto points = static_cast<std::uint32_t>(count + 1U);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.184)
      bias = adapt(i - old_i, points, old_i == 0U);
      if ((i / points) > (c_max_value - n))
      {
        return std::nullopt;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.183)
      n += i / points;
      i %= points;
      if (is_invalid_code_point(n))
      {
        return std::nullopt;
      }
      insert_at(output, count, static_cast<std::size_t>(i), n);
      ++i;
      ++count;
    }
    return count;
  }

  /**
   * @brief Validate an A-label.
   * @par Requirements
   * J5-069; RFC 5891 section 5.4; RFC 5892 section 3 and Appendix A; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-378
   */
  bool is_a_label(std::string_view label) noexcept
  {
    label_buffer decoded{};
    const auto count = punycode_decode(label.substr(c_ace_prefix_length), decoded);
    return count.has_value() && (*count != 0U) && is_u_label(decoded, *count);
  }
} // namespace adaptive_json5::detail
