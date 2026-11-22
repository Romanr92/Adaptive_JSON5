#include "schema/uri.hpp"

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "schema/pod_vector.hpp"

#include <cstddef>
#include <string_view>

namespace adaptive_json5::detail
{
  namespace
  {
    using status = result<void, schema_error>;

    /**
     * @brief Report whether c may appear in a scheme after its first letter.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-052
     */
    bool is_scheme_char(char c) noexcept
    {
      return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '+') ||
             (c == '-') || (c == '.');
    }

    /**
     * @brief Lowercase an ASCII letter.
     * @par Requirements
     * P044; ADR 0006 decision 8.
     * @par ID SWU-SCHEMA-053
     */
    char lower(char c) noexcept
    {
      return ((c >= 'A') && (c <= 'Z')) ? static_cast<char>(c - 'A' + 'a') : c;
    }

    /**
     * @brief Append text with ASCII letters lowercased.
     * @par Requirements
     * P044; ADR 0006 decision 8.
     * @par ID SWU-SCHEMA-054
     */
    status append_lower(pod_vector<char>& out, std::string_view text) noexcept
    {
      for (const char c : text)
      {
        const auto pushed = out.push_back(lower(c));
        if (!pushed.has_value())
        {
          return pushed;
        }
      }
      return status::success();
    }

    /**
     * @brief Append an authority with its host lowercased; userinfo and port keep their case.
     * @par Requirements
     * P044; ADR 0006 decision 8.
     * @par ID SWU-SCHEMA-055
     */
    status append_authority(pod_vector<char>& out, std::string_view authority) noexcept
    {
      const std::size_t at = authority.rfind('@');
      const std::size_t host_start = (at == std::string_view::npos) ? 0U : (at + 1U);
      std::size_t host_end = authority.size();
      const std::size_t colon = authority.rfind(':');
      const std::size_t bracket = authority.rfind(']');
      if ((colon != std::string_view::npos) && (colon >= host_start) &&
          ((bracket == std::string_view::npos) || (colon > bracket)))
      {
        host_end = colon;
      }
      const auto userinfo = append_text(out, authority.substr(0U, host_start));
      if (!userinfo.has_value())
      {
        return userinfo;
      }
      const auto host = append_lower(out, authority.substr(host_start, host_end - host_start));
      if (!host.has_value())
      {
        return host;
      }
      return append_text(out, authority.substr(host_end));
    }

    /**
     * @brief Remove the last path segment written after floor, including its '/'.
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-056
     */
    void pop_segment(pod_vector<char>& out, std::size_t floor) noexcept
    {
      std::size_t end = out.size();
      while (end > floor)
      {
        --end;
        if (out.get(end) == '/')
        {
          break;
        }
      }
      out.truncate(end);
    }

    /**
     * @brief Append path with dot segments removed (RFC 3986 section 5.2.4).
     * @par Requirements
     * P044; J5-055; ADR 0006 decision 8.
     * @par ID SWU-SCHEMA-057
     */
    status append_without_dots(pod_vector<char>& out, std::string_view path) noexcept
    {
      constexpr std::string_view c_slash{"/"};
      const std::size_t floor = out.size();
      std::string_view input = path;
      while (!input.empty())
      {
        if ((input.substr(0U, 3U) == "../"))
        {
          input.remove_prefix(3U);
        }
        else if (input.substr(0U, 2U) == "./")
        {
          input.remove_prefix(2U);
        }
        else if (input.substr(0U, 3U) == "/./")
        {
          input.remove_prefix(2U);
        }
        else if (input == "/.")
        {
          input = c_slash;
        }
        else if (input.substr(0U, 4U) == "/../")
        {
          input.remove_prefix(3U);
          pop_segment(out, floor);
        }
        else if (input == "/..")
        {
          input = c_slash;
          pop_segment(out, floor);
        }
        else if ((input == ".") || (input == ".."))
        {
          input = std::string_view{};
        }
        else
        {
          const std::size_t next = input.find('/', 1U);
          const std::size_t length = (next == std::string_view::npos) ? input.size() : next;
          const auto appended = append_text(out, input.substr(0U, length));
          if (!appended.has_value())
          {
            return appended;
          }
          input.remove_prefix(length);
        }
      }
      return status::success();
    }

    /**
     * @brief Merge a relative path with the base path (RFC 3986 section 5.2.3).
     * @par Requirements
     * P044; J5-055.
     * @par ID SWU-SCHEMA-058
     */
    status merge_paths(const uri_parts& base, std::string_view path, pod_vector<char>& merged) noexcept
    {
      if (base.has_authority && base.path.empty())
      {
        const auto slash = merged.push_back('/');
        if (!slash.has_value())
        {
          return slash;
        }
      }
      else
      {
        const std::size_t last = base.path.rfind('/');
        if (last != std::string_view::npos)
        {
          const auto directory = append_text(merged, base.path.substr(0U, last + 1U));
          if (!directory.has_value())
          {
            return directory;
          }
        }
      }
      return append_text(merged, path);
    }

    /**
     * @brief Append the normalized target from its five components.
     * @par Requirements
     * P044; J5-055; ADR 0006 decision 8.
     * @par ID SWU-SCHEMA-059
     */
    status compose(pod_vector<char>& out, const uri_parts& target, std::string_view path) noexcept
    {
      auto written = append_lower(out, target.scheme);
      if (written.has_value())
      {
        written = out.push_back(':');
      }
      if (written.has_value() && target.has_authority)
      {
        written = append_text(out, "//");
        if (written.has_value())
        {
          written = append_authority(out, target.authority);
        }
      }
      if (written.has_value())
      {
        written = append_without_dots(out, path);
      }
      if (written.has_value() && target.has_query)
      {
        written = out.push_back('?');
        if (written.has_value())
        {
          written = append_text(out, target.query);
        }
      }
      if (written.has_value() && target.has_fragment && !target.fragment.empty())
      {
        written = out.push_back('#');
        if (written.has_value())
        {
          written = append_text(out, target.fragment);
        }
      }
      return written;
    }
  } // namespace

  /**
   * @brief Split a URI reference into its five components.
   * @par Requirements
   * P044; J5-055.
   * @par ID SWU-SCHEMA-047
   */
  uri_parts split_uri(std::string_view text) noexcept
  {
    uri_parts parts{};
    std::string_view rest = text;

    const std::size_t hash = rest.find('#');
    if (hash != std::string_view::npos)
    {
      parts.has_fragment = true;
      parts.fragment = rest.substr(hash + 1U);
      rest = rest.substr(0U, hash);
    }
    const std::size_t question = rest.find('?');
    if (question != std::string_view::npos)
    {
      parts.has_query = true;
      parts.query = rest.substr(question + 1U);
      rest = rest.substr(0U, question);
    }

    std::size_t scheme_end = 0U;
    while ((scheme_end < rest.size()) && is_scheme_char(rest[scheme_end]))
    {
      ++scheme_end;
    }
    const bool letter_first =
      !rest.empty() && (((rest[0U] >= 'a') && (rest[0U] <= 'z')) || ((rest[0U] >= 'A') && (rest[0U] <= 'Z')));
    if (letter_first && (scheme_end < rest.size()) && (rest[scheme_end] == ':'))
    {
      parts.has_scheme = true;
      parts.scheme = rest.substr(0U, scheme_end);
      rest = rest.substr(scheme_end + 1U);
    }

    if (rest.substr(0U, 2U) == "//")
    {
      rest.remove_prefix(2U);
      const std::size_t slash = rest.find('/');
      parts.has_authority = true;
      parts.authority = rest.substr(0U, slash);
      rest = (slash == std::string_view::npos) ? std::string_view{} : rest.substr(slash);
    }
    parts.path = rest;
    return parts;
  }

  /**
   * @brief Resolve a reference against an absolute base URI (RFC 3986 section 5.2).
   * @par Requirements
   * P044; J5-055; ADR 0006 decision 8.
   * @par ID SWU-SCHEMA-048
   */
  result<void, schema_error> resolve_uri(std::string_view base, std::string_view reference, pod_vector<char>& out,
                                         const storage_callbacks& scratch) noexcept
  {
    const uri_parts b = split_uri(base);
    const uri_parts r = split_uri(reference);
    if (!b.has_scheme)
    {
      return status::failure(schema_error::of(schema_error_code::internal_error));
    }

    uri_parts t{};
    pod_vector<char> merged{scratch};
    std::string_view path{};
    if (r.has_scheme)
    {
      t = r;
      path = r.path;
    }
    else
    {
      t.has_scheme = true;
      t.scheme = b.scheme;
      if (r.has_authority)
      {
        t.has_authority = true;
        t.authority = r.authority;
        t.has_query = r.has_query;
        t.query = r.query;
        path = r.path;
      }
      else
      {
        t.has_authority = b.has_authority;
        t.authority = b.authority;
        if (r.path.empty())
        {
          path = b.path;
          t.has_query = r.has_query ? true : b.has_query;
          t.query = r.has_query ? r.query : b.query;
        }
        else
        {
          t.has_query = r.has_query;
          t.query = r.query;
          if (r.path[0U] == '/')
          {
            path = r.path;
          }
          else
          {
            const auto merged_ok = merge_paths(b, r.path, merged);
            if (!merged_ok.has_value())
            {
              return merged_ok;
            }
            path = text_of(merged, 0U, merged.size());
          }
        }
      }
      t.has_fragment = r.has_fragment;
      t.fragment = r.fragment;
    }
    return compose(out, t, path);
  }

  /**
   * @brief Return the text before the first '#', the URI without its fragment.
   * @par Requirements
   * P044; J5-055.
   * @par ID SWU-SCHEMA-049
   */
  std::string_view without_fragment(std::string_view uri) noexcept
  {
    return uri.substr(0U, uri.find('#'));
  }

  /**
   * @brief Borrow a pod_vector<char> as text.
   * @par Requirements
   * P044.
   * @par ID SWU-SCHEMA-050
   */
  std::string_view text_of(const pod_vector<char>& text, std::size_t offset, std::size_t length) noexcept
  {
    if (length == 0U)
    {
      return std::string_view{};
    }
    return std::string_view{static_cast<const char*>(text.data()) + offset, length};
  }

  /**
   * @brief Append text to a pod_vector<char>.
   * @par Requirements
   * P044; J5-046.
   * @par ID SWU-SCHEMA-051
   */
  result<void, schema_error> append_text(pod_vector<char>& out, std::string_view text) noexcept
  {
    for (const char c : text)
    {
      const auto pushed = out.push_back(c);
      if (!pushed.has_value())
      {
        return pushed;
      }
    }
    return status::success();
  }
} // namespace adaptive_json5::detail
