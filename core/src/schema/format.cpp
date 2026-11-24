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

    constexpr std::string_view c_digits{"0123456789"};
    constexpr std::string_view c_hex_digits{"0123456789abcdefABCDEF"};
    constexpr std::string_view c_letters{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
    constexpr std::string_view c_label{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-"};
    constexpr std::string_view c_scheme{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-."};
    // RFC 5322 atext plus '.', for dot-atom local parts.
    constexpr std::string_view c_local{
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+/=?^_`{|}~-."};
    // RFC 3986 unreserved, gen-delims, sub-delims and '%' (checked separately).
    constexpr std::string_view c_uri{
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%"};
    constexpr std::size_t c_max_hostname = 253U;
    constexpr std::size_t c_max_label = 63U;
    constexpr std::size_t c_max_local_part = 64U;
    constexpr std::size_t c_ipv6_groups = 8U;
    // A non-ASCII code unit in the ASCII copy; formats that allow any character never inspect it.
    constexpr std::string_view c_non_ascii{"\x7f"};

    /**
     * @brief Names of the supported formats, indexed by format_kind.
     * @par Requirements
     * J5-069.
     */
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
      return ((month == 2U) && leap) ? 29U : c_days[month - 1U];
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
      return rest.has_value() && is_offset(*rest);
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
        rest = rest.substr(dot + 1U);
      }
    }

    /**
     * @brief Email address: dot-atom local part, '@', host name (RFC 5321 section 4.1.2, basic).
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
      return only(local, c_local) && (local.size() <= c_max_local_part) && !is_one_of(local.front(), ".") &&
             !is_one_of(local.back(), ".") && (local.find("..") == std::string_view::npos) &&
             is_hostname(text.substr(at + 1U));
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
      for (std::size_t part = 0U; part < 4U; ++part)
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
        rest = rest.substr(dot + 1U);
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
          return count + 2U;
        }
        if (!only(group, c_hex_digits) || (group.size() > 4U))
        {
          return std::nullopt;
        }
        ++count;
        if (colon == std::string_view::npos)
        {
          return count;
        }
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
     * @brief RFC 3986 URI reference: allowed characters, valid percent escapes, at most one
     * fragment and a valid scheme when there is one.
     * @par Requirements
     * J5-069; RFC 3986 sections 2, 3.1 and 4.1.
     * @param absolute Require a scheme (format uri).
     * @par ID SWU-SCHEMA-335
     */
    bool is_uri_reference(std::string_view text, bool absolute) noexcept
    {
      if ((text.find_first_not_of(c_uri) != std::string_view::npos) || (text.find('#') != text.rfind('#')))
      {
        return false;
      }
      for (std::size_t percent = text.find('%'); percent != std::string_view::npos;
           percent = text.find('%', percent + 1U))
      {
        if (!only(text.substr(percent + 1U, 2U), c_hex_digits) || (text.substr(percent + 1U, 2U).size() != 2U))
        {
          return false;
        }
      }
      const std::size_t colon = text.find(':');
      const std::size_t first_delimiter = text.find_first_of("/?#");
      const bool has_scheme = (colon != std::string_view::npos) && (colon < first_delimiter);
      if (!has_scheme)
      {
        return !absolute;
      }
      const std::string_view scheme = text.substr(0U, colon);
      return only(scheme, c_scheme) && is_one_of(scheme[0U], c_letters);
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
      for (std::size_t tilde = text.find('~'); tilde != std::string_view::npos; tilde = text.find('~', tilde + 1U))
      {
        if (((tilde + 1U) >= text.size()) || !is_one_of(text[tilde + 1U], "01"))
        {
          return false;
        }
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
     * @brief Copy the code units of text as ASCII characters; other units become c_non_ascii.
     * @par Requirements
     * J5-069.
     * @return true when every unit is ASCII, or a storage failure.
     * @par ID SWU-SCHEMA-338
     */
    bool_result ascii_copy(const decoded_string& text, pod_vector<char>& out) noexcept
    {
      constexpr std::string_view c_ascii{
        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
        "\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
        "abcdefghijklmnopqrstuvwxyz{|}~\x7f",
        128U};
      bool ascii = true;
      for (std::size_t index = 0U; index < text.size(); ++index)
      {
        const auto unit = text.unit_at(index);
        if (!unit.has_value())
        {
          return bool_result::failure(schema_error::from_storage(*unit.error_if()));
        }
        const std::size_t code = *unit.value_if();
        ascii = ascii && (code < c_ascii.size());
        const auto pushed = out.push_back((code < c_ascii.size()) ? c_ascii[code] : c_non_ascii[0U]);
        if (!pushed.has_value())
        {
          return bool_result::failure(*pushed.error_if());
        }
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
    for (std::size_t index = 1U; index < c_format_names.size(); ++index)
    {
      if (c_format_names[index] == name)
      {
        return static_cast<format_kind>(index);
      }
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
    constexpr std::string_view c_name_characters{"abcdefghijklmnopqrstuvwxyz0123456789-"};
    for (std::size_t index = 0U; index < name.size(); ++index)
    {
      const auto unit = name.unit_at(index);
      bool matched = false;
      for (const char c : c_name_characters)
      {
        if (unit.has_value() && (*unit.value_if() == static_cast<std::uint16_t>(static_cast<unsigned char>(c))))
        {
          buffer[index] = c;
          matched = true;
        }
      }
      if (!matched)
      {
        return format_kind::none;
      }
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
    return bool_result::success(check_text(format, text_of(copy, 0U, copy.size())));
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
