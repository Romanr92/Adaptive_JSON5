#include "cli/inspect_command.hpp"

#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/text_position.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/integer_conversion.hpp"
#include "adaptive_json5/value/number_kind.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_iterator.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"
#include "cli/cli_app.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"
#include "cli/host_buffer.hpp"
#include "cli/report.hpp"
#include "cli/validate_command.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>
#include <type_traits>

namespace adaptive_json5::cli
{
  namespace
  {
    /**
     * @brief One open container of the iterative walk.
     * @par Requirements
     * P058; J5-048 (no recursion).
     */
    struct walk_frame
    {
        value_iterator children{};
        bool object{};
        std::size_t count{};
    };
    static_assert(std::is_trivially_copyable<walk_frame>::value, "frames are copied as bytes");

    /**
     * @brief Name of a value kind.
     * @par Requirements
     * P058; J5-071.
     * @par ID SWU-CLI-062
     */
    std::string_view kind_name(value_kind kind) noexcept
    {
      switch (kind)
      {
      case value_kind::null:
        return "null";
      case value_kind::boolean:
        return "boolean";
      case value_kind::number:
        return "number";
      case value_kind::string:
        return "string";
      case value_kind::array:
        return "array";
      case value_kind::object:
      default:
        return "object";
      }
    }

    /**
     * @brief Name of a number spelling kind.
     * @par Requirements
     * P058; J5-021.
     * @par ID SWU-CLI-063
     */
    std::string_view number_kind_name(number_kind kind) noexcept
    {
      switch (kind)
      {
      case number_kind::hexadecimal:
        return "hexadecimal";
      case number_kind::infinity:
        return "infinity";
      case number_kind::nan:
        return "nan";
      case number_kind::decimal:
      default:
        return "decimal";
      }
    }

    /**
     * @brief Name of a numeric conversion failure.
     * @par Requirements
     * P058; J5-034; J5-071.
     * @par ID SWU-CLI-064
     */
    std::string_view conversion_failure(const error& failure) noexcept
    {
      const auto reason = failure.numeric_reason();
      if (!reason.has_value())
      {
        return "failed";
      }
      switch (*reason)
      {
      case numeric_conversion_reason::non_integral:
        return "non_integral";
      case numeric_conversion_reason::out_of_range:
        return "out_of_range";
      case numeric_conversion_reason::non_finite:
        return "non_finite";
      case numeric_conversion_reason::underflow:
        return "underflow";
      case numeric_conversion_reason::unsupported_destination:
      default:
        return "unsupported_destination";
      }
    }

    /**
     * @brief Write a signed 64-bit value.
     * @par Requirements
     * P058; J5-071.
     * @par ID SWU-CLI-065
     */
    void put_signed(text_writer& out, std::int64_t value) noexcept
    {
      if (value < 0)
      {
        out.put("-").put_count(static_cast<std::size_t>(0U - static_cast<std::uint64_t>(value)));
        return;
      }
      out.put_count(static_cast<std::size_t>(value));
    }

    /**
     * @brief Write a double: shortest round-trip decimal, or {"$number": ...} when non-finite.
     * @par Requirements
     * P058; J5-071; ADR 0007 D2.
     * @par ID SWU-CLI-066
     */
    void put_double(text_writer& out, double value) noexcept
    {
      if (std::isnan(value))
      {
        out.put("{\"$number\": \"NaN\"}");
        return;
      }
      if (std::isinf(value))
      {
        out.put((value < 0.0) ? "{\"$number\": \"-Infinity\"}" : "{\"$number\": \"Infinity\"}");
        return;
      }
      std::array<char, 40> text{};
      const int written = std::snprintf(text.data(), text.size(), "%.17g", value);
      out.put(std::string_view{text.data(), (written > 0) ? static_cast<std::size_t>(written) : 0U});
    }

    /**
     * @brief Writes the inspection of one document, in text or JSON.
     * @par Requirements
     * P058; J5-041; J5-071.
     */
    class inspection
    {
      private:
        text_writer& m_out;
        diagnostics_format m_format;
        input_view m_text;
        const storage_callbacks& m_scratch;
        bool m_storage_ok{true};

      public:
        inspection(text_writer& out, diagnostics_format format, input_view text,
                   const storage_callbacks& scratch) noexcept
            : m_out(out), m_format(format), m_text(text), m_scratch(scratch)
        {
        }

        /**
         * @brief Report whether every scratch allocation succeeded.
         * @par Requirements
         * P058; J5-046.
         * @par ID SWU-CLI-067
         */
        [[nodiscard]] bool storage_ok() const noexcept
        {
          return m_storage_ok;
        }

        /**
         * @brief Write a value's position ("line"/"column" fields or "(line L, column C)").
         * @par Requirements
         * P058; J5-041.
         * @par ID SWU-CLI-068
         */
        void put_position(const value_view& value) noexcept
        {
          const auto span = value.source_span();
          if (!span.has_value() || !span.value_if()->has_value())
          {
            return;
          }
          const auto located = locate(m_text, (*span.value_if())->byte_offset);
          if (!located.has_value())
          {
            return;
          }
          if (m_format == diagnostics_format::json)
          {
            m_out.put(", \"line\": ").put_count(located.value_if()->line);
            m_out.put(", \"column\": ").put_count(located.value_if()->column);
          }
          else
          {
            m_out.put(" (line ").put_count(located.value_if()->line);
            m_out.put(", column ").put_count(located.value_if()->column).put(")");
          }
        }

        /**
         * @brief Write the checked conversions of a number.
         * @par Requirements
         * P058; J5-034; J5-035.
         * @par ID SWU-CLI-069
         */
        void put_conversions(const owned_number& number) noexcept
        {
          const bool json = m_format == diagnostics_format::json;
          const auto as_int = to_integer<std::int64_t>(number);
          m_out.put(json ? ", \"int64\": " : "; int64 ");
          if (as_int.has_value())
          {
            put_signed(m_out, *as_int.value_if());
          }
          else
          {
            m_out.put(json ? "{\"error\": \"" : "error ")
              .put(conversion_failure(*as_int.error_if()))
              .put(json ? "\"}" : "");
          }
          const auto as_unsigned = to_integer<std::uint64_t>(number);
          m_out.put(json ? ", \"uint64\": " : "; uint64 ");
          if (as_unsigned.has_value())
          {
            m_out.put_count(static_cast<std::size_t>(*as_unsigned.value_if()));
          }
          else
          {
            m_out.put(json ? "{\"error\": \"" : "error ")
              .put(conversion_failure(*as_unsigned.error_if()))
              .put(json ? "\"}" : "");
          }
          const auto as_double = to_double(number, m_scratch);
          m_out.put(json ? ", \"double\": " : "; double ");
          if (as_double.has_value())
          {
            put_double(m_out, *as_double.value_if());
          }
          else
          {
            m_storage_ok = m_storage_ok && (as_double.error_if()->code() != error_code::allocation_failure);
            m_out.put(json ? "{\"error\": \"" : "error ")
              .put(conversion_failure(*as_double.error_if()))
              .put(json ? "\"}" : "");
          }
        }

        /**
         * @brief Write the payload of a scalar value after its kind.
         * @par Requirements
         * P058; J5-021; J5-030; J5-071.
         * @par ID SWU-CLI-070
         */
        void put_scalar(const value_view& value, value_kind kind) noexcept
        {
          const bool json = m_format == diagnostics_format::json;
          if (kind == value_kind::boolean)
          {
            const auto flag = value.as_boolean();
            m_out.put(json ? ", \"value\": " : " ").put((flag.has_value() && *flag.value_if()) ? "true" : "false");
          }
          else if (kind == value_kind::string)
          {
            const auto text = value.as_string();
            m_out.put(json ? ", \"value\": " : " ");
            m_storage_ok = m_storage_ok && text.has_value() && put_json_decoded(m_out, **text.value_if(), m_scratch);
          }
          else if (kind == value_kind::number)
          {
            const auto number = value.as_number();
            if (!number.has_value())
            {
              return;
            }
            const input_view spelling = (*number.value_if())->spelling();
            m_out.put(json ? ", \"spelling\": " : " ");
            put_json_text(m_out, std::string_view{spelling.data, spelling.size});
            m_out.put(json ? ", \"number_kind\": \"" : " (").put(number_kind_name((*number.value_if())->kind()));
            m_out.put(json ? "\"" : "");
            put_conversions(**number.value_if());
            m_out.put(json ? "" : ")");
          }
          else
          {
            // null has no payload.
          }
        }

        /**
         * @brief Write the separator or indentation and, for object members, the name.
         * @par Requirements
         * P058; J5-031.
         * @par ID SWU-CLI-112
         */
        void put_prefix(const value_view& value, bool member, std::size_t depth, bool first) noexcept
        {
          const bool json = m_format == diagnostics_format::json;
          if (json)
          {
            m_out.put(first ? "{" : ", {");
          }
          for (std::size_t level = 0U; !json && (level < depth); ++level)
          {
            m_out.put("  ");
          }
          if (member)
          {
            const auto name = value.member_name();
            m_out.put(json ? "\"name\": " : "");
            m_storage_ok = m_storage_ok && name.has_value() && put_json_decoded(m_out, **name.value_if(), m_scratch);
            m_out.put(json ? ", " : ": ");
          }
        }

        /**
         * @brief Open one value: write its name (object members), kind, position and payload,
         * and return whether it is a container whose children follow.
         * @par Requirements
         * P058; J5-031; J5-041.
         * @par ID SWU-CLI-071
         */
        bool open(const value_view& value, bool member, std::size_t depth, bool first) noexcept
        {
          const auto kind = value.kind();
          const value_kind known = kind.has_value() ? *kind.value_if() : value_kind::null;
          const bool json = m_format == diagnostics_format::json;
          put_prefix(value, member, depth, first);
          m_out.put(json ? "\"kind\": \"" : "").put(kind_name(known)).put(json ? "\"" : "");
          put_position(value);
          const bool container = (known == value_kind::array) || (known == value_kind::object);
          if (!container)
          {
            put_scalar(value, known);
            m_out.put(json ? "}" : "\n");
            return false;
          }
          m_out.put(json ? ((known == value_kind::object) ? ", \"members\": [" : ", \"elements\": [") : "\n");
          return true;
        }

        /**
         * @brief Close a container opened by open().
         * @par Requirements
         * P058.
         * @par ID SWU-CLI-072
         */
        void close() noexcept
        {
          if (m_format == diagnostics_format::json)
          {
            m_out.put("]}");
          }
        }
    };

    /**
     * @brief Copy a frame into or out of the frame stack (trivially copyable bytes).
     * @par Requirements
     * P058; J5-048.
     * @par ID SWU-CLI-073
     */
    void store_frame(char* stack, std::size_t index, const walk_frame& frame) noexcept
    {
      std::memcpy(stack + (index * sizeof(walk_frame)), &frame, sizeof(walk_frame));
    }

    /**
     * @brief Read a frame from the frame stack.
     * @par Requirements
     * P058; J5-048.
     * @par ID SWU-CLI-074
     */
    walk_frame load_frame(const char* stack, std::size_t index) noexcept
    {
      walk_frame frame{};
      std::memcpy(&frame, stack + (index * sizeof(walk_frame)), sizeof(walk_frame));
      return frame;
    }

    /**
     * @brief Walk the document iteratively and write every value; false on storage failure.
     * @par Requirements
     * P058; J5-031; J5-048.
     * @par ID SWU-CLI-075
     */
    bool walk(inspection& writer, const value_view& root, std::size_t max_depth,
              const storage_callbacks& storage) noexcept
    {
      host_buffer stack{storage, (max_depth + 2U) * sizeof(walk_frame)};
      if (stack.data() == nullptr)
      {
        return false;
      }
      std::size_t depth = 0U;
      if (writer.open(root, false, 0U, true))
      {
        const auto children = root.children();
        const auto kind = root.kind();
        store_frame(stack.data(), 0U,
                    walk_frame{children.has_value() ? *children.value_if() : value_iterator{},
                               kind.has_value() && (*kind.value_if() == value_kind::object), 0U});
        depth = 1U;
      }
      while (depth != 0U)
      {
        walk_frame top = load_frame(stack.data(), depth - 1U);
        const auto next = top.children.next();
        if (!next.has_value() || !next.value_if()->has_value())
        {
          writer.close();
          --depth;
          continue;
        }
        const value_view child = **next.value_if();
        const bool container = writer.open(child, top.object, depth, top.count == 0U);
        ++top.count;
        store_frame(stack.data(), depth - 1U, top);
        if (container && (depth <= max_depth))
        {
          const auto children = child.children();
          const auto kind = child.kind();
          store_frame(stack.data(), depth,
                      walk_frame{children.has_value() ? *children.value_if() : value_iterator{},
                                 kind.has_value() && (*kind.value_if() == value_kind::object), 0U});
          ++depth;
        }
      }
      return writer.storage_ok();
    }

    /**
     * @brief Write the string warnings of a document (JSON array items or "warning:" lines).
     * @par Requirements
     * P058; J5-045; J5-071.
     * @par ID SWU-CLI-077
     */
    void put_warnings(text_writer& out, bool json, std::string_view input, const parsed_document& parsed,
                      input_view text) noexcept
    {
      for (std::size_t index = 0U; index < parsed.warnings.size(); ++index)
      {
        const auto warning = parsed.warnings.at(index);
        if (!warning.has_value())
        {
          continue;
        }
        std::array<char, 512> message{};
        const auto formatted = format_warning(*warning.value_if(), text, message.data(), message.size());
        const std::size_t length =
          formatted.has_value() ? ((*formatted.value_if() < message.size()) ? *formatted.value_if() : message.size())
                                : 0U;
        const std::string_view line{message.data(), length};
        if (json)
        {
          out.put((index == 0U) ? "{\"message\": " : ", {\"message\": ");
          put_json_text(out, line);
          out.put(", \"byte_offset\": ").put_count(warning.value_if()->byte_offset).put("}");
        }
        else
        {
          out.put(input).put(": warning: ").put(line).put("\n");
        }
      }
    }

    /**
     * @brief Report an operational failure of inspect in the diagnostic format; exit 2.
     * @par Requirements
     * P058; J5-070; J5-071.
     * @par ID SWU-CLI-078
     */
    int inspect_failure(const options& selected, const environment& host, category kind,
                        std::string_view message) noexcept
    {
      reporter failures{host.output, selected.diagnostics, "inspect", selected.input};
      failures.add(diagnostic{severity::error, kind, message});
      return failures.finish(outcome::error, exit_failure);
    }
  } // namespace

  /**
   * @brief Write a decoded string as a strict-JSON value.
   * @par Requirements
   * P058; J5-071.
   * @par ID SWU-CLI-060
   */
  bool put_json_decoded(text_writer& out, const decoded_string& text, const storage_callbacks& scratch) noexcept
  {
    const auto size = utf8_size(text);
    if (!size.has_value())
    {
      out.put("{\"$utf16\": [");
      for (std::size_t index = 0U; index < text.size(); ++index)
      {
        const auto unit = text.unit_at(index);
        out.put((index == 0U) ? "" : ", ").put_count(unit.has_value() ? *unit.value_if() : 0U);
      }
      out.put("]}");
      return true;
    }
    host_buffer utf8{scratch, *size.value_if() + 1U};
    if (utf8.data() == nullptr)
    {
      return false;
    }
    const auto exported = export_utf8(text, utf8.data(), utf8.size());
    put_json_text(out, std::string_view{utf8.data(), exported.has_value() ? *exported.value_if() : 0U});
    return true;
  }

  /**
   * @brief The inspect command.
   * @par Requirements
   * P058; J5-041; J5-045; J5-070; J5-071.
   * @par ID SWU-CLI-061
   */
  int run_inspect(const options& selected, const environment& host) noexcept
  {
    host_buffer input{host.storage, selected.limits.max_input_bytes};
    if (input.data() == nullptr)
    {
      return inspect_failure(selected, host, category::allocation, "out of memory");
    }
    std::size_t size = 0U;
    const io_status read = read_input(host, selected.input, input.data(), input.size(), size);
    if (read != io_status::ok)
    {
      return inspect_failure(selected, host, category::io, describe_io(read));
    }
    parse_options parse{};
    parse.syntax = *selected.syntax;
    parse.duplicate_names = selected.duplicates;
    parse.limits = selected.limits;
    accounted_storage accounting{host.storage, parse.limits.max_owned_bytes};
    const input_view text{input.data(), size};
    const auto parsed = parse_document(text, parse, accounting);
    if (!parsed.has_value())
    {
      reporter failures{host.output, selected.diagnostics, "inspect", selected.input};
      return finish_with_parse_failure(failures, *parsed.error_if(), text);
    }
    const auto root = parsed.value_if()->values.root();
    text_writer out{host.output, output_stream::out};
    const bool json = selected.diagnostics == diagnostics_format::json;
    if (json)
    {
      out.put("{\"format\": \"adaptive-json5-inspection\", \"version\": ").put_count(c_format_version);
      out.put(", \"input\": ");
      put_json_text(out, selected.input);
      out.put((*selected.syntax == syntax_mode::json5) ? ", \"syntax\": \"json5\"" : ", \"syntax\": \"json\"");
      out.put(", \"warnings\": [");
    }
    put_warnings(out, json, selected.input, *parsed.value_if(), text);
    out.put(json ? "], \"value\": " : "");
    inspection writer{out, selected.diagnostics, text, host.storage};
    const bool walked = root.has_value() && walk(writer, *root.value_if(), selected.limits.max_depth, host.storage);
    if (json)
    {
      out.put(walked ? ", \"result\": \"valid\", \"exit\": 0}\n" : "null, \"result\": \"error\", \"exit\": 2}\n");
    }
    if (!walked)
    {
      text_writer err{host.output, output_stream::err};
      err.put("error: out of memory: ").put(selected.input).put("\n");
      return exit_failure;
    }
    return out.ok() ? exit_ok : exit_failure;
  }
} // namespace adaptive_json5::cli
