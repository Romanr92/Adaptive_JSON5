#include "schema/format.hpp"

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "schema/idna.hpp"
#include "schema/pod_vector.hpp"
#include "schema/regex.hpp"
#include "schema/regex_program.hpp"
#include "schema/schema_work.hpp"
#include "schema/uri.hpp"
#include "schema/uts46.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  namespace
  {
    using bool_result = result<bool, schema_error>;

    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.11)
    constexpr std::string_view c_digits{"0123456789"};
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.10)
    constexpr std::string_view c_hex_digits{"0123456789abcdefABCDEF"};
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.9)
    constexpr std::string_view c_letters{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.8)
    constexpr std::string_view c_label{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-"};
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.7)
    constexpr std::string_view c_scheme{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-."};
    // RFC 5322 atext plus '.', for dot-atom local parts.
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.6)
    constexpr std::string_view c_local{
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+/=?^_`{|}~-."};
    // RFC 3986 unreserved, gen-delims, sub-delims and '%' (checked separately).
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.5)
    constexpr std::string_view c_uri{
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%"};
    // Printable ASCII including space (RFC 5321 qtext and quoted-pair characters).
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.4)
    constexpr std::string_view c_printable{
      " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"};
    constexpr std::size_t c_max_hostname = 253U;
    constexpr std::size_t c_max_label = 63U;
    constexpr std::size_t c_max_local_part = 64U;
    constexpr std::size_t c_ipv6_groups = 8U;
    // A non-ASCII code unit in the ASCII copy; formats that allow any character never inspect it.
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.3)
    constexpr std::string_view c_non_ascii{"\x7f"};
    constexpr std::uint32_t c_first_printable = 0x20U; // ' ', the first character of c_printable.
    constexpr std::uint32_t c_ascii_end = 0x80U;
    constexpr std::size_t c_text_start = 0U;

    /**
     * @brief Names of the supported formats, indexed by format_kind.
     * @par Requirements
     * J5-069.
     */
    // MISRA rule: RULE-4-1-2 cpp/misra/redeclaration-of-static-constexpr-data-member
    // MISRA justification: A namespace-scope constexpr std::string_view in an anonymous namespace is declared once;
    // it is not a static data member, so the redeclaration the query reports does not exist.
    // codeql::misra_deviation_next_line(DEV-114.2)
    constexpr std::array<std::string_view, 18> c_format_names{
      {"", "date-time", "date", "time", "email", "hostname", "ipv4", "ipv6", "uri", "uri-reference", "json-pointer",
       "relative-json-pointer", "regex", "iri", "iri-reference", "uri-template", "idn-email", "idn-hostname"}};

    /**
     * @brief Report whether c is one of set.
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-323
     */
    bool is_one_of(char c, std::string_view set) noexcept
    {
      return set.find(c) != std::string_view::npos;
    }

    /**
     * @brief Report whether text is non-empty and made only of characters in set.
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-324
     */
    bool only(std::string_view text, std::string_view set) noexcept
    {
      return !text.empty() && (text.find_first_not_of(set) == std::string_view::npos);
    }

    /**
     * @brief Return the decimal value of a digit run, or nullopt when it is not all digits.
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-325
     */
    std::optional<std::uint32_t> decimal(std::string_view digits) noexcept
    {
      if (!only(digits, c_digits) || (digits.size() > 9U))
      {
        return std::nullopt;
      }
      std::uint32_t value = 0U;
      for (const char c : digits)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.179)
        value = (value * 10U) + static_cast<std::uint32_t>(c_digits.find(c));
      }
      return value;
    }

    /**
     * @brief Return the decimal value of exactly two digits when it is at most limit.
     * @par Requirements
     * J5-069; RFC 3339 section 5.6.
     * @par ID SWU-SCHEMA-326
     */
    bool two_digits_at_most(std::string_view text, std::uint32_t limit) noexcept
    {
      const auto value = decimal(text);
      return (text.size() == 2U) && value.has_value() && (*value <= limit);
    }

    /**
     * @brief Return the number of days in month (1-12) of year (Gregorian leap years).
     * @par Requirements
     * J5-069; RFC 3339 section 5.7.
     * @par ID SWU-SCHEMA-343
     */
    std::uint32_t days_in_month(std::uint32_t year, std::uint32_t month) noexcept
    {
      constexpr std::array<std::uint32_t, 12> c_days{{31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U}};
      const bool leap = ((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U));
      constexpr std::uint32_t c_leap_february = 29U;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds. Comparisons and arithmetic on
      // std::uint16_t code units promote to int without changing the value; the operands are unsigned values below
      // 0x110000 and the results are used as such.
      // codeql::misra_deviation_next_line(DEV-101.178)
      return ((month == 2U) && leap) ? c_leap_february : c_days[month - 1U];
    }

    /**
     * @brief RFC 3339 full-date with the real month length (leap years included).
     * @par Requirements
     * J5-069; RFC 3339 section 5.6.
     * @par ID SWU-SCHEMA-327
     */
    bool is_date(std::string_view text) noexcept
    {
      if ((text.size() != 10U) || !is_one_of(text[4U], "-") || !is_one_of(text[7U], "-"))
      {
        return false;
      }
      const auto year = decimal(text.substr(0U, 4U));
      const auto month = decimal(text.substr(5U, 2U));
      const auto day = decimal(text.substr(8U, 2U));
      if (!year.has_value() || !month.has_value() || !day.has_value() || (*month < 1U) || (*month > 12U))
      {
        return false;
      }
      return (*day >= 1U) && (*day <= days_in_month(*year, *month));
    }

    /**
     * @brief Skip an optional time-secfrac ('.' and at least one digit); nullopt when malformed.
     * @par Requirements
     * J5-069; RFC 3339 section 5.6.
     * @par ID SWU-SCHEMA-344
     */
    std::optional<std::string_view> skip_fraction(std::string_view rest) noexcept
    {
      if (rest.empty() || !is_one_of(rest[0U], "."))
      {
        return rest;
      }
      const std::size_t end = rest.find_first_not_of(c_digits, 1U);
      if ((end == 1U) || (end == std::string_view::npos))
      {
        return std::nullopt;
      }
      return rest.substr(end);
    }

    /**
     * @brief RFC 3339 time-offset: Z or a numeric +hh:mm / -hh:mm offset.
     * @par Requirements
     * J5-069; RFC 3339 section 5.6.
     * @par ID SWU-SCHEMA-345
     */
    bool is_offset(std::string_view text) noexcept
    {
      if ((text.size() == 1U) && is_one_of(text[0U], "Zz"))
      {
        return true;
      }
      return (text.size() == 6U) && is_one_of(text[0U], "+-") && is_one_of(text[3U], ":") &&
             two_digits_at_most(text.substr(1U, 2U), 23U) && two_digits_at_most(text.substr(4U, 2U), 59U);
    }

    /**
     * @brief A leap second (:60) is valid only at 23:59 UTC once the offset is applied.
     * @par Requirements
     * J5-069; RFC 3339 sections 5.6 and 5.7.
     * @param text The full-time text (hh:mm:60...); offset its time-offset part.
     * @par ID SWU-SCHEMA-351
     */
    bool leap_second_allowed(std::string_view text, std::string_view offset) noexcept
    {
      constexpr std::uint32_t c_day_minutes = 1440U;
      constexpr std::uint32_t c_last_minute = 1439U; // 23:59
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.177)
      const std::uint32_t local = (*decimal(text.substr(0U, 2U)) * 60U) + *decimal(text.substr(3U, 2U));
      std::uint32_t shift = 0U;
      if (offset.size() == 6U)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.176)
        shift = (*decimal(offset.substr(1U, 2U)) * 60U) + *decimal(offset.substr(4U, 2U));
      }
      // UTC = local - offset: subtract a positive offset, add a negative one (mod one day).
      const bool positive = (offset.size() == 6U) && is_one_of(offset[0U], "+");
      const std::uint32_t utc =
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.175)
        positive ? (((local + c_day_minutes) - shift) % c_day_minutes) : ((local + shift) % c_day_minutes);
      return utc == c_last_minute;
    }

    /**
     * @brief RFC 3339 full-time: partial-time with optional fraction, then Z or a numeric offset.
     * @par Requirements
     * J5-069; RFC 3339 section 5.6.
     * @par ID SWU-SCHEMA-328
     */
    bool is_time(std::string_view text) noexcept
    {
      if ((text.size() < 9U) || !is_one_of(text[2U], ":") || !is_one_of(text[5U], ":") ||
          !two_digits_at_most(text.substr(0U, 2U), 23U) || !two_digits_at_most(text.substr(3U, 2U), 59U) ||
          !two_digits_at_most(text.substr(6U, 2U), 60U))
      {
        return false;
      }
      const auto rest = skip_fraction(text.substr(8U));
      if (!rest.has_value() || !is_offset(*rest))
      {
        return false;
      }
      return !is_one_of(text[6U], "6") || leap_second_allowed(text, *rest);
    }

    /**
     * @brief RFC 3339 date-time.
     * @par Requirements
     * J5-069; RFC 3339 section 5.6.
     * @par ID SWU-SCHEMA-329
     */
    bool is_date_time(std::string_view text) noexcept
    {
      return (text.size() > 11U) && is_one_of(text[10U], "Tt") && is_date(text.substr(0U, 10U)) &&
             is_time(text.substr(11U));
    }

    /**
     * @brief RFC 1123 host name: dot-separated labels of letters, digits and inner hyphens; a label
     * with the ACE prefix must be a valid IDNA2008 A-label, and a host with right-to-left labels
     * must satisfy the RFC 5893 Bidi rule.
     * @par Requirements
     * J5-069; RFC 1123 section 2.1; RFC 5891 section 5.4; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-330
     */
    bool is_hostname(std::string_view text) noexcept
    {
      if (text.empty() || (text.size() > c_max_hostname))
      {
        return false;
      }
      std::string_view rest = text;
      for (;;)
      {
        const std::size_t dot = rest.find('.');
        const std::string_view label = rest.substr(0U, dot);
        if (!only(label, c_label) || (label.size() > c_max_label) || is_one_of(label.front(), "-") ||
            is_one_of(label.back(), "-"))
        {
          return false;
        }
        const bool a_label_ok = has_ace_prefix(label) ? is_a_label(label) : true;
        if (!a_label_ok)
        {
          return false;
        }
        if (dot == std::string_view::npos)
        {
          return satisfies_bidi_rule(text);
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.174)
        rest = rest.substr(dot + 1U);
      }
    }

    /**
     * @brief Dotted-decimal IPv4 address without leading zeros (RFC 2673 section 3.2).
     * @par Requirements
     * J5-069; RFC 2673.
     * @par ID SWU-SCHEMA-332
     */
    bool is_ipv4(std::string_view text) noexcept
    {
      std::string_view rest = text;
      std::size_t part = 0U;
      while (part < 4U)
      {
        const std::size_t dot = rest.find('.');
        const std::string_view octet = rest.substr(0U, dot);
        const auto value = decimal(octet);
        if (!value.has_value() || (octet.size() > 3U) || (*value > 255U) ||
            ((octet.size() > 1U) && is_one_of(octet[0U], "0")))
        {
          return false;
        }
        if (part == 3U)
        {
          return dot == std::string_view::npos;
        }
        if (dot == std::string_view::npos)
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.173)
        rest = rest.substr(dot + 1U);
        ++part;
      }
      return false;
    }

    /**
     * @brief Count the colon-separated groups of an IPv6 half; an IPv4 tail counts as two.
     * @par Requirements
     * J5-069; RFC 4291 section 2.2.
     * @return The group count, or nullopt for a malformed group.
     * @par ID SWU-SCHEMA-333
     */
    std::optional<std::size_t> ipv6_groups(std::string_view half, bool allow_ipv4) noexcept
    {
      if (half.empty())
      {
        return 0U;
      }
      std::size_t count = 0U;
      std::string_view rest = half;
      for (;;)
      {
        const std::size_t colon = rest.find(':');
        const std::string_view group = rest.substr(0U, colon);
        if ((colon == std::string_view::npos) && allow_ipv4 && is_one_of('.', group) && is_ipv4(group))
        {
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
          // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.172)
          return count + 2U;
        }
        if (!only(group, c_hex_digits) || (group.size() > 4U))
        {
          return std::nullopt;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.171)
        ++count;
        if (colon == std::string_view::npos)
        {
          return count;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.170)
        rest = rest.substr(colon + 1U);
      }
    }

    /**
     * @brief IPv6 address in RFC 4291 text form (at most one "::", optional IPv4 tail).
     * @par Requirements
     * J5-069; RFC 4291 section 2.2.
     * @par ID SWU-SCHEMA-334
     */
    bool is_ipv6(std::string_view text) noexcept
    {
      const std::size_t gap = text.find("::");
      if (gap == std::string_view::npos)
      {
        const auto groups = ipv6_groups(text, true);
        return groups.has_value() && (*groups == c_ipv6_groups);
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.169)
      const std::string_view right = text.substr(gap + 2U);
      if (right.find("::") != std::string_view::npos)
      {
        return false;
      }
      const auto left_groups = ipv6_groups(text.substr(0U, gap), false);
      const auto right_groups = ipv6_groups(right, true);
      return left_groups.has_value() && right_groups.has_value() && ((*left_groups + *right_groups) < c_ipv6_groups);
    }

    /**
     * @brief RFC 5321 quoted-string local part: printable ASCII and space, with backslash pairs.
     * @par Requirements
     * J5-069; RFC 5321 section 4.1.2.
     * @par ID SWU-SCHEMA-352
     */
    bool is_quoted_local(std::string_view local) noexcept
    {
      if ((local.size() < 2U) || !is_one_of(local.front(), "\"") || !is_one_of(local.back(), "\""))
      {
        return false;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.168)
      const std::string_view inner = local.substr(1U, local.size() - 2U);
      bool escaped = false;
      for (const char c : inner)
      {
        if (!is_one_of(c, c_printable))
        {
          return false;
        }
        if (escaped)
        {
          escaped = false;
        }
        else if (is_one_of(c, "\\"))
        {
          escaped = true;
        }
        else if (is_one_of(c, "\""))
        {
          return false;
        }
        else
        {
          // Any other printable character or space is qtext.
        }
      }
      return !escaped;
    }

    /**
     * @brief RFC 5321 domain: a host name or an address literal [IPv4] / [IPv6:...].
     * @par Requirements
     * J5-069; RFC 5321 section 4.1.3.
     * @par ID SWU-SCHEMA-353
     */
    bool is_mail_domain(std::string_view domain) noexcept
    {
      if (domain.empty() || !is_one_of(domain.front(), "[") || !is_one_of(domain.back(), "]"))
      {
        return is_hostname(domain);
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.167)
      const std::string_view literal = domain.substr(1U, domain.size() - 2U);
      const bool ipv6_tag = (literal.size() > 5U) && is_one_of(literal[0U], "Ii") && is_one_of(literal[1U], "Pp") &&
                            is_one_of(literal[2U], "Vv") && is_one_of(literal[3U], "6") && is_one_of(literal[4U], ":");
      return ipv6_tag ? is_ipv6(literal.substr(5U)) : is_ipv4(literal);
    }

    /**
     * @brief RFC 5321 local part: a dot-atom or a quoted string of at most c_max_local_part octets.
     * @par Requirements
     * J5-069; RFC 5321 sections 4.1.2 and 4.5.3.1.1.
     * @par ID SWU-SCHEMA-448
     */
    bool is_local_part(std::string_view local) noexcept
    {
      const bool dot_atom = only(local, c_local) && !is_one_of(local.front(), ".") && !is_one_of(local.back(), ".") &&
                            (local.find("..") == std::string_view::npos);
      const bool quoted = dot_atom ? false : is_quoted_local(local);
      return (local.size() <= c_max_local_part) && (dot_atom || quoted);
    }

    /**
     * @brief Email address: dot-atom or quoted local part, '@', domain (RFC 5321 section 4.1.2).
     * @par Requirements
     * J5-069; RFC 5321.
     * @par ID SWU-SCHEMA-331
     */
    bool is_email(std::string_view text) noexcept
    {
      const std::size_t at = text.rfind('@');
      if ((at == std::string_view::npos) || !is_local_part(text.substr(0U, at)))
      {
        return false;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.210)
      return is_mail_domain(text.substr(at + 1U));
    }

    /**
     * @brief Return the port text after an RFC 3986 host (reg-name or [IP-literal]); nullopt when
     * the host is malformed.
     * @par Requirements
     * J5-069; RFC 3986 sections 3.2.2 and 3.2.3.
     * @par ID SWU-SCHEMA-355
     */
    std::optional<std::string_view> port_of(std::string_view host_port) noexcept
    {
      if (host_port.empty() || !is_one_of(host_port.front(), "["))
      {
        const std::size_t colon = host_port.find(':');
        if (host_port.substr(0U, colon).find_first_of("[]") != std::string_view::npos)
        {
          return std::nullopt;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.166)
        return (colon == std::string_view::npos) ? std::string_view{} : host_port.substr(colon + 1U);
      }
      const std::size_t close = host_port.find(']');
      if (close == std::string_view::npos)
      {
        return std::nullopt;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.165)
      const std::string_view literal = host_port.substr(1U, close - 1U);
      const bool future = !literal.empty() && is_one_of(literal.front(), "vV");
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.164)
      const std::string_view after = host_port.substr(close + 1U);
      if ((!future && !is_ipv6(literal)) || (!after.empty() && !is_one_of(after.front(), ":")))
      {
        return std::nullopt;
      }
      return after.empty() ? after : after.substr(1U);
    }

    /**
     * @brief Report whether every '%' starts a two-hex-digit escape (RFC 3986 section 2.1).
     * @par Requirements
     * J5-069; RFC 3986 section 2.1.
     * @par ID SWU-SCHEMA-356
     */
    bool valid_percent_escapes(std::string_view text) noexcept
    {
      std::size_t percent = text.find('%');
      while (percent != std::string_view::npos)
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.163)
        const std::string_view escape = text.substr(percent + 1U, 2U);
        if ((escape.size() != 2U) || !only(escape, c_hex_digits))
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.162)
        percent = text.find('%', percent + 1U);
      }
      return true;
    }

    /**
     * @brief RFC 3986 authority: optional userinfo '@', host (reg-name or [IP-literal]) and digit port.
     * @par Requirements
     * J5-069; RFC 3986 section 3.2.
     * @par ID SWU-SCHEMA-354
     */
    bool is_authority(std::string_view authority) noexcept
    {
      const std::size_t at = authority.find('@');
      if ((at != std::string_view::npos) && (authority.find('@', at + 1U) != std::string_view::npos))
      {
        return false;
      }
      const std::string_view userinfo = (at == std::string_view::npos) ? std::string_view{} : authority.substr(0U, at);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.161)
      const std::string_view host_port = (at == std::string_view::npos) ? authority : authority.substr(at + 1U);
      if (userinfo.find_first_of("[]") != std::string_view::npos)
      {
        return false;
      }
      const auto port = port_of(host_port);
      return port.has_value() && (port->empty() || only(*port, c_digits));
    }

    /**
     * @brief RFC 3986 URI reference: allowed characters, valid percent escapes, at most one
     * fragment, a valid scheme when there is one, a valid authority after "//" and brackets
     * only inside the authority.
     * @par Requirements
     * J5-069; RFC 3986 sections 2, 3 and 4.1.
     * @param absolute Require a scheme (format uri).
     * @par ID SWU-SCHEMA-335
     */
    bool is_uri_reference(std::string_view text, bool absolute) noexcept
    {
      if ((text.find_first_not_of(c_uri) != std::string_view::npos) || (text.find('#') != text.rfind('#')) ||
          !valid_percent_escapes(text))
      {
        return false;
      }
      const std::size_t colon = text.find(':');
      const std::size_t first_delimiter = text.find_first_of("/?#");
      const bool has_scheme = (colon != std::string_view::npos) && (colon < first_delimiter);
      if (has_scheme && (!only(text.substr(0U, colon), c_scheme) || !is_one_of(text[0U], c_letters)))
      {
        return false;
      }
      if (!has_scheme && absolute)
      {
        return false;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.160)
      std::string_view rest = has_scheme ? text.substr(colon + 1U) : text;
      if (rest.substr(0U, 2U) == "//")
      {
        const std::size_t end = rest.find_first_of("/?#", 2U);
        if (!is_authority(rest.substr(2U, end - 2U)))
        {
          return false;
        }
        rest = (end == std::string_view::npos) ? std::string_view{} : rest.substr(end);
      }
      return rest.find_first_of("[]") == std::string_view::npos;
    }

    /**
     * @brief RFC 6901 JSON Pointer: empty, or '/'-prefixed tokens where '~' is followed by 0 or 1.
     * @par Requirements
     * J5-069; RFC 6901 section 3.
     * @par ID SWU-SCHEMA-336
     */
    bool is_json_pointer(std::string_view text) noexcept
    {
      if (!text.empty() && !is_one_of(text[0U], "/"))
      {
        return false;
      }
      std::size_t tilde = text.find('~');
      while (tilde != std::string_view::npos)
      {
        if (((tilde + 1U) >= text.size()) || !is_one_of(text[tilde + 1U], "01"))
        {
          return false;
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.159)
        tilde = text.find('~', tilde + 1U);
      }
      return true;
    }

    /**
     * @brief Relative JSON Pointer: a non-negative integer, then '#' or a JSON Pointer.
     * @par Requirements
     * J5-069; draft-handrews-relative-json-pointer-01 section 3.
     * @par ID SWU-SCHEMA-337
     */
    bool is_relative_json_pointer(std::string_view text) noexcept
    {
      const std::size_t end = text.find_first_not_of(c_digits);
      const std::string_view prefix = text.substr(0U, end);
      if (prefix.empty() || ((prefix.size() > 1U) && is_one_of(prefix[0U], "0")))
      {
        return false;
      }
      const std::string_view rest = text.substr(prefix.size());
      return (rest.size() == 1U) ? is_one_of(rest[0U], "#") : is_json_pointer(rest);
    }

    /**
     * @brief RFC 6570 varname: varchars (letters, digits, '_' and percent escapes) separated by
     * single dots.
     * @par Requirements
     * J5-069; RFC 6570 section 2.3.
     * @par ID SWU-SCHEMA-418
     */
    bool is_varname(std::string_view name) noexcept
    {
      constexpr std::string_view c_varname{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_%."};
      return only(name, c_varname) && valid_percent_escapes(name) && !is_one_of(name.front(), ".") &&
             !is_one_of(name.back(), ".") && (name.find("..") == std::string_view::npos);
    }

    /**
     * @brief RFC 6570 varspec: a varname with an optional prefix (":" and 1-9999 without a
     * leading zero) or explode ("*") modifier.
     * @par Requirements
     * J5-069; RFC 6570 sections 2.3 and 2.4.
     * @par ID SWU-SCHEMA-419
     */
    bool is_varspec(std::string_view spec) noexcept
    {
      constexpr std::size_t c_max_length_digits = 4U;
      const std::size_t colon = spec.find(':');
      if (colon != std::string_view::npos)
      {
        const std::string_view length = spec.substr(colon).substr(1U);
        return is_varname(spec.substr(0U, colon)) && (length.size() <= c_max_length_digits) && only(length, c_digits) &&
               !is_one_of(length.front(), "0");
      }
      const bool explode = !spec.empty() && is_one_of(spec.back(), "*");
      return is_varname(explode ? spec.substr(0U, spec.rfind('*')) : spec);
    }

    /**
     * @brief RFC 6570 expression body: an optional operator and a comma-separated variable list.
     * @par Requirements
     * J5-069; RFC 6570 section 2.2.
     * @par ID SWU-SCHEMA-420
     */
    bool is_expression(std::string_view body) noexcept
    {
      // Level 2 and 3 operators, and the operators RFC 6570 reserves.
      constexpr std::string_view c_operators{"+#./;?&=,!@|"};
      std::string_view rest = (!body.empty() && is_one_of(body.front(), c_operators)) ? body.substr(1U) : body;
      for (;;)
      {
        const std::size_t comma = rest.find(',');
        if (!is_varspec(rest.substr(0U, comma)))
        {
          return false;
        }
        if (comma == std::string_view::npos)
        {
          return true;
        }
        rest = rest.substr(comma).substr(1U);
      }
    }

    /**
     * @brief RFC 6570 URI Template: literals (with valid percent escapes) and "{...}" expressions.
     * The apostrophe is a literal character (an RFC 6570 erratum, as the test suite expects).
     * @par Requirements
     * J5-069; RFC 6570 section 2; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-421
     */
    bool is_uri_template(std::string_view text) noexcept
    {
      // RFC 6570 literals; an allowed non-ASCII character is '~' in the copy.
      constexpr std::string_view c_template_literal{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$&'()*+,-./:;=?@[]_~%"};
      std::string_view rest = text;
      for (;;)
      {
        const std::size_t open = rest.find('{');
        const std::string_view literal = rest.substr(0U, open);
        if ((literal.find_first_not_of(c_template_literal) != std::string_view::npos) ||
            !valid_percent_escapes(literal))
        {
          return false;
        }
        if (open == std::string_view::npos)
        {
          return true;
        }
        const std::size_t close = rest.find('}', open);
        if ((close == std::string_view::npos) || !is_expression(rest.substr(open, close - open).substr(1U)))
        {
          return false;
        }
        rest = rest.substr(close).substr(1U);
      }
    }

    /**
     * @brief Return the printable ASCII character of a code unit, or c_non_ascii for every other
     * unit (control characters and non-ASCII), without using character values numerically.
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-357
     */
    char ascii_of(std::uint32_t unit) noexcept
    {
      const bool printable = (unit >= c_first_printable) && ((unit - c_first_printable) < c_printable.size());
      // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
      // MISRA justification: Comparisons and arithmetic on std::uint16_t code units promote to int without changing
      // the value; the operands are unsigned values below 0x110000 and the results are used as such.
      // codeql::misra_deviation_next_line(DEV-106.22)
      return printable ? c_printable[unit - c_first_printable] : c_non_ascii[0U];
    }

    /**
     * @brief Copy the code units of text as ASCII characters; other units become c_non_ascii.
     * @par Requirements
     * J5-069.
     * @return true when every unit is ASCII, or a storage failure.
     * @par ID SWU-SCHEMA-338
     */
    bool_result ascii_copy(const decoded_string& text, pod_vector<char>& out) noexcept
    {
      bool ascii = true;
      std::size_t index = 0U;
      while (index < text.size())
      {
        const auto unit = text.unit_at(index);
        if (!unit.has_value())
        {
          return bool_result::failure(schema_error::from_storage(*unit.error_if()));
        }
        const std::uint32_t code = static_cast<std::uint32_t>(*unit.value_if());
        ascii = ascii && (code < c_ascii_end);
        const auto pushed = out.push_back(ascii_of(code));
        if (!pushed.has_value())
        {
          return bool_result::failure(*pushed.error_if());
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.158)
        ++index;
      }
      return bool_result::success(ascii);
    }

    /**
     * @brief A code point read from UTF-16 text and the number of code units it occupies.
     * @par Requirements
     * J5-069.
     */
    // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
    // MISRA justification: Aggregates and owning types follow the rule of zero or declare exactly the move-only set
    // they need; the implicitly provided members are the intended ones.
    // codeql::misra_deviation_next_line(DEV-104.46)
    struct code_point_read
    {
        std::uint32_t value;
        std::size_t units;
    };

    /**
     * @brief Read the code point at index; an unpaired surrogate is returned as itself.
     * @par Requirements
     * J5-069.
     * @return The code point, or a storage failure.
     * @par ID SWU-SCHEMA-422
     */
    result<code_point_read, schema_error> read_code_point(const decoded_string& text, std::size_t index) noexcept
    {
      using read_result = result<code_point_read, schema_error>;
      const auto unit = text.unit_at(index);
      if (!unit.has_value())
      {
        return read_result::failure(schema_error::from_storage(*unit.error_if()));
      }
      const std::uint32_t high = static_cast<std::uint32_t>(*unit.value_if());
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.229)
      const std::size_t next = index + 1U;
      if ((high < 0xD800U) || (high > 0xDBFFU) || (next >= text.size()))
      {
        return read_result::success(code_point_read{high, 1U});
      }
      const auto low = text.unit_at(next);
      if (!low.has_value())
      {
        return read_result::failure(schema_error::from_storage(*low.error_if()));
      }
      const std::uint32_t second = static_cast<std::uint32_t>(*low.value_if());
      if ((second < 0xDC00U) || (second > 0xDFFFU))
      {
        return read_result::success(code_point_read{high, 1U});
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.230)
      const std::uint32_t offset = ((high - 0xD800U) << 10U) | (second - 0xDC00U);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.231)
      const std::uint32_t value = 0x10000U + offset;
      return read_result::success(code_point_read{value, 2U});
    }

    /**
     * @brief Report whether code_point is an RFC 3987 ucschar.
     * @par Requirements
     * J5-069; RFC 3987 section 2.2.
     * @par ID SWU-SCHEMA-423
     */
    bool is_ucschar(std::uint32_t code_point) noexcept
    {
      if ((code_point & 0xFFFFU) >= 0xFFFEU)
      {
        return false;
      }
      return ((code_point >= 0xA0U) && (code_point <= 0xD7FFU)) ||
             ((code_point >= 0xF900U) && (code_point <= 0xFDCFU)) ||
             ((code_point >= 0xFDF0U) && (code_point <= 0xFFEFU)) ||
             ((code_point >= 0x10000U) && (code_point < 0xE0000U)) ||
             ((code_point >= 0xE1000U) && (code_point < 0xF0000U));
    }

    /**
     * @brief Report whether code_point is an RFC 3987 iprivate character.
     * @par Requirements
     * J5-069; RFC 3987 section 2.2.
     * @par ID SWU-SCHEMA-424
     */
    bool is_iprivate(std::uint32_t code_point) noexcept
    {
      return ((code_point >= 0xE000U) && (code_point <= 0xF8FFU)) ||
             ((code_point >= 0xF0000U) && ((code_point & 0xFFFFU) < 0xFFFEU));
    }

    /**
     * @brief Return the copy character of a code point for the IRI formats: ASCII as itself,
     * an allowed non-ASCII character as c_stand_in and every other character as c_non_ascii.
     * @par Requirements
     * J5-069; RFC 3987 section 2.2.
     * @param private_allowed iprivate characters are allowed here (an IRI query, or a URI Template).
     * @par ID SWU-SCHEMA-425
     */
    char iri_char(std::uint32_t code_point, bool private_allowed) noexcept
    {
      // Unreserved and a template literal, but not a scheme, hex-digit or port character.
      constexpr std::string_view c_stand_in{"~"};
      if (code_point < c_ascii_end)
      {
        return ascii_of(code_point);
      }
      const bool allowed = is_ucschar(code_point) || (private_allowed && is_iprivate(code_point));
      return allowed ? c_stand_in[0U] : c_non_ascii[0U];
    }

    /**
     * @brief Copy text for the IRI formats, one character per code point (see iri_char); iprivate
     * characters are allowed in the query (after the first '?' and before '#') or, with
     * private_anywhere, everywhere.
     * @par Requirements
     * J5-069; RFC 3987 section 2.2; RFC 6570 section 2.1.
     * @return true, or a storage failure.
     * @par ID SWU-SCHEMA-426
     */
    bool_result iri_copy(const decoded_string& text, pod_vector<char>& out, bool private_anywhere) noexcept
    {
      bool in_query = false;
      bool in_fragment = false;
      std::size_t index = 0U;
      while (index < text.size())
      {
        const auto read = read_code_point(text, index);
        if (!read.has_value())
        {
          return bool_result::failure(*read.error_if());
        }
        const code_point_read point = *read.value_if();
        const char c = iri_char(point.value, private_anywhere || (in_query && !in_fragment));
        in_fragment = in_fragment || is_one_of(c, "#");
        in_query = in_query || (!in_fragment && is_one_of(c, "?"));
        const auto pushed = out.push_back(c);
        if (!pushed.has_value())
        {
          return bool_result::failure(*pushed.error_if());
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.232)
        index += point.units;
      }
      return bool_result::success(true);
    }

    /**
     * @brief Decode text into code points (see read_code_point).
     * @par Requirements
     * J5-069.
     * @return true, or a storage failure.
     * @par ID SWU-SCHEMA-449
     */
    bool_result decode_code_points(const decoded_string& text, pod_vector<std::uint32_t>& out) noexcept
    {
      std::size_t index = 0U;
      while (index < text.size())
      {
        const auto read = read_code_point(text, index);
        if (!read.has_value())
        {
          return bool_result::failure(*read.error_if());
        }
        const code_point_read point = *read.value_if();
        const auto pushed = out.push_back(point.value);
        if (!pushed.has_value())
        {
          return bool_result::failure(*pushed.error_if());
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.260)
        index += point.units;
      }
      return bool_result::success(true);
    }

    /**
     * @brief Internationalized host name of the code points [first, end): its UTS #46 ASCII form
     * is a valid host name (format hostname: labels, A-labels, lengths and the Bidi rule) or,
     * for mail, a mail domain (address literals included).
     * @par Requirements
     * J5-069; RFC 5890; RFC 5891; UTS #46; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-450
     */
    bool is_idn_domain(const pod_vector<std::uint32_t>& code_points, std::size_t first, std::size_t end,
                       bool mail) noexcept
    {
      ascii_host host{};
      const auto length = idn_to_ascii(code_points, first, end, host);
      if (!length.has_value())
      {
        return false;
      }
      const std::string_view ascii{host.data(), *length};
      return mail ? is_mail_domain(ascii) : is_hostname(ascii);
    }

    /**
     * @brief Number of UTF-8 octets of a code point.
     * @par Requirements
     * J5-069; RFC 3629 section 3.
     * @par ID SWU-SCHEMA-451
     */
    std::size_t utf8_octets(std::uint32_t code_point) noexcept
    {
      if (code_point < c_ascii_end)
      {
        return 1U;
      }
      if (code_point < 0x800U)
      {
        return 2U;
      }
      if (code_point < 0x10000U)
      {
        return 3U;
      }
      return 4U;
    }

    /**
     * @brief RFC 6531 local part of the code points [0, at): every non-ASCII character is allowed
     * in a dot-atom or a quoted string, and the local part has at most c_max_local_part UTF-8
     * octets.
     * @par Requirements
     * J5-069; RFC 6531 section 3.3; RFC 5321 section 4.5.3.1.1.
     * @par ID SWU-SCHEMA-452
     */
    bool is_idn_local_part(const pod_vector<std::uint32_t>& code_points, std::size_t at) noexcept
    {
      // atext and qtext in the copy; RFC 6531 allows every non-ASCII character in both.
      constexpr std::string_view c_stand_in{"~"};
      std::array<char, c_max_local_part> local{};
      std::size_t octets = 0U;
      std::size_t index = 0U;
      while (index < at)
      {
        const std::uint32_t code_point = code_points.get(index);
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.261)
        octets += utf8_octets(code_point);
        if (octets > c_max_local_part)
        {
          return false;
        }
        local[index] = (code_point < c_ascii_end) ? ascii_of(code_point) : c_stand_in[0U];
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
        // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked
        // before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.262)
        ++index;
      }
      return is_local_part(std::string_view{local.data(), at});
    }

    /**
     * @brief RFC 6531 internationalized email address: a local part, the last '@' and an
     * internationalized domain or address literal.
     * @par Requirements
     * J5-069; RFC 6531 section 3.3; ADR 0006 (issue #187).
     * @par ID SWU-SCHEMA-453
     */
    bool is_idn_email(const pod_vector<std::uint32_t>& code_points) noexcept
    {
      constexpr std::uint32_t c_commercial_at = 0x40U;
      std::size_t index = code_points.size();
      while (index > 0U)
      {
        --index;
        if (code_points.get(index) == c_commercial_at)
        {
          if (!is_idn_local_part(code_points, index))
          {
            return false;
          }
          // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
          // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the
          // loop condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves
          // checked before combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
          // codeql::misra_deviation_next_line(DEV-101.263)
          return is_idn_domain(code_points, index + 1U, code_points.size(), true);
        }
      }
      return false;
    }

    /**
     * @brief Check idn-hostname and idn-email on the decoded code points.
     * @par Requirements
     * J5-069; RFC 5890; RFC 6531; ADR 0006 (issue #187).
     * @return The verdict, or a storage failure.
     * @par ID SWU-SCHEMA-454
     */
    bool_result check_idn(format_kind format, const decoded_string& text, const storage_callbacks& scratch) noexcept
    {
      pod_vector<std::uint32_t> code_points{scratch};
      const auto decoded = decode_code_points(text, code_points);
      if (!decoded.has_value())
      {
        return decoded;
      }
      if (format == format_kind::idn_email)
      {
        return bool_result::success(is_idn_email(code_points));
      }
      return bool_result::success(is_idn_domain(code_points, c_text_start, code_points.size(), false));
    }

    /**
     * @brief Check the network formats (email, hostname, IP addresses, URIs, IRIs and URI
     * Templates); none and regex are valid here (regex is checked before the copy).
     * @par Requirements
     * J5-069; RFC 3987; RFC 6570.
     * @par ID SWU-SCHEMA-427
     */
    bool check_network_text(format_kind format, std::string_view text) noexcept
    {
      if (format == format_kind::email)
      {
        return is_email(text);
      }
      if (format == format_kind::hostname)
      {
        return is_hostname(text);
      }
      if (format == format_kind::ipv4)
      {
        return is_ipv4(text);
      }
      if (format == format_kind::ipv6)
      {
        return is_ipv6(text);
      }
      if (format == format_kind::uri_template)
      {
        return is_uri_template(text);
      }
      const bool absolute = (format == format_kind::uri) || (format == format_kind::iri);
      const bool reference = (format == format_kind::uri_reference) || (format == format_kind::iri_reference);
      return (absolute || reference) ? is_uri_reference(text, absolute) : true;
    }

    /**
     * @brief Check the formats that work on a character copy (every format except regex).
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-339
     */
    bool check_text(format_kind format, std::string_view text) noexcept
    {
      if (format == format_kind::date_time)
      {
        return is_date_time(text);
      }
      if (format == format_kind::date)
      {
        return is_date(text);
      }
      if (format == format_kind::time)
      {
        return is_time(text);
      }
      if (format == format_kind::json_pointer)
      {
        return is_json_pointer(text);
      }
      if (format == format_kind::relative_json_pointer)
      {
        return is_relative_json_pointer(text);
      }
      return check_network_text(format, text);
    }

    /**
     * @brief format regex: a pattern that is not malformed ECMA-262 (unsupported features count
     * as valid syntax).
     * @par Requirements
     * J5-069; J5-060; ECMA-262 5.1 section 15.10.
     * @par ID SWU-SCHEMA-340
     */
    bool_result is_regex(const decoded_string& text, schema_work& work, const storage_callbacks& scratch) noexcept
    {
      pod_vector<regex_inst> code{scratch};
      pod_vector<regex_range> ranges{scratch};
      std::size_t steps = 0U;
      const auto compiled =
        compile_regex(text, code, ranges, scratch, steps, work.budget().max_regex_steps, std::nullopt);
      const auto charged = work.regex_steps(steps);
      if (!charged.has_value())
      {
        return bool_result::failure(*charged.error_if());
      }
      if (compiled.has_value())
      {
        return bool_result::success(true);
      }
      const schema_error& failed = *compiled.error_if();
      if (failed.code() != schema_error_code::invalid_schema)
      {
        return bool_result::failure(failed);
      }
      return bool_result::success(failed.invalid_schema_reason() != invalid_schema_reason::invalid_pattern);
    }
  } // namespace

  /**
   * @brief Return the format_kind spelled by name, or none for an unsupported format.
   * @par Requirements
   * J5-069.
   * @par ID SWU-SCHEMA-320
   */
  format_kind format_from_name(std::string_view name) noexcept
  {
    std::size_t index = 1U;
    while (index < c_format_names.size())
    {
      if (c_format_names[index] == name)
      {
        return static_cast<format_kind>(index);
      }
      ++index;
    }
    return format_kind::none;
  }

  /**
   * @brief Return the format_kind spelled by a decoded name, or none.
   * @par Requirements
   * J5-069.
   * @par ID SWU-SCHEMA-321
   */
  format_kind format_from_name(const decoded_string& name) noexcept
  {
    std::array<char, 24> buffer{};
    if (name.size() > buffer.size())
    {
      return format_kind::none;
    }
    std::size_t index = 0U;
    while (index < name.size())
    {
      const auto unit = name.unit_at(index);
      if (!unit.has_value())
      {
        return format_kind::none;
      }
      buffer[index] = ascii_of(static_cast<std::uint32_t>(*unit.value_if()));
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset, count and code-point arithmetic is bounded by a preceding check, the loop
      // condition, a container size or the Unicode range (code points <= 0x10FFFF, surrogate halves checked before
      // combining), so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.157)
      ++index;
    }
    return format_from_name(std::string_view{buffer.data(), name.size()});
  }

  /**
   * @brief Report whether text is a valid value of format.
   * @par Requirements
   * J5-069; J5-065.
   * @par ID SWU-SCHEMA-322
   */
  result<bool, schema_error> check_format(format_kind format, const decoded_string& text, schema_work& work,
                                          const storage_callbacks& scratch) noexcept
  {
    const auto charged = work.regex_steps(text.size());
    if (!charged.has_value())
    {
      return bool_result::failure(*charged.error_if());
    }
    if (format == format_kind::regex)
    {
      return is_regex(text, work, scratch);
    }
    if ((format == format_kind::idn_hostname) || (format == format_kind::idn_email))
    {
      return check_idn(format, text, scratch);
    }
    pod_vector<char> copy{scratch};
    const bool iri_like =
      (format == format_kind::iri) || (format == format_kind::iri_reference) || (format == format_kind::uri_template);
    const auto ascii = iri_like ? iri_copy(text, copy, format == format_kind::uri_template) : ascii_copy(text, copy);
    if (!ascii.has_value())
    {
      return ascii;
    }
    const bool any_text =
      iri_like || (format == format_kind::json_pointer) || (format == format_kind::relative_json_pointer);
    if (!*ascii.value_if() && !any_text)
    {
      return bool_result::success(false);
    }
    return bool_result::success(check_text(format, text_of(copy, c_text_start, copy.size())));
  }
} // namespace adaptive_json5::detail

namespace adaptive_json5
{
  /**
   * @brief Report whether `format` value name is asserted when compile_options::assert_format is set.
   * @par Requirements
   * J5-069.
   * @par ID SWU-SCHEMA-341
   */
  bool format_assertion_supported(std::string_view name) noexcept
  {
    return detail::format_from_name(name) != detail::format_kind::none;
  }
} // namespace adaptive_json5
