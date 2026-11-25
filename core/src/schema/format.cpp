#include "schema/format.hpp"

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/compiled_schema.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "schema/pod_vector.hpp"
#include "schema/regex.hpp"
#include "schema/regex_program.hpp"
#include "schema/schema_work.hpp"
#include "schema/uri.hpp"

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
    constexpr std::array<std::string_view, 13> c_format_names{{"", "date-time", "date", "time", "email", "hostname",
                                                               "ipv4", "ipv6", "uri", "uri-reference", "json-pointer",
                                                               "relative-json-pointer", "regex"}};

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
     * @brief RFC 1123 host name: dot-separated labels of letters, digits and inner hyphens.
     * @par Requirements
     * J5-069; RFC 1123 section 2.1.
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
        if (dot == std::string_view::npos)
        {
          return true;
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
     * @brief Email address: dot-atom or quoted local part, '@', domain (RFC 5321 section 4.1.2).
     * @par Requirements
     * J5-069; RFC 5321.
     * @par ID SWU-SCHEMA-331
     */
    bool is_email(std::string_view text) noexcept
    {
      const std::size_t at = text.rfind('@');
      if (at == std::string_view::npos)
      {
        return false;
      }
      const std::string_view local = text.substr(0U, at);
      const bool dot_atom = only(local, c_local) && !is_one_of(local.front(), ".") && !is_one_of(local.back(), ".") &&
                            (local.find("..") == std::string_view::npos);
      return (local.size() <= c_max_local_part) && (dot_atom || is_quoted_local(local)) &&
             is_mail_domain(text.substr(at + 1U));
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
     * @brief Check the ASCII-only formats (every format except json-pointer and regex variants).
     * @par Requirements
     * J5-069.
     * @par ID SWU-SCHEMA-339
     */
    bool check_text(format_kind format, std::string_view text) noexcept
    {
      switch (format)
      {
      case format_kind::date_time:
        return is_date_time(text);
      case format_kind::date:
        return is_date(text);
      case format_kind::time:
        return is_time(text);
      case format_kind::email:
        return is_email(text);
      case format_kind::hostname:
        return is_hostname(text);
      case format_kind::ipv4:
        return is_ipv4(text);
      case format_kind::ipv6:
        return is_ipv6(text);
      case format_kind::uri:
        return is_uri_reference(text, true);
      case format_kind::uri_reference:
        return is_uri_reference(text, false);
      case format_kind::json_pointer:
        return is_json_pointer(text);
      case format_kind::relative_json_pointer:
        return is_relative_json_pointer(text);
      case format_kind::none:
      case format_kind::regex:
      default:
        return true;
      }
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
    pod_vector<char> copy{scratch};
    const auto ascii = ascii_copy(text, copy);
    if (!ascii.has_value())
    {
      return ascii;
    }
    const bool any_text = (format == format_kind::json_pointer) || (format == format_kind::relative_json_pointer);
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
