#include "cli/report.hpp"

#include "adaptive_json5/text_position.hpp"
#include "cli/cli_app.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  namespace
  {
    constexpr std::string_view c_hex{"0123456789abcdef"};

    /**
     * @brief Return the byte at index as an unsigned value 0..255.
     * @par Requirements
     * P057; J5-071.
     * @par ID SWU-CLI-038
     */
    std::uint32_t byte_at(std::string_view text, std::size_t index) noexcept
    {
      return static_cast<std::uint32_t>(static_cast<unsigned char>(text[index]));
    }

    /**
     * @brief Sequence length announced by a UTF-8 lead byte, or 0 for an invalid lead.
     * @par Requirements
     * P057; J5-071; RFC 3629.
     * @par ID SWU-CLI-056
     */
    std::size_t sequence_length(std::uint32_t lead) noexcept
    {
      if (lead < 0x80U)
      {
        return 1U;
      }
      if (lead < 0xC2U)
      {
        return 0U;
      }
      if (lead < 0xE0U)
      {
        return 2U;
      }
      if (lead < 0xF0U)
      {
        return 3U;
      }
      return (lead < 0xF5U) ? 4U : 0U;
    }

    /**
     * @brief Report whether the second byte is allowed after lead (no overlong forms,
     * surrogates or code points above U+10FFFF).
     * @par Requirements
     * P057; J5-071; RFC 3629 section 4.
     * @par ID SWU-CLI-057
     */
    bool second_allowed(std::uint32_t lead, std::uint32_t second) noexcept
    {
      const bool overlong = ((lead == 0xE0U) && (second < 0xA0U)) || ((lead == 0xF0U) && (second < 0x90U));
      const bool surrogate = (lead == 0xEDU) && (second >= 0xA0U);
      const bool too_high = (lead == 0xF4U) && (second >= 0x90U);
      return !overlong && !surrogate && !too_high;
    }

    /**
     * @brief Length of the well-formed UTF-8 sequence at index, or 0 when it is malformed.
     * @par Requirements
     * P057; J5-071; RFC 3629.
     * @par ID SWU-CLI-039
     */
    std::size_t utf8_length(std::string_view text, std::size_t index) noexcept
    {
      const std::uint32_t lead = byte_at(text, index);
      const std::size_t length = sequence_length(lead);
      if ((length == 0U) || ((index + length) > text.size()))
      {
        return 0U;
      }
      for (std::size_t offset = 1U; offset < length; ++offset)
      {
        if ((byte_at(text, index + offset) & 0xC0U) != 0x80U)
        {
          return 0U;
        }
      }
      return ((length == 1U) || second_allowed(lead, byte_at(text, index + 1U))) ? length : 0U;
    }

    /**
     * @brief Report whether text is well-formed UTF-8.
     * @par Requirements
     * P057; J5-071.
     * @par ID SWU-CLI-040
     */
    bool is_utf8(std::string_view text) noexcept
    {
      std::size_t index = 0U;
      while (index < text.size())
      {
        const std::size_t length = utf8_length(text, index);
        if (length == 0U)
        {
          return false;
        }
        index += length;
      }
      return true;
    }

    /**
     * @brief Write one ASCII byte of a JSON string, escaping quotes, backslashes and controls.
     * @par Requirements
     * P057; J5-071; RFC 8259 section 7.
     * @par ID SWU-CLI-041
     */
    void put_escaped(text_writer& out, char c, std::uint32_t value) noexcept
    {
      if (c == '"')
      {
        out.put("\\\"");
      }
      else if (c == '\\')
      {
        out.put("\\\\");
      }
      else if (value < 0x20U)
      {
        const std::array<char, 6> escape{{'\\', 'u', '0', '0', c_hex[value >> 4U], c_hex[value & 0xFU]}};
        out.put(std::string_view{escape.data(), escape.size()});
      }
      else
      {
        out.put(std::string_view{&c, 1U});
      }
    }

    /**
     * @brief Write a JSON property name and colon (name is plain ASCII).
     * @par Requirements
     * P057; J5-071.
     * @par ID SWU-CLI-042
     */
    void put_key(text_writer& out, std::string_view name) noexcept
    {
      out.put("\"");
      out.put(name);
      out.put("\": ");
    }
  } // namespace

  /**
   * @brief Write a string as a strict-JSON value.
   * @par Requirements
   * P057; J5-071.
   * @par ID SWU-CLI-033
   */
  void put_json_text(text_writer& out, std::string_view text) noexcept
  {
    if (!is_utf8(text))
    {
      out.put("{\"$bytes\": [");
      for (std::size_t index = 0U; index < text.size(); ++index)
      {
        out.put((index == 0U) ? "" : ", ");
        out.put_count(byte_at(text, index));
      }
      out.put("]}");
      return;
    }
    out.put("\"");
    std::size_t index = 0U;
    while (index < text.size())
    {
      const std::uint32_t value = byte_at(text, index);
      if (value < 0x80U)
      {
        put_escaped(out, text[index], value);
        ++index;
        continue;
      }
      const std::size_t length = utf8_length(text, index);
      out.put(text.substr(index, length));
      index += length;
    }
    out.put("\"");
  }

  /**
   * @brief Stable JSON name of a category.
   * @par Requirements
   * P057; J5-071.
   * @par ID SWU-CLI-037
   */
  std::string_view category_name(category kind) noexcept
  {
    constexpr std::array<std::string_view, 14> c_names{{"io", "syntax", "encoding", "duplicate", "security", "limit",
                                                        "allocation", "internal", "string", "schema", "assertion",
                                                        "instance", "unsupported", "mismatch"}};
    const auto index = static_cast<std::size_t>(kind);
    return (index < c_names.size()) ? c_names[index] : std::string_view{"internal"};
  }

  /**
   * @brief Start a report.
   * @par Requirements
   * P057; J5-071.
   * @par ID SWU-CLI-034
   */
  reporter::reporter(const output_sink& sink, diagnostics_format format, std::string_view command,
                     std::string_view input) noexcept
      : m_out(sink, output_stream::out), m_err(sink, output_stream::err), m_format(format), m_input(input)
  {
    if (m_format == diagnostics_format::json)
    {
      m_out.put("{\"format\": \"adaptive-json5-diagnostics\", \"version\": ");
      m_out.put_count(c_format_version);
      m_out.put(", \"command\": ");
      put_json_text(m_out, command);
      m_out.put(", \"input\": ");
      put_json_text(m_out, input);
      m_out.put(", \"diagnostics\": [");
    }
  }

  /**
   * @brief Write one diagnostic as readable text.
   * @par Requirements
   * P057; J5-071.
   * @par ID SWU-CLI-058
   */
  void reporter::add_text(const diagnostic& item) noexcept
  {
    const bool operational = (item.kind == category::io) || (item.kind == category::allocation) ||
                             (item.kind == category::unsupported) || (item.kind == category::internal);
    if (operational)
    {
      m_err.put("error: ");
      m_err.put(item.message);
      m_err.put(m_input.empty() ? "" : ": ");
      m_err.put(m_input);
      m_err.put("\n");
      return;
    }
    m_out.put(m_input);
    m_out.put(": ");
    m_out.put(item.message);
    if (!item.keyword.empty())
    {
      m_out.put(" (");
      m_out.put(item.keyword);
      m_out.put(" at \"");
      m_out.put(item.instance_pointer);
      m_out.put("\", schema ");
      m_out.put(item.schema_pointer);
      m_out.put(")");
    }
    m_out.put("\n");
  }

  /**
   * @brief Write the optional location and schema fields of a JSON diagnostic.
   * @par Requirements
   * P057; J5-064; J5-071.
   * @par ID SWU-CLI-059
   */
  void reporter::put_details(const diagnostic& item) noexcept
  {
    if (item.position.has_value())
    {
      m_out.put(", ");
      put_key(m_out, "line");
      m_out.put_count(item.position->line);
      m_out.put(", ");
      put_key(m_out, "column");
      m_out.put_count(item.position->column);
    }
    if (item.byte_offset.has_value())
    {
      m_out.put(", ");
      put_key(m_out, "byte_offset");
      m_out.put_count(*item.byte_offset);
    }
    if (!item.keyword.empty())
    {
      m_out.put(", ");
      put_key(m_out, "keyword");
      put_json_text(m_out, item.keyword);
      m_out.put(", ");
      put_key(m_out, "instance_pointer");
      put_json_text(m_out, item.instance_pointer);
      m_out.put(", ");
      put_key(m_out, "schema_pointer");
      put_json_text(m_out, item.schema_pointer);
    }
  }

  /**
   * @brief Add one diagnostic.
   * @par Requirements
   * P057; J5-071.
   * @par ID SWU-CLI-035
   */
  void reporter::add(const diagnostic& item) noexcept
  {
    if (m_format == diagnostics_format::text)
    {
      add_text(item);
      return;
    }
    m_out.put((m_count == 0U) ? "{" : ", {");
    put_key(m_out, "severity");
    m_out.put((item.level == severity::error) ? "\"error\"" : "\"warning\"");
    m_out.put(", ");
    put_key(m_out, "category");
    m_out.put("\"");
    m_out.put(category_name(item.kind));
    m_out.put("\", ");
    put_key(m_out, "message");
    put_json_text(m_out, item.message);
    put_details(item);
    m_out.put("}");
    ++m_count;
  }

  /**
   * @brief Finish the report and return the exit code.
   * @par Requirements
   * P057; J5-070; J5-071.
   * @par ID SWU-CLI-036
   */
  std::int32_t reporter::finish(outcome result, std::int32_t exit) noexcept
  {
    if (m_format == diagnostics_format::json)
    {
      constexpr std::array<std::string_view, 3> c_results{{"valid", "invalid", "error"}};
      m_out.put("], \"result\": \"");
      m_out.put(c_results[static_cast<std::size_t>(result)]);
      m_out.put("\", \"exit\": ");
      m_out.put_count(static_cast<std::size_t>(exit));
      m_out.put("}\n");
    }
    else if (result == outcome::valid)
    {
      m_out.put(m_input);
      m_out.put(": valid\n");
    }
    else
    {
      // Diagnostics were already written.
    }
    return (m_out.ok() && m_err.ok()) ? exit : exit_failure;
  }
} // namespace adaptive_json5::cli
