#include "cli/cli_app.hpp"

#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"

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
     * @brief A buffer from the host storage, released on destruction.
     * @par Requirements
     * P056; J5-046; ADR 0007.
     */
    class host_buffer
    {
      private:
        storage_callbacks m_storage;
        char* m_data{};
        std::size_t m_size{};

      public:
        /**
         * @brief Allocate size bytes; data() is nullptr when that failed.
         * @par Requirements
         * P056; J5-046.
         * @par ID SWU-CLI-015
         */
        host_buffer(const storage_callbacks& storage, std::size_t size) noexcept : m_storage(storage)
        {
          if ((storage.allocate != nullptr) && (size != 0U))
          {
            const auto block = storage.allocate(storage.context, size, alignof(char));
            if (block.has_value())
            {
              m_data = static_cast<char*>(*block.value_if());
              m_size = size;
            }
          }
        }

        host_buffer(const host_buffer&) = delete;
        host_buffer& operator=(const host_buffer&) = delete;
        host_buffer(host_buffer&&) = delete;
        host_buffer& operator=(host_buffer&&) = delete;

        /**
         * @brief Release the buffer.
         * @par Requirements
         * P056; J5-046.
         * @par ID SWU-CLI-016
         */
        ~host_buffer() noexcept
        {
          if ((m_data != nullptr) && (m_storage.deallocate != nullptr))
          {
            m_storage.deallocate(m_storage.context, m_data, m_size, alignof(char));
          }
        }

        /**
         * @brief Return the buffer, or nullptr when allocation failed.
         * @par Requirements
         * P056.
         * @par ID SWU-CLI-017
         */
        [[nodiscard]] char* data() const noexcept
        {
          return m_data;
        }
    };

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
     * @brief Report whether a parser failure means invalid input (exit 1) rather than an
     * operational failure (exit 2).
     * @par Requirements
     * P056; P057; J5-070.
     * @par ID SWU-CLI-021
     */
    bool is_input_error(error_code code) noexcept
    {
      return (code == error_code::syntax) || (code == error_code::encoding) || (code == error_code::duplicate_policy) ||
             (code == error_code::security_policy);
    }

    /**
     * @brief validate: read the input, parse it with the selected syntax and limits.
     * @par Requirements
     * P056; P057; J5-070.
     * @par ID SWU-CLI-022
     */
    int run_validate(const options& selected, const environment& host) noexcept
    {
      if (!selected.schema.empty())
      {
        return report_failure(host, "schema validation is not available in this build", selected.schema);
      }
      const std::size_t capacity = selected.limits.max_input_bytes;
      host_buffer input{host.storage, capacity};
      if (input.data() == nullptr)
      {
        return report_failure(host, "out of memory", {});
      }
      std::size_t size = 0U;
      const io_status read = (host.input.read != nullptr)
                               ? host.input.read(host.input.context, selected.input, input.data(), capacity, size)
                               : io_status::read_error;
      if (read != io_status::ok)
      {
        return report_failure(host, describe_io(read), selected.input);
      }
      parse_options parse{};
      parse.syntax = *selected.syntax;
      parse.duplicate_names = selected.duplicates;
      parse.limits = selected.limits;
      accounted_storage accounting{host.storage, parse.limits.max_owned_bytes};
      const input_view text{input.data(), size};
      const auto parsed = parse_document(text, parse, accounting);
      text_writer out{host.output, output_stream::out};
      if (parsed.has_value())
      {
        out.put(selected.input).put(": valid\n");
        return out.ok() ? exit_ok : exit_failure;
      }
      std::array<char, 512> message{};
      const auto formatted = format_error(*parsed.error_if(), text, message.data(), message.size());
      const std::size_t length = formatted.has_value()
                                   ? ((*formatted.value_if() < message.size()) ? *formatted.value_if() : message.size())
                                   : 0U;
      out.put(selected.input).put(": ").put(std::string_view{message.data(), length}).put("\n");
      if (!out.ok())
      {
        return exit_failure;
      }
      return is_input_error(parsed.error_if()->code()) ? exit_invalid : exit_failure;
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
    case command::version:
    case command::capabilities:
    case command::inspect:
    case command::run:
    default:
      return report_failure(host, "command is not available yet", argv[1]);
    }
  }
} // namespace adaptive_json5::cli
