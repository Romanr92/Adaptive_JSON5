#ifndef ADAPTIVE_JSON5_CLI_REPORT_HPP_
#define ADAPTIVE_JSON5_CLI_REPORT_HPP_

#include "adaptive_json5/text_position.hpp"
#include "cli/cli_app.hpp"
#include "cli/cli_io.hpp"
#include "cli/cli_options.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace adaptive_json5::cli
{
  /// @brief Version of the strict-JSON diagnostic and capability formats (J5-071).
  inline constexpr std::size_t c_format_version = 1U;

  /**
   * @brief Diagnostic severity.
   * @par Requirements
   * P057; J5-071.
   */
  enum class severity : std::uint8_t
  {
    error,
    warning
  };

  /**
   * @brief What kind of problem a diagnostic describes; stable names in the JSON format.
   * @par Requirements
   * P057; J5-070; J5-071.
   */
  enum class category : std::uint8_t
  {
    io,          // Input or schema file could not be read.
    syntax,      // Grammar violation.
    encoding,    // Not well-formed UTF-8.
    duplicate,   // Duplicate member name rejected.
    security,    // Rejected by a secure-mode option.
    limit,       // A configured limit or budget was exceeded.
    allocation,  // Out of memory.
    internal,    // Library invariant failed.
    string,      // String warning (raw separators, unpaired surrogate escapes).
    schema,      // The schema itself is invalid or cannot be compiled.
    assertion,   // The instance fails a schema assertion.
    instance,    // The instance is outside the schema component's domain.
    unsupported, // A requested capability is not available.
    mismatch     // run: an observed outcome differs from the manifest expectation.
  };

  /**
   * @brief Overall outcome written at the end of a report.
   * @par Requirements
   * P057; J5-070.
   */
  enum class outcome : std::uint8_t
  {
    valid,
    invalid,
    error
  };

  /**
   * @brief One diagnostic; text fields are borrowed for the duration of reporter::add().
   * @par Requirements
   * P057; J5-071.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: The type relies on its member types for copy, move and destruction, or explicitly restricts
  // ownership transfer.
  // codeql::misra_deviation_next_line(DEV-204.9)
  struct diagnostic
  {
      severity level = severity::error;
      category kind = category::internal;
      std::string_view message{};
      std::optional<text_position> position{};
      std::optional<std::size_t> byte_offset{};
      std::string_view keyword{};
      std::string_view instance_pointer{};
      std::string_view schema_pointer{};
  };

  /**
   * @brief Write a string as a strict-JSON value: an escaped string when it is valid UTF-8,
   * otherwise {"$bytes": [...]} (ADR 0009 D2).
   * @par Requirements
   * P057; J5-071.
   * @par ID SWU-CLI-033
   */
  void put_json_text(text_writer& out, std::string_view text) noexcept;

  /**
   * @brief Writes one command's diagnostics as readable text or as a versioned strict-JSON
   * document, without allocating.
   * @par Requirements
   * P057; J5-070; J5-071; ADR 0009.
   * @note Text: diagnostics on standard output as "<input>: <message>", "<input>: valid" at the
   * end of a valid run, I/O and operational errors on standard error as "error: ...".
   * JSON: one object on standard output with format, version, command, input, diagnostics,
   * result and exit.
   */
  // MISRA rule: RULE-15-0-1 cpp/misra/improperly-provided-special-member-functions-audit
  // MISRA justification: The type relies on its member types for copy, move and destruction, or explicitly restricts
  // ownership transfer.
  // codeql::misra_deviation_next_line(DEV-204.8)
  class reporter
  {
    private:
      text_writer m_out;
      text_writer m_err;
      diagnostics_format m_format;
      std::string_view m_input;
      std::size_t m_count{};

      void add_text(const diagnostic& item) noexcept;
      void put_details(const diagnostic& item) noexcept;

    public:
      /**
       * @brief Start a report; JSON output writes its header immediately.
       * @par Requirements
       * P057; J5-071.
       * @par ID SWU-CLI-034
       */
      reporter(const output_sink& sink, diagnostics_format format, std::string_view command,
               std::string_view input) noexcept;

      /**
       * @brief Add one diagnostic.
       * @par Requirements
       * P057; J5-071.
       * @par ID SWU-CLI-035
       */
      void add(const diagnostic& item) noexcept;

      /**
       * @brief Finish the report and return the exit code (exit_failure when any write failed).
       * @par Requirements
       * P057; J5-070; J5-071.
       * @par ID SWU-CLI-036
       */
      [[nodiscard]] std::int32_t finish(outcome result, std::int32_t exit) noexcept;
  };

  /**
   * @brief Stable JSON name of a category.
   * @par Requirements
   * P057; J5-071.
   * @par ID SWU-CLI-037
   */
  [[nodiscard]] std::string_view category_name(category kind) noexcept;
} // namespace adaptive_json5::cli

#endif /* ADAPTIVE_JSON5_CLI_REPORT_HPP_ */
