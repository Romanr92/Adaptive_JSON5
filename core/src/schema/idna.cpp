#include "schema/idna.hpp"

#include "schema/normalization.hpp"
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
     * @brief Punycode encoder state: the A-label so far and the RFC 3492 section 6.3 variables.
     * @par Requirements
     * J5-069; RFC 3492 section 6.3.
     */
    struct punycode_encoder
    {
        a_label_text text;
        std::size_t size;
        std::uint32_t n;
        std::uint32_t delta;
        std::uint32_t bias;
        std::size_t handled;
    };

    /**
     * @brief Punycode digit character of a value below c_base (a-z: 0-25, 0-9: 26-35).
     * @par Requirements
     * J5-069; RFC 3492 section 5.
     * @par ID SWU-SCHEMA-440
     */
    char digit_char(std::uint32_t value) noexcept
    {
      constexpr std::string_view c_digit_chars{"abcdefghijklmnopqrstuvwxyz0123456789"};
      return c_digit_chars[value];
    }

    /**
     * @brief Append a character to the A-label; false when it is full.
     * @par Requirements
     * J5-069; RFC 1035 section 2.3.4.
     * @par ID SWU-SCHEMA-441
     */
    bool put(punycode_encoder& encoder, char c) noexcept
    {
      if (encoder.size >= encoder.text.size())
      {
        return false;
      }
      encoder.text[encoder.size] = c;
      ++encoder.size;
      return true;
    }

    /**
     * @brief Character of a basic code point that may appear in an A-label (lowercase letter,
     * digit or hyphen), or nullopt.
     * @par Requirements
     * J5-069; RFC 5891 section 4.4.
     * @par ID SWU-SCHEMA-442
     */
    std::optional<char> basic_char(std::uint32_t code_point) noexcept
    {
      if ((code_point >= 0x61U) && (code_point <= 0x7AU))
      {
        return digit_char(code_point - 0x61U);
      }
      if ((code_point >= 0x30U) && (code_point <= 0x39U))
      {
        return digit_char((code_point - 0x30U) + 26U);
      }
      if (code_point == c_hyphen)
      {
        return '-';
      }
      return std::nullopt;
    }

    /**
     * @brief Append the basic code points, then the delimiter when there is one.
     * @par Requirements
     * J5-069; RFC 3492 section 6.3.
     * @return The number of basic code points, or nullopt.
     * @par ID SWU-SCHEMA-443
     */
    std::optional<std::size_t> put_basic(punycode_encoder& encoder, const label_buffer& label,
                                         std::size_t count) noexcept
    {
      std::size_t basic = 0U;
      std::size_t index = 0U;
      while (index < count)
      {
        if (label[index] < c_initial_n)
        {
          const auto c = basic_char(label[index]);
          if (!c.has_value() || !put(encoder, *c))
          {
            return std::nullopt;
          }
          ++basic;
        }
        ++index;
      }
      if ((basic != c_no_basic) && !put(encoder, '-'))
      {
        return std::nullopt;
      }
      return basic;
    }

    /**
     * @brief Smallest code point of the label that is at least n.
     * @par Requirements
     * J5-069; RFC 3492 section 6.3.
     * @par ID SWU-SCHEMA-444
     */
    std::uint32_t next_at_least(const label_buffer& label, std::size_t count, std::uint32_t n) noexcept
    {
      std::uint32_t smallest = c_max_value;
      std::size_t index = 0U;
      while (index < count)
      {
        if ((label[index] >= n) && (label[index] < smallest))
        {
          smallest = label[index];
        }
        ++index;
      }
      return smallest;
    }

    /**
     * @brief Append delta as a generalized variable-length integer.
     * @par Requirements
     * J5-069; RFC 3492 section 6.3.
     * @par ID SWU-SCHEMA-445
     */
    bool put_delta(punycode_encoder& encoder) noexcept
    {
      std::uint32_t q = encoder.delta;
      std::uint32_t k = c_base;
      for (;;)
      {
        const std::uint32_t t = threshold(k, encoder.bias);
        if (q < t)
        {
          return put(encoder, digit_char(q));
        }
        if (!put(encoder, digit_char(t + ((q - t) % (c_base - t)))))
        {
          return false;
        }
        q = (q - t) / (c_base - t);
        k += c_base;
      }
    }

    /**
     * @brief One round of the encoder: count the code points below n and encode those equal to n.
     * @par Requirements
     * J5-069; RFC 3492 section 6.3.
     * @par ID SWU-SCHEMA-446
     */
    bool encode_round(punycode_encoder& encoder, const label_buffer& label, std::size_t count,
                      std::size_t basic) noexcept
    {
      std::size_t index = 0U;
      while (index < count)
      {
        if (label[index] < encoder.n)
        {
          ++encoder.delta;
        }
        else if (label[index] == encoder.n)
        {
          if (!put_delta(encoder))
          {
            return false;
          }
          const auto points = static_cast<std::uint32_t>(encoder.handled + 1U);
          encoder.bias = adapt(encoder.delta, points, encoder.handled == basic);
          encoder.delta = 0U;
          ++encoder.handled;
        }
        else
        {
          // A larger code point is encoded in a later round.
        }
        ++index;
      }
      return true;
    }

    /**
     * @brief Report whether two ASCII labels are equal, ignoring the case of letters.
     * @par Requirements
     * J5-069; RFC 5890 section 2.3.2.1.
     * @par ID SWU-SCHEMA-447
     */
    bool same_ignoring_case(std::string_view first, std::string_view second) noexcept
    {
      if (first.size() != second.size())
      {
        return false;
      }
      std::size_t index = 0U;
      while (index < first.size())
      {
        const auto left = digit_value(first[index]);
        const bool same = (first[index] == second[index]) || (left.has_value() && (left == digit_value(second[index])));
        if (!same)
        {
          return false;
        }
        ++index;
      }
      return true;
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
      if (!is_nfc(label, count))
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

    /**
     * @brief Code points of a label: the decoded U-label of an A-label, otherwise the ASCII
     * characters; nullopt when an A-label does not decode or the label is too long.
     * @par Requirements
     * J5-069; RFC 5893 section 2.
     * @par ID SWU-SCHEMA-411
     */
    std::optional<std::size_t> label_code_points(std::string_view label, label_buffer& output) noexcept
    {
      if (has_ace_prefix(label))
      {
        return punycode_decode(label.substr(c_ace_prefix_length), output);
      }
      if (label.size() > output.size())
      {
        return std::nullopt;
      }
      std::size_t index = 0U;
      while (index < label.size())
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
        // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
        // codeql::misra_deviation_next_line(DEV-102.37)
        output[index] = static_cast<std::uint32_t>(static_cast<unsigned char>(label[index]));
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.215)
        ++index;
      }
      return label.size();
    }

    /**
     * @brief Report whether a Bidi_Class is right-to-left (R, AL or AN; RFC 5893 section 1.4).
     * @par Requirements
     * J5-069; RFC 5893 section 1.4.
     * @par ID SWU-SCHEMA-412
     */
    bool is_rtl_class(bidi_class value) noexcept
    {
      return (value == bidi_class::r_class) || (value == bidi_class::al_class) || (value == bidi_class::an_class);
    }

    /**
     * @brief Report whether a Bidi_Class may appear in an RTL (rtl) or LTR label (RFC 5893
     * section 2, conditions 2 and 5).
     * @par Requirements
     * J5-069; RFC 5893 section 2.
     * @par ID SWU-SCHEMA-413
     */
    bool allowed_class(bidi_class value, bool rtl) noexcept
    {
      const bool common = (value == bidi_class::en_class) || (value == bidi_class::es_class) ||
                          (value == bidi_class::cs_class) || (value == bidi_class::et_class) ||
                          (value == bidi_class::on_class) || (value == bidi_class::bn_class) ||
                          (value == bidi_class::nsm_class);
      return common || (rtl ? is_rtl_class(value) : (value == bidi_class::l_class));
    }

    /**
     * @brief Bidi_Class of the last code point that is not NSM (NSM when there is none).
     * @par Requirements
     * J5-069; RFC 5893 section 2, conditions 3 and 6.
     * @par ID SWU-SCHEMA-416
     */
    bidi_class last_class(const label_buffer& label, std::size_t count) noexcept
    {
      std::size_t end = count;
      while ((end > 0U) && (bidi_class_of(label[end - 1U]) == bidi_class::nsm_class))
      {
        --end;
      }
      return (end > 0U) ? bidi_class_of(label[end - 1U]) : bidi_class::nsm_class;
    }

    /**
     * @brief Report whether a label may end with this class: R, AL, EN or AN for RTL labels; L
     * or EN for LTR labels.
     * @par Requirements
     * J5-069; RFC 5893 section 2, conditions 3 and 6.
     * @par ID SWU-SCHEMA-417
     */
    bool ends_well(bidi_class last, bool rtl) noexcept
    {
      if (rtl)
      {
        return (last == bidi_class::r_class) || (last == bidi_class::al_class) || (last == bidi_class::en_class) ||
               (last == bidi_class::an_class);
      }
      return (last == bidi_class::l_class) || (last == bidi_class::en_class);
    }

    /**
     * @brief RFC 5893 section 2 conditions 1 to 6 for one label.
     * @par Requirements
     * J5-069; RFC 5893 section 2.
     * @par ID SWU-SCHEMA-414
     */
    bool label_satisfies_bidi_rule(const label_buffer& label, std::size_t count) noexcept
    {
      const bidi_class first = bidi_class_of(label[0U]);
      if ((first != bidi_class::l_class) && (first != bidi_class::r_class) && (first != bidi_class::al_class))
      {
        return false;
      }
      const bool rtl = first != bidi_class::l_class;
      bool european = false;
      bool arabic = false;
      std::size_t index = 0U;
      while (index < count)
      {
        const bidi_class value = bidi_class_of(label[index]);
        if (!allowed_class(value, rtl))
        {
          return false;
        }
        european = european || (value == bidi_class::en_class);
        arabic = arabic || (value == bidi_class::an_class);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.214)
        ++index;
      }
      return ends_well(last_class(label, count), rtl) && !(rtl && european && arabic);
    }

    /**
     * @brief Report whether any label of the host holds a right-to-left character.
     * @par Requirements
     * J5-069; RFC 5893 section 1.4.
     * @par ID SWU-SCHEMA-415
     */
    bool is_bidi_domain(std::string_view host) noexcept
    {
      std::string_view rest = host;
      for (;;)
      {
        const std::size_t dot = rest.find('.');
        label_buffer label{};
        const auto count = label_code_points(rest.substr(0U, dot), label);
        const std::size_t size = count.has_value() ? *count : 0U;
        std::size_t index = 0U;
        while (index < size)
        {
          if (is_rtl_class(bidi_class_of(label[index])))
          {
            return true;
          }
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.213)
          ++index;
        }
        if (dot == std::string_view::npos)
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.212)
        rest = rest.substr(dot + 1U);
      }
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
    if (!count.has_value() || (*count == 0U) || !is_u_label(decoded, *count))
    {
      return false;
    }
    a_label_text encoded{};
    const auto length = to_a_label(decoded, *count, encoded);
    return length.has_value() && same_ignoring_case(label, std::string_view{encoded.data(), *length});
  }

  /**
   * @brief Encode a label as an A-label.
   * @par Requirements
   * J5-069; RFC 3492 section 6.3; RFC 5891 section 4.4; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-439
   */
  std::optional<std::size_t> to_a_label(const label_buffer& label, std::size_t count, a_label_text& out) noexcept
  {
    punycode_encoder encoder{};
    encoder.n = c_initial_n;
    encoder.bias = c_initial_bias;
    const bool prefixed = put(encoder, 'x') && put(encoder, 'n') && put(encoder, '-') && put(encoder, '-');
    const auto basic = put_basic(encoder, label, count);
    if (!prefixed || !basic.has_value())
    {
      return std::nullopt;
    }
    encoder.handled = *basic;
    while (encoder.handled < count)
    {
      const std::uint32_t smallest = next_at_least(label, count, encoder.n);
      const auto points = static_cast<std::uint32_t>(encoder.handled + 1U);
      encoder.delta += (smallest - encoder.n) * points;
      encoder.n = smallest;
      if (!encode_round(encoder, label, count, *basic))
      {
        return std::nullopt;
      }
      ++encoder.delta;
      ++encoder.n;
    }
    out = encoder.text;
    return encoder.size;
  }

  /**
   * @brief Report whether a label starts with the ACE prefix "xn--".
   * @par Requirements
   * J5-069; RFC 5890 section 2.3.2.1; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-394
   */
  bool has_ace_prefix(std::string_view label) noexcept
  {
    constexpr std::string_view c_x{"xX"};
    constexpr std::string_view c_n{"nN"};
    return (label.size() >= c_ace_prefix_length) && (c_x.find(label[0U]) != std::string_view::npos) &&
           (c_n.find(label[1U]) != std::string_view::npos) && (label.substr(2U, 2U) == "--");
  }

  /**
   * @brief RFC 5893 Bidi rule of a host name.
   * @par Requirements
   * J5-069; RFC 5893 section 2; RFC 5891 section 5.4; ADR 0006 (issue #187).
   * @par ID SWU-SCHEMA-410
   */
  bool satisfies_bidi_rule(std::string_view host) noexcept
  {
    if (!is_bidi_domain(host))
    {
      return true;
    }
    std::string_view rest = host;
    for (;;)
    {
      const std::size_t dot = rest.find('.');
      label_buffer label{};
      const auto count = label_code_points(rest.substr(0U, dot), label);
      if (!count.has_value() || (*count == 0U) || !label_satisfies_bidi_rule(label, *count))
      {
        return false;
      }
      if (dot == std::string_view::npos)
      {
        return true;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.211)
      rest = rest.substr(dot + 1U);
    }
  }
} // namespace adaptive_json5::detail
