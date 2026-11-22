#include "schema/json_pointer.hpp"

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "schema/pod_vector.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  namespace
  {
    using status = result<void, schema_error>;

    /**
     * @brief Return the value of a hexadecimal digit, or nullopt.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-063
     */
    std::optional<unsigned> hex_value(char c) noexcept
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.22)
      if ((c >= '0') && (c <= '9'))
      {
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
        // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
        // codeql::misra_deviation_next_line(DEV-102.21)
        return static_cast<unsigned>(c - '0');
      }
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.20)
      if ((c >= 'a') && (c <= 'f'))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds. URI,
        // JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and code units by value, as
        // RFC 3986, RFC 6901 and Unicode define them.
        // codeql::misra_deviation_next_line(DEV-101.51)
        return static_cast<unsigned>(c - 'a') + 10U;
      }
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.19)
      if ((c >= 'A') && (c <= 'F'))
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds. URI,
        // JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and code units by value, as
        // RFC 3986, RFC 6901 and Unicode define them.
        // codeql::misra_deviation_next_line(DEV-101.50)
        return static_cast<unsigned>(c - 'A') + 10U;
      }
      return std::nullopt;
    }

    /**
     * @brief Decode the character at position, consuming a `%XX` escape when present.
     * @par Requirements
     * P044; J5-055; RFC 6901 section 6.
     * @return The character, or nullopt for a malformed escape. position is advanced.
     * @par ID SWU-SCHEMA-168
     */
    std::optional<char> next_char(std::string_view raw, std::size_t& position) noexcept
    {
      const char c = raw[position];
      if (c != '%')
      {
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.49)
        ++position;
        return c;
      }
      if ((position + 2U) >= raw.size())
      {
        return std::nullopt;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.48)
      const auto high = hex_value(raw[position + 1U]);
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.47)
      const auto low = hex_value(raw[position + 2U]);
      if (!high.has_value() || !low.has_value())
      {
        return std::nullopt;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.46)
      position += 3U;
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds. URI,
      // JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and code units by value, as RFC
      // 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-101.45)
      return static_cast<char>(static_cast<unsigned char>((*high * 16U) + *low));
    }

    /**
     * @brief Return a malformed-pointer failure.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-064
     */
    status malformed() noexcept
    {
      return status::failure(schema_error::of(schema_error_code::unresolved_reference));
    }
  } // namespace

  /**
   * @brief Decode the next reference token of a JSON Pointer taken from a URI fragment.
   * @par Requirements
   * P044; J5-055; J5-064; RFC 6901 sections 3, 4 and 6.
   * @par ID SWU-SCHEMA-060
   */
  result<void, schema_error> next_pointer_token(std::string_view& rest, pod_vector<char>& token) noexcept
  {
    token.truncate(0U);
    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
    // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
    // codeql::misra_deviation_next_line(DEV-102.18)
    if (rest.empty() || (rest[0U] != '/'))
    {
      return malformed();
    }
    rest.remove_prefix(1U);
    const std::size_t end = rest.find('/');
    const std::string_view raw = rest.substr(0U, end);
    rest = (end == std::string_view::npos) ? std::string_view{} : rest.substr(end);

    bool tilde = false;
    std::size_t position = 0U;
    while (position < raw.size())
    {
      const auto c = next_char(raw, position);
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.17)
      if (!c.has_value() || (tilde && (*c != '0') && (*c != '1')))
      {
        return malformed();
      }
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.16)
      if (!tilde && (*c == '~'))
      {
        tilde = true;
        continue;
      }
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.15)
      const char decoded = tilde ? ((*c == '0') ? '~' : '/') : *c;
      tilde = false;
      const auto pushed = token.push_back(decoded);
      if (!pushed.has_value())
      {
        return pushed;
      }
    }
    return tilde ? malformed() : status::success();
  }

  /**
   * @brief Parse a decoded token as an array index (RFC 6901 section 4).
   * @par Requirements
   * P044; J5-055.
   * @par ID SWU-SCHEMA-061
   */
  std::optional<std::size_t> pointer_index(std::string_view token) noexcept
  {
    // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
    // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
    // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
    // codeql::misra_deviation_next_line(DEV-102.14)
    if (token.empty() || ((token.size() > 1U) && (token[0U] == '0')))
    {
      return std::nullopt;
    }
    std::size_t value = 0U;
    for (const char c : token)
    {
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.13)
      if ((c < '0') || (c > '9'))
      {
        return std::nullopt;
      }
      // MISRA rule: RULE-7-0-3 cpp/misra/no-character-numerical-value
      // MISRA justification: URI, JSON Pointer and UTF-8/UTF-16 handling compares and computes ASCII characters and
      // code units by value, as RFC 3986, RFC 6901 and Unicode define them.
      // codeql::misra_deviation_next_line(DEV-102.12)
      const auto digit = static_cast<std::size_t>(c - '0');
      if (value > ((std::numeric_limits<std::size_t>::max() - digit) / 10U))
      {
        return std::nullopt;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.44)
      value = (value * 10U) + digit;
    }
    return value;
  }

  /**
   * @brief Write one RFC 6901 reference token with '~' as `~0` and '/' as `~1`.
   * @par Requirements
   * P046; J5-064; RFC 6901 section 3.
   * @par ID SWU-SCHEMA-062
   */
  std::size_t escape_pointer_token(std::string_view token, char* out, std::size_t capacity) noexcept
  {
    std::size_t needed = 0U;
    for (const char c : token)
    {
      const bool escaped = (c == '~') || (c == '/');
      const char first = escaped ? '~' : c;
      if (needed < capacity)
      {
        out[needed] = first;
      }
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.43)
      ++needed;
      if (escaped)
      {
        if (needed < capacity)
        {
          out[needed] = (c == '~') ? '0' : '1';
        }
        // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
        // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
        // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
        // codeql::misra_deviation_next_line(DEV-101.42)
        ++needed;
      }
    }
    return needed;
  }
} // namespace adaptive_json5::detail
