#include "cli/cli_app.hpp"

#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"
#include "cli/inspect_command.hpp"
#include "cli/run_command.hpp"
#include "cli/validate_command.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  /**
   * @brief Bind the writer to a sink and stream.
   * @par Requirements
   * P056.
   * @par ID SWU-CLI-010
   */
  text_writer::text_writer(const output_sink& sink, output_stream stream) noexcept : m_sink(&sink), m_stream(stream) {}

  /**
   * @brief Write text; ignored after a failed write.
   * @par Requirements
   * P056; J5-071.
   * @par ID SWU-CLI-011
   */
  text_writer& text_writer::put(std::string_view text) noexcept
  {
    if (m_ok && !text.empty())
    {
      m_ok = (m_sink->write != nullptr) && m_sink->write(m_sink->context, m_stream, text);
    }
    return *this;
  }

  /**
   * @brief Write an unsigned decimal number.
   * @par Requirements
   * P056; J5-071.
   * @par ID SWU-CLI-012
   */
  text_writer& text_writer::put_count(std::size_t value) noexcept
  {
    constexpr std::string_view c_digits{"0123456789"};
    std::array<char, 24> digits{};
    std::size_t first = digits.size();
    std::size_t rest = value;
    do
    {
      --first;
      digits[first] = c_digits[rest % 10U];
      rest /= 10U;
    } while (rest != 0U);
    return put(std::string_view{digits.data() + first, digits.size() - first});
  }

  /**
   * @brief Return false when any write failed.
   * @par Requirements
   * P056; J5-071.
   * @par ID SWU-CLI-013
   */
  bool text_writer::ok() const noexcept
  {
    return m_ok;
  }

  namespace
  {
    constexpr std::string_view c_usage =
      "usage: adaptive_json5_examples <command> [options] [input]\n"
      "commands:\n"
      "  validate --syntax json|json5 [--duplicates preserve|reject] [--schema PATH] INPUT\n"
      "  inspect  --syntax json|json5 [--duplicates preserve|reject] INPUT\n"
      "  run      --manifest PATH\n"
      "  capabilities | version | help\n"
      "common options: --diagnostics text|json, --max-input-bytes N, --max-depth N,\n"
      "  --max-nodes N, --max-members N, --max-string-units N\n"
      "INPUT is a path, or - for standard input.\n"
      "exit: 0 valid, 1 invalid input or assertion, 2 configuration, I/O or operational failure\n";

    /**
     * @brief Report an argument error with usage text; exit 2.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-018
     */
    int report_arguments(const environment& host, const argument_error& failure) noexcept
    {
      text_writer err{host.output, output_stream::err};
      err.put("error: ").put(describe(failure.code));
      if (!failure.argument.empty())
      {
        err.put(": ").put(failure.argument);
      }
      err.put("\n").put(c_usage);
      return exit_failure;
    }

    /**
     * @brief Report an operational failure message; exit 2.
     * @par Requirements
     * P056; J5-070.
     * @par ID SWU-CLI-019
     */
    int report_failure(const environment& host, std::string_view message, std::string_view subject) noexcept
    {
      text_writer err{host.output, output_stream::err};
      err.put("error: ").put(message);
      if (!subject.empty())
      {
        err.put(": ").put(subject);
      }
      err.put("\n");
      return exit_failure;
    }

  } // namespace

  /**
   * @brief Run the tool: parse the arguments, dispatch the command and return the exit code.
   * @par Requirements
   * P056; P057; P058; J5-070.
   * @par ID SWU-CLI-014
   */
  int run_cli(int argc, const char* const* argv, const environment& host) noexcept
  {
    const parsed_arguments arguments = parse_arguments(argc, argv);
    if (!arguments.parsed.has_value())
    {
      return report_arguments(host, arguments.failure);
    }
    const options& selected = *arguments.parsed;
    switch (selected.selected)
    {
    case command::validate:
      return run_validate(selected, host);
    case command::help:
    {
      text_writer out{host.output, output_stream::out};
      out.put(c_usage);
      return out.ok() ? exit_ok : exit_failure;
    }
    case command::capabilities:
      return run_capabilities(host);
    case command::inspect:
      return run_inspect(selected, host);
    case command::run:
      return run_manifest(selected, host);
    case command::version:
    default:
      return report_failure(host, "command is not available yet", argv[1]);
    }
  }
} // namespace adaptive_json5::cli
