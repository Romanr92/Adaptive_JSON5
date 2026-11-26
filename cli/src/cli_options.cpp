#include "cli/cli_options.hpp"

#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/options.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  namespace
  {
    constexpr std::string_view c_digits{"0123456789"};
    constexpr std::size_t c_max_decimal_digits = 18U; // Fits std::size_t without overflow.

    /**
     * @brief Option identifiers.
     * @par Requirements
     * P056; J5-070.
     */
    enum class option_id : std::uint8_t
    {
      syntax,
      duplicates,
      schema,
      diagnostics,
      manifest,
      max_input_bytes,
      max_depth,
      max_nodes,
      max_members,
      max_string_units
    };

    /**
     * @brief One known option and the commands that accept it.
     * @par Requirements
     * P056; J5-070.
     */
    struct option_spec
    {
        std::string_view name;
        option_id id;
        bool for_validate;
        bool for_inspect;
        bool for_run;
    };

    constexpr std::array<option_spec, 10> c_options{{
      {"--syntax", option_id::syntax, true, true, false},
      {"--duplicates", option_id::duplicates, true, true, false},
      {"--schema", option_id::schema, true, false, false},
      {"--diagnostics", option_id::diagnostics, true, true, true},
      {"--manifest", option_id::manifest, false, false, true},
      {"--max-input-bytes", option_id::max_input_bytes, true, true, true},
      {"--max-depth", option_id::max_depth, true, true, true},
      {"--max-nodes", option_id::max_nodes, true, true, true},
      {"--max-members", option_id::max_members, true, true, true},
      {"--max-string-units", option_id::max_string_units, true, true, true},
    }};

    /**
     * @brief Return the command named by text, if any.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-003
     */
    std::optional<command> command_named(std::string_view text) noexcept
    {
      constexpr std::array<std::pair<std::string_view, command>, 6> c_commands{{
        {"help", command::help},
        {"version", command::version},
        {"capabilities", command::capabilities},
        {"validate", command::validate},
        {"inspect", command::inspect},
        {"run", command::run},
      }};
      for (const auto& entry : c_commands)
      {
        if (entry.first == text)
        {
          return entry.second;
        }
      }
      if ((text == "--help") || (text == "-h"))
      {
        return command::help;
      }
      if (text == "--version")
      {
        return command::version;
      }
      return std::nullopt;
    }

    /**
     * @brief Return the option spec named by text, if any.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-004
     */
    const option_spec* option_named(std::string_view text) noexcept
    {
      for (const option_spec& spec : c_options)
      {
        if (spec.name == text)
        {
          return &spec;
        }
      }
      return nullptr;
    }

    /**
     * @brief Report whether an option applies to a command.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-005
     */
    bool allowed(const option_spec& spec, command selected) noexcept
    {
      return ((selected == command::validate) && spec.for_validate) ||
             ((selected == command::inspect) && spec.for_inspect) || ((selected == command::run) && spec.for_run);
    }

    /**
     * @brief Parse a positive decimal count; nullopt for anything else (including 0).
     * @par Requirements
     * P056; J5-047; J5-070.
     * @par ID SWU-CLI-006
     */
    std::optional<std::size_t> positive_count(std::string_view text) noexcept
    {
      if (text.empty() || (text.size() > c_max_decimal_digits) ||
          (text.find_first_not_of(c_digits) != std::string_view::npos))
      {
        return std::nullopt;
      }
      std::size_t value = 0U;
      for (const char c : text)
      {
        value = (value * 10U) + c_digits.find(c);
      }
      return (value == 0U) ? std::nullopt : std::optional<std::size_t>{value};
    }

    /**
     * @brief Apply one --max-* limit; false when the value is not a positive count.
     * @par Requirements
     * P056; J5-047; J5-070.
     * @par ID SWU-CLI-030
     */
    bool apply_limit(resource_limits& limits, option_id id, std::string_view value) noexcept
    {
      const auto count = positive_count(value);
      const std::size_t applied = count.value_or(0U);
      switch (id)
      {
      case option_id::max_input_bytes:
        limits.max_input_bytes = applied;
        break;
      case option_id::max_depth:
        limits.max_depth = applied;
        break;
      case option_id::max_nodes:
        limits.max_nodes = applied;
        break;
      case option_id::max_members:
        limits.max_members = applied;
        break;
      case option_id::max_string_units:
      case option_id::syntax:
      case option_id::duplicates:
      case option_id::schema:
      case option_id::diagnostics:
      case option_id::manifest:
      default:
        limits.max_string_units = applied;
        break;
      }
      return count.has_value();
    }

    /**
     * @brief Apply --syntax, --duplicates or --diagnostics; false for an unknown choice.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-032
     */
    bool apply_choice(options& target, option_id id, std::string_view value) noexcept
    {
      if (id == option_id::syntax)
      {
        target.syntax = (value == "json5")  ? std::optional<syntax_mode>{syntax_mode::json5}
                        : (value == "json") ? std::optional<syntax_mode>{syntax_mode::strict_json}
                                            : std::nullopt;
        return target.syntax.has_value();
      }
      if (id == option_id::duplicates)
      {
        target.duplicates =
          (value == "reject") ? duplicate_name_policy::reject_duplicates : duplicate_name_policy::preserve_all;
        return (value == "reject") || (value == "preserve");
      }
      target.diagnostics = (value == "json") ? diagnostics_format::json : diagnostics_format::text;
      return (value == "json") || (value == "text");
    }

    /**
     * @brief Apply one option value; false when the value is invalid.
     * @par Requirements
     * P056; J5-047; J5-070.
     * @par ID SWU-CLI-007
     */
    bool apply(options& target, option_id id, std::string_view value) noexcept
    {
      switch (id)
      {
      case option_id::syntax:
      case option_id::duplicates:
      case option_id::diagnostics:
        return apply_choice(target, id, value);
      case option_id::schema:
        target.schema = value;
        return !value.empty();
      case option_id::manifest:
        target.manifest = value;
        return !value.empty();
      case option_id::max_input_bytes:
      case option_id::max_depth:
      case option_id::max_nodes:
      case option_id::max_members:
      case option_id::max_string_units:
      default:
        return apply_limit(target.limits, id, value);
      }
    }

    /**
     * @brief Build a failed parse result.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-008
     */
    parsed_arguments failed(argument_error_code code, std::string_view argument) noexcept
    {
      parsed_arguments result{};
      result.failure = argument_error{code, argument};
      return result;
    }

    /**
     * @brief Check and apply one option at argv[index] with its value at argv[index + 1].
     * @par Requirements
     * P056; J5-070.
     * @return The argument error, or nullopt when the option was applied.
     * @par ID SWU-CLI-031
     */
    std::optional<argument_error> take_option(options& parsed, std::array<bool, c_options.size()>& seen,
                                              const option_spec& spec, std::int32_t argc, const char* const* argv,
                                              std::int32_t index) noexcept
    {
      const std::string_view argument{argv[index]};
      const auto slot = static_cast<std::size_t>(spec.id);
      if (!allowed(spec, parsed.selected))
      {
        return argument_error{argument_error_code::option_not_allowed, argument};
      }
      if (seen[slot])
      {
        return argument_error{argument_error_code::repeated_option, argument};
      }
      if ((index + 1) >= argc)
      {
        return argument_error{argument_error_code::missing_value, argument};
      }
      seen[slot] = true;
      const std::string_view value{argv[index + 1]};
      if (!apply(parsed, spec.id, value))
      {
        return argument_error{argument_error_code::invalid_value, value};
      }
      return std::nullopt;
    }

    /**
     * @brief Check the per-command requirements after all arguments were read.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-009
     */
    parsed_arguments finish(const options& parsed) noexcept
    {
      const bool needs_input = (parsed.selected == command::validate) || (parsed.selected == command::inspect);
      if (needs_input && parsed.input.empty())
      {
        return failed(argument_error_code::missing_input, {});
      }
      if (needs_input && !parsed.syntax.has_value())
      {
        return failed(argument_error_code::missing_syntax, "--syntax");
      }
      if ((parsed.selected == command::run) && parsed.manifest.empty())
      {
        return failed(argument_error_code::missing_value, "--manifest");
      }
      parsed_arguments result{};
      result.parsed = parsed;
      return result;
    }
  } // namespace

  /**
   * @brief Parse argv (argv[0] is the program name) without allocating.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-001
   */
  parsed_arguments parse_arguments(std::int32_t argc, const char* const* argv) noexcept
  {
    if ((argc < 2) || (argv == nullptr))
    {
      return failed(argument_error_code::missing_command, {});
    }
    const std::string_view name{argv[1]};
    const auto selected = command_named(name);
    if (!selected.has_value())
    {
      return failed(argument_error_code::unknown_command, name);
    }
    options parsed{};
    parsed.selected = *selected;
    std::array<bool, c_options.size()> seen{};
    std::int32_t index = 2;
    while (index < argc)
    {
      const std::string_view argument{argv[index]};
      const option_spec* spec = option_named(argument);
      const bool looks_like_option = (argument.size() > 1U) && (argument.substr(0U, 2U) == "--");
      if ((spec == nullptr) && looks_like_option)
      {
        return failed(argument_error_code::unknown_option, argument);
      }
      if (spec == nullptr)
      {
        if (!parsed.input.empty() || (parsed.selected != command::validate && parsed.selected != command::inspect))
        {
          return failed(argument_error_code::extra_argument, argument);
        }
        parsed.input = argument;
        ++index;
        continue;
      }
      const auto taken = take_option(parsed, seen, *spec, argc, argv, index);
      if (taken.has_value())
      {
        return failed(taken->code, taken->argument);
      }
      index += 2;
    }
    return finish(parsed);
  }

  /**
   * @brief Short human-readable text of an argument error code.
   * @par Requirements
   * P056; J5-070.
   * @par ID SWU-CLI-002
   */
  std::string_view describe(argument_error_code code) noexcept
  {
    constexpr std::array<std::string_view, 10> c_texts{{
      "missing command",
      "unknown command",
      "unknown option",
      "missing option value",
      "invalid option value",
      "option given twice",
      "option does not apply to this command",
      "missing input (a path, or - for standard input)",
      "unexpected argument",
      "missing --syntax json|json5",
    }};
    const auto index = static_cast<std::size_t>(code);
    return (index < c_texts.size()) ? c_texts[index] : std::string_view{"invalid arguments"};
  }
} // namespace adaptive_json5::cli
