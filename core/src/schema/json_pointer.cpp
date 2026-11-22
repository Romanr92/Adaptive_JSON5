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
      if ((c >= '0') && (c <= '9'))
      {
        return static_cast<unsigned>(c - '0');
      }
      if ((c >= 'a') && (c <= 'f'))
      {
        return static_cast<unsigned>(c - 'a') + 10U;
      }
      if ((c >= 'A') && (c <= 'F'))
      {
        return static_cast<unsigned>(c - 'A') + 10U;
      }
      return std::nullopt;
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
      char c = raw[position];
      if (c == '%')
      {
        if ((position + 2U) >= raw.size())
        {
          return malformed();
        }
        const auto high = hex_value(raw[position + 1U]);
        const auto low = hex_value(raw[position + 2U]);
        if (!high.has_value() || !low.has_value())
        {
          return malformed();
        }
        c = static_cast<char>(static_cast<unsigned char>((*high * 16U) + *low));
        position += 3U;
      }
      else
      {
        ++position;
      }

      if (tilde)
      {
        tilde = false;
        if ((c != '0') && (c != '1'))
        {
          return malformed();
        }
        const auto pushed = token.push_back((c == '0') ? '~' : '/');
        if (!pushed.has_value())
        {
          return pushed;
        }
      }
      else if (c == '~')
      {
        tilde = true;
      }
      else
      {
        const auto pushed = token.push_back(c);
        if (!pushed.has_value())
        {
          return pushed;
        }
      }
    }
    if (tilde)
    {
      return malformed();
    }
    return status::success();
  }

  /**
   * @brief Parse a decoded token as an array index (RFC 6901 section 4).
   * @par Requirements
   * P044; J5-055.
   * @par ID SWU-SCHEMA-061
   */
  std::optional<std::size_t> pointer_index(std::string_view token) noexcept
  {
    if (token.empty() || ((token.size() > 1U) && (token[0U] == '0')))
    {
      return std::nullopt;
    }
    std::size_t value = 0U;
    for (const char c : token)
    {
      if ((c < '0') || (c > '9'))
      {
        return std::nullopt;
      }
      const auto digit = static_cast<std::size_t>(c - '0');
      if (value > ((std::numeric_limits<std::size_t>::max() - digit) / 10U))
      {
        return std::nullopt;
      }
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
      ++needed;
      if (escaped)
      {
        if (needed < capacity)
        {
          out[needed] = (c == '~') ? '0' : '1';
        }
        ++needed;
      }
    }
    return needed;
  }
} // namespace adaptive_json5::detail
