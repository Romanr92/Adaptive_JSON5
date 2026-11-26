#include "cli/validate_command.hpp"

#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/text_position.hpp"
#include "cli/cli_app.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"
#include "cli/cli_version.hpp"
#include "cli/host_buffer.hpp"
#include "cli/report.hpp"

#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
  #include "adaptive_json5/schema.hpp"
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  namespace
  {
    constexpr std::size_t c_message_capacity = 512U;
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    constexpr std::size_t c_pointer_capacity = 1024U;
    constexpr std::size_t c_registry_bytes = 1U << 26U;
#endif
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    constexpr bool c_exceptions_enabled = true;
#else
    constexpr bool c_exceptions_enabled = false;
#endif
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    constexpr bool c_schema_enabled = true;
#else
    constexpr bool c_schema_enabled = false;
#endif

    /**
     * @brief Line and column of byte_offset in text, if it can be located.
     * @par Requirements
     * P057; J5-044; J5-071.
     * @par ID SWU-CLI-048
     */
    std::optional<text_position> position_of(input_view text, std::size_t byte_offset) noexcept
    {
      const auto located = locate(text, byte_offset);
      return located.has_value() ? std::optional<text_position>{*located.value_if()} : std::nullopt;
    }

    /**
     * @brief Add a parser failure diagnostic and return its category.
     * @par Requirements
     * P057; J5-044; J5-070; J5-071.
     * @par ID SWU-CLI-049
     */
    category add_parse_failure(reporter& report, const error& failure, input_view text) noexcept
    {
      std::array<char, c_message_capacity> message{};
      const auto formatted = format_error(failure, text, message.data(), message.size());
      const std::size_t length = formatted.has_value()
                                   ? ((*formatted.value_if() < message.size()) ? *formatted.value_if() : message.size())
                                   : 0U;
      diagnostic item{};
      item.kind = category_of(failure.code());
      item.message = std::string_view{message.data(), length};
      if (failure.has_byte_offset())
      {
        item.byte_offset = failure.byte_offset();
        item.position = position_of(text, failure.byte_offset());
      }
      report.add(item);
      return item.kind;
    }

    /**
     * @brief Report the string warnings of a parsed document.
     * @par Requirements
     * P057; J5-045; J5-071.
     * @par ID SWU-CLI-050
     */
    void report_warnings(reporter& report, const parsed_document& parsed, input_view text) noexcept
    {
      for (std::size_t index = 0U; index < parsed.warnings.size(); ++index)
      {
        const auto warning = parsed.warnings.at(index);
        if (!warning.has_value())
        {
          continue;
        }
        std::array<char, c_message_capacity> message{};
        const auto formatted = format_warning(*warning.value_if(), text, message.data(), message.size());
        const std::size_t length =
          formatted.has_value() ? ((*formatted.value_if() < message.size()) ? *formatted.value_if() : message.size())
                                : 0U;
        diagnostic item{};
        item.level = severity::warning;
        item.kind = category::string;
        item.message = std::string_view{message.data(), length};
        item.byte_offset = warning.value_if()->byte_offset;
        item.position = position_of(text, warning.value_if()->byte_offset);
        report.add(item);
      }
    }

#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    /**
     * @brief Text of a schema error code.
     * @par Requirements
     * P057; J5-063; J5-071.
     * @par ID SWU-CLI-051
     */
    std::string_view describe_schema(schema_error_code code) noexcept
    {
      constexpr std::array<std::string_view, 12> c_texts{
        {"schema error", "invalid schema", "unresolved schema reference", "unsupported schema dialect",
         "unsupported schema option", "instance outside the schema domain", "nonprogressing schema reference",
         "validation cancelled", "schema resource limit exceeded", "out of memory", "internal schema error",
         "invalid schema registration"}};
      const auto index = static_cast<std::size_t>(code);
      return (index < c_texts.size()) ? c_texts[index] : c_texts[0U];
    }

    /**
     * @brief Category and exit code of a schema failure.
     * @par Requirements
     * P057; J5-063; J5-070.
     * @par ID SWU-CLI-052
     */
    category schema_category(schema_error_code code) noexcept
    {
      switch (code)
      {
      case schema_error_code::unsupported_instance:
        return category::instance;
      case schema_error_code::resource_limit:
      case schema_error_code::cancelled:
        return category::limit;
      case schema_error_code::allocation_failure:
        return category::allocation;
      case schema_error_code::internal_error:
        return category::internal;
      case schema_error_code::invalid_schema:
      case schema_error_code::unresolved_reference:
      case schema_error_code::unsupported_dialect:
      case schema_error_code::unsupported_option:
      case schema_error_code::nonprogressing_reference:
      case schema_error_code::invalid_registration:
      default:
        return category::schema;
      }
    }

    /**
     * @brief Report a schema failure (registration, compilation or validation) and finish.
     * @par Requirements
     * P057; J5-063; J5-070; J5-071.
     * @param text The text the error's range refers to (the schema, or the instance).
     * @par ID SWU-CLI-053
     */
    std::int32_t report_schema_failure(reporter& report, const schema_error& failure, input_view text) noexcept
    {
      diagnostic item{};
      item.kind = schema_category(failure.code());
      item.message = describe_schema(failure.code());
      const auto range = failure.source_range();
      if (range.has_value())
      {
        item.byte_offset = range->byte_offset;
        item.position = position_of(text, range->byte_offset);
      }
      report.add(item);
      return (item.kind == category::instance) ? report.finish(outcome::invalid, exit_invalid)
                                               : report.finish(outcome::error, exit_failure);
    }

    /**
     * @brief Report one failed assertion with keyword, pointers and instance position.
     * @par Requirements
     * P057; J5-064; J5-071.
     * @par ID SWU-CLI-054
     */
    bool report_assertion(reporter& report, const compiled_schema& schema, const document& instance,
                          const schema_diagnostic& failed, input_view text, const environment& host) noexcept
    {
      std::array<char, c_pointer_capacity> schema_pointer{};
      std::array<char, c_pointer_capacity> instance_pointer{};
      const std::size_t schema_size =
        format_schema_location(schema, failed, schema_pointer.data(), schema_pointer.size());
      const auto instance_size =
        format_instance_location(instance, failed, instance_pointer.data(), instance_pointer.size(), host.storage);
      if (!instance_size.has_value())
      {
        return false;
      }
      const std::string_view name = keyword_name(failed.keyword);
      diagnostic item{};
      item.kind = category::assertion;
      item.message = "schema assertion failed";
      item.keyword = name.empty() ? std::string_view{"false"} : name;
      item.schema_pointer = std::string_view{
        schema_pointer.data(), (schema_size < schema_pointer.size()) ? schema_size : schema_pointer.size()};
      item.instance_pointer = std::string_view{
        instance_pointer.data(),
        (*instance_size.value_if() < instance_pointer.size()) ? *instance_size.value_if() : instance_pointer.size()};
      if (failed.has_instance_span)
      {
        item.byte_offset = failed.instance_span.byte_offset;
        item.position = position_of(text, failed.instance_span.byte_offset);
      }
      report.add(item);
      return true;
    }

    /**
     * @brief Validate a parsed instance against the schema file and finish the report.
     * @par Requirements
     * P057; J5-063; J5-064; J5-070.
     * @par ID SWU-CLI-055
     */
    std::int32_t validate_with_schema(const options& selected, const environment& host, reporter& report,
                                      const document& instance, input_view text) noexcept
    {
      host_buffer schema_text{host.storage, selected.limits.max_input_bytes};
      std::size_t size = 0U;
      const io_status read = (schema_text.data() == nullptr)
                               ? io_status::read_error
                               : read_input(host, selected.schema, schema_text.data(), schema_text.size(), size);
      if (schema_text.data() == nullptr)
      {
        report.add(diagnostic{severity::error, category::allocation, "out of memory"});
        return report.finish(outcome::error, exit_failure);
      }
      if (read != io_status::ok)
      {
        report.add(diagnostic{severity::error, category::io, describe_io(read)});
        return report.finish(outcome::error, exit_failure);
      }
      const input_view schema_input{schema_text.data(), size};
      accounted_storage registry_storage{host.storage, c_registry_bytes};
      auto registry = schema_registry::create(registry_storage, 1U);
      if (!registry.has_value())
      {
        return report_schema_failure(report, *registry.error_if(), schema_input);
      }
      const auto added = registry.value_if()->add(schema_input);
      if (!added.has_value())
      {
        return report_schema_failure(report, *added.error_if(), schema_input);
      }
      compile_options compile{};
      compile.result_storage = host.storage;
      compile.scratch_storage = host.storage;
      const auto compiled =
        compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, compile);
      if (!compiled.has_value())
      {
        return report_schema_failure(report, *compiled.error_if(), schema_input);
      }
      validation_options run{};
      run.work_storage = host.storage;
      run.report_storage = host.storage;
      run.mode = evaluation_mode::collect_all;
      const auto verdict = validate(*compiled.value_if(), instance, run);
      if (!verdict.has_value())
      {
        return report_schema_failure(report, *verdict.error_if(), text);
      }
      for (std::size_t index = 0U; index < verdict.value_if()->diagnostic_count(); ++index)
      {
        if (!report_assertion(report, *compiled.value_if(), instance, *verdict.value_if()->diagnostic(index), text,
                              host))
        {
          report.add(diagnostic{severity::error, category::allocation, "out of memory"});
          return report.finish(outcome::error, exit_failure);
        }
      }
      return verdict.value_if()->valid() ? report.finish(outcome::valid, exit_ok)
                                         : report.finish(outcome::invalid, exit_invalid);
    }
#endif
  } // namespace

  /**
   * @brief Report a parser failure and finish the report.
   * @par Requirements
   * P057; P058; J5-044; J5-070; J5-071.
   * @par ID SWU-CLI-076
   */
  std::int32_t finish_with_parse_failure(reporter& report, const error& failure, input_view text) noexcept
  {
    const category kind = add_parse_failure(report, failure, text);
    const bool invalid = (kind == category::syntax) || (kind == category::encoding) || (kind == category::duplicate) ||
                         (kind == category::security);
    return invalid ? report.finish(outcome::invalid, exit_invalid) : report.finish(outcome::error, exit_failure);
  }

  /**
   * @brief Report a parser failure of a configuration file (schema, manifest) and finish with
   * exit 2: such files are configuration, not the validated input.
   * @par Requirements
   * P058; J5-070.
   * @par ID SWU-CLI-105
   */
  std::int32_t finish_with_configuration_failure(reporter& report, const error& failure, input_view text) noexcept
  {
    static_cast<void>(add_parse_failure(report, failure, text));
    return report.finish(outcome::error, exit_failure);
  }

  /**
   * @brief Diagnostic category of a parser failure.
   * @par Requirements
   * P057; J5-070; J5-071.
   * @par ID SWU-CLI-046
   */
  category category_of(error_code code) noexcept
  {
    switch (code)
    {
    case error_code::syntax:
      return category::syntax;
    case error_code::encoding:
      return category::encoding;
    case error_code::duplicate_policy:
      return category::duplicate;
    case error_code::security_policy:
      return category::security;
    case error_code::resource_limit:
      return category::limit;
    case error_code::allocation_failure:
      return category::allocation;
    case error_code::wrong_type_or_access:
    case error_code::numeric_conversion:
    case error_code::internal_error:
    default:
      return category::internal;
    }
  }

  /**
   * @brief Read path into buffer through the host reader.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-047
   */
  io_status read_input(const environment& host, std::string_view path, char* buffer, std::size_t capacity,
                       std::size_t& size) noexcept
  {
    size = 0U;
    return (host.input.read != nullptr) ? host.input.read(host.input.context, path, buffer, capacity, size)
                                        : io_status::read_error;
  }

  /**
   * @brief Text of a reader failure.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-020
   */
  std::string_view describe_io(io_status status) noexcept
  {
    switch (status)
    {
    case io_status::not_found:
      return "cannot open input";
    case io_status::too_large:
      return "input exceeds --max-input-bytes";
    case io_status::read_error:
    case io_status::ok:
    default:
      return "cannot read input";
    }
  }

  /**
   * @brief The validate command.
   * @par Requirements
   * P056; P057; J5-070; J5-071.
   * @par ID SWU-CLI-044
   */
  std::int32_t run_validate(const options& selected, const environment& host) noexcept
  {
    reporter report{host.output, selected.diagnostics, "validate", selected.input};
#if !defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    if (!selected.schema.empty())
    {
      report.add(
        diagnostic{severity::error, category::unsupported, "schema validation is not available in this build"});
      return report.finish(outcome::error, exit_failure);
    }
#endif
    host_buffer input{host.storage, selected.limits.max_input_bytes};
    if (input.data() == nullptr)
    {
      report.add(diagnostic{severity::error, category::allocation, "out of memory"});
      return report.finish(outcome::error, exit_failure);
    }
    std::size_t size = 0U;
    const io_status read = read_input(host, selected.input, input.data(), input.size(), size);
    if (read != io_status::ok)
    {
      report.add(diagnostic{severity::error, category::io, describe_io(read)});
      return report.finish(outcome::error, exit_failure);
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
      return finish_with_parse_failure(report, *parsed.error_if(), text);
    }
    report_warnings(report, *parsed.value_if(), text);
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    if (!selected.schema.empty())
    {
      return validate_with_schema(selected, host, report, parsed.value_if()->values, text);
    }
#endif
    return report.finish(outcome::valid, exit_ok);
  }

  /**
   * @brief The capabilities command.
   * @par Requirements
   * P057; P059; J5-069; J5-071.
   * @par ID SWU-CLI-045
   */
  std::int32_t run_capabilities(const environment& host) noexcept
  {
    text_writer out{host.output, output_stream::out};
    out.put("{\"format\": \"adaptive-json5-capabilities\", \"version\": ");
    out.put_count(c_format_version);
    out.put(", \"tool_version\": \"");
    out.put(c_tool_version);
    out.put("\", \"library_version\": \"");
    out.put(c_library_version);
    out.put("\", \"source_revision\": \"");
    out.put(c_source_revision);
    out.put("\"");
    out.put(", \"exceptions\": ");
    out.put(c_exceptions_enabled ? "true" : "false");
    out.put(", \"commands\": [\"validate\", \"inspect\", \"run\", \"capabilities\", \"version\", \"help\"]");
    out.put(", \"syntax\": [\"json\", \"json5\"], \"diagnostics\": [\"text\", \"json\"]");
#if defined(ADAPTIVE_JSON5_CLI_HAS_SCHEMA)
    out.put(", \"schema\": {\"available\": true, \"dialects\": [\"draft-07\"], \"format_assertions\": [");
    constexpr std::array<std::string_view, 12> c_formats{{"date-time", "date", "time", "email", "hostname", "ipv4",
                                                          "ipv6", "uri", "uri-reference", "json-pointer",
                                                          "relative-json-pointer", "regex"}};
    bool first = true;
    for (const std::string_view name : c_formats)
    {
      if (format_assertion_supported(name))
      {
        out.put(first ? "\"" : ", \"");
        out.put(name);
        out.put("\"");
        first = false;
      }
    }
    out.put("], \"content_assertions\": false}");
#else
    out.put(", \"schema\": {\"available\": false}");
#endif
    out.put("}\n");
    return out.ok() ? exit_ok : exit_failure;
  }

  /**
   * @brief The version command: tool and library versions, source revision and build options.
   * @par Requirements
   * P059; J5-093.
   * @par ID SWU-CLI-115
   */
  std::int32_t run_version(const environment& host) noexcept
  {
    text_writer out{host.output, output_stream::out};
    out.put("adaptive_json5_examples ");
    out.put(c_tool_version);
    out.put("\n");
    out.put("library ");
    out.put(c_library_version);
    out.put("\n");
    out.put("revision ");
    out.put(c_source_revision);
    out.put("\n");
    out.put("schema ");
    out.put(c_schema_enabled ? "on" : "off");
    out.put("\n");
    out.put("exceptions ");
    out.put(c_exceptions_enabled ? "on" : "off");
    out.put("\n");
    return out.ok() ? exit_ok : exit_failure;
  }
} // namespace adaptive_json5::cli
