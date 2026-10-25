#include "adaptive_json5/diagnostic_format.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "adaptive_json5/text_position.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Append message pieces to caller storage while counting the untruncated length.
     * @par Requirements
     * J5-045; issue #96.
     * @note Writes at most capacity - 1 characters and reserves the last byte for NUL.
     * Keeps counting after the buffer is full so callers learn the required size.
     * Performs no allocation and never writes outside buffer.
     */
    class message_writer
    {
      private:
        char* m_buffer;
        std::size_t m_capacity;
        std::size_t m_length{};

        /**
         * @brief Start an empty message in caller storage.
         * @par Requirements
         * Issue #96.
         * @param buffer Caller storage, or nullptr when capacity is 0.
         * @param capacity Size of buffer in bytes, including the NUL.
         */
        message_writer(char* buffer, std::size_t capacity) noexcept : m_buffer{buffer}, m_capacity{capacity} {}

      public:
        /**
         * @brief Create a writer, rejecting storage it could not write safely.
         * @par Requirements
         * J5-045; issue #96.
         * @param buffer Caller storage, or nullptr when capacity is 0.
         * @param capacity Size of buffer in bytes, including the NUL.
         * @return The writer, or wrong_type_or_access for a null buffer with nonzero capacity.
         * @note A null buffer with zero capacity is a valid size query.
         */
        [[nodiscard]] static result<message_writer> create(char* buffer, std::size_t capacity) noexcept
        {
          if ((buffer == nullptr) && (capacity != 0U))
          {
            return result<message_writer>::failure(error::without_location(error_code::wrong_type_or_access));
          }

          return result<message_writer>::success(message_writer{buffer, capacity});
        }

        /**
         * @brief Append text, copying only what still fits before the reserved NUL byte.
         * @par Requirements
         * Issue #96.
         * @param text Characters to append; contains no input bytes.
         */
        void append(std::string_view text) noexcept
        {
          for (const char character : text)
          {
            // The last byte stays free for the terminator, so stop copying one byte early.
            if ((m_capacity != 0U) && (m_length < (m_capacity - 1U)))
            {
              m_buffer[m_length] = character;
            }

            ++m_length;
          }
        }

        /**
         * @brief Append an unsigned number in decimal without locale or allocation.
         * @par Requirements
         * Issue #96.
         * @param value Number to print, e.g. a line or column.
         */
        void append_number(std::size_t value) noexcept
        {
          // Enough room for the largest std::size_t in decimal (20 digits for 64 bits).
          constexpr std::size_t c_max_digits = std::numeric_limits<std::size_t>::digits10 + 1U;
          char digits[c_max_digits]{};
          std::size_t start = c_max_digits;

          // Produce digits from the right, so the loop needs no reversal step.
          do
          {
            --start;
            digits[start] = static_cast<char>('0' + (value % 10U));
            value /= 10U;
          } while (value != 0U);

          append(std::string_view{&digits[start], c_max_digits - start});
        }

        /**
         * @brief Terminate the stored text and report the full message length.
         * @par Requirements
         * Issue #96.
         * @return Untruncated length excluding the NUL.
         * @note Writes the NUL after the last stored character whenever capacity is nonzero.
         */
        std::size_t finish() noexcept
        {
          if (m_capacity != 0U)
          {
            // Truncated text ends at the reserved byte; complete text ends right after itself.
            m_buffer[(m_length < m_capacity) ? m_length : (m_capacity - 1U)] = '\0';
          }

          return m_length;
        }
    };

    /**
     * @brief Give the fixed text for an error category.
     * @par Requirements
     * J5-043; issue #96.
     * @param code Error category.
     * @return Static English text; "unknown error" for a value outside the enumeration.
     * @note No default label, so -Wswitch reports a newly added category without text.
     */
    std::string_view describe(error_code code) noexcept
    {
      switch (code)
      {
      case error_code::syntax:
        return "syntax error";
      case error_code::encoding:
        return "invalid UTF-8";
      case error_code::duplicate_policy:
        return "duplicate member name";
      case error_code::wrong_type_or_access:
        return "wrong type or invalid access";
      case error_code::numeric_conversion:
        return "number cannot be converted";
      case error_code::resource_limit:
        return "resource limit exceeded";
      case error_code::allocation_failure:
        return "allocation failure";
      }

      // A value outside the enumeration still gets text instead of undefined behavior.
      return "unknown error";
    }

    /**
     * @brief Give the fixed text for a syntax error reason.
     * @par Requirements
     * J5-043; J5-044; issue #96.
     * @param reason Specific grammar violation.
     * @return Static English text; "syntax error" for a value outside the enumeration.
     */
    std::string_view describe(syntax_error_reason reason) noexcept
    {
      switch (reason)
      {
      case syntax_error_reason::unexpected_end:
        return "unexpected end of input";
      case syntax_error_reason::unexpected_character:
        return "unexpected character";
      case syntax_error_reason::unexpected_comma:
        return "unexpected comma";
      case syntax_error_reason::unexpected_colon:
        return "unexpected colon";
      case syntax_error_reason::unexpected_closing_bracket:
        return "unexpected closing bracket";
      case syntax_error_reason::missing_colon:
        return "missing colon";
      case syntax_error_reason::missing_comma:
        return "missing comma";
      case syntax_error_reason::missing_value:
        return "missing value";
      case syntax_error_reason::expected_member_name:
        return "expected member name";
      case syntax_error_reason::unterminated_string:
        return "unterminated string";
      case syntax_error_reason::unterminated_comment:
        return "unterminated comment";
      case syntax_error_reason::line_terminator_in_string:
        return "line terminator in string";
      case syntax_error_reason::control_character_in_string:
        return "control character in string";
      case syntax_error_reason::invalid_escape:
        return "invalid escape sequence";
      case syntax_error_reason::invalid_number:
        return "invalid number";
      case syntax_error_reason::invalid_identifier:
        return "invalid identifier";
      case syntax_error_reason::unknown_literal:
        return "unknown literal";
      case syntax_error_reason::not_allowed_in_strict_json:
        return "not allowed in strict JSON";
      case syntax_error_reason::empty_input:
        return "empty input";
      case syntax_error_reason::trailing_content:
        return "unexpected content after value";
      }

      return "syntax error";
    }

    /**
     * @brief Give the fixed text for a numeric conversion reason.
     * @par Requirements
     * J5-035; J5-043; issue #96.
     * @param reason Why the conversion failed.
     * @return Static English text; "number cannot be converted" for a value outside the enumeration.
     */
    std::string_view describe(numeric_conversion_reason reason) noexcept
    {
      switch (reason)
      {
      case numeric_conversion_reason::non_integral:
        return "number is not an integer";
      case numeric_conversion_reason::out_of_range:
        return "number is out of range";
      case numeric_conversion_reason::non_finite:
        return "number is not finite";
      case numeric_conversion_reason::underflow:
        return "number underflows to zero";
      case numeric_conversion_reason::unsupported_destination:
        return "unsupported numeric destination";
      }

      return "number cannot be converted";
    }

    /**
     * @brief Give the fixed text for a string warning.
     * @par Requirements
     * J5-017; issue #96.
     * @param code Warning category.
     * @return Static English text; "string warning" for a value outside the enumeration.
     */
    std::string_view describe(string_warning_code code) noexcept
    {
      switch (code)
      {
      case string_warning_code::raw_line_separator:
        return "raw U+2028 line separator in string";
      case string_warning_code::raw_paragraph_separator:
        return "raw U+2029 paragraph separator in string";
      case string_warning_code::unpaired_surrogate_escape:
        return "unpaired surrogate escape";
      }

      return "string warning";
    }

    /**
     * @brief Choose the most specific text available for an error.
     * @par Requirements
     * J5-043; issue #96.
     * @param failure Error to describe.
     * @return The syntax or numeric reason text when present, otherwise the category text.
     */
    std::string_view describe(const error& failure) noexcept
    {
      // A reason is only meaningful together with its own category.
      if ((failure.code == error_code::syntax) && failure.syntax_reason.has_value())
      {
        return describe(*failure.syntax_reason);
      }

      if ((failure.code == error_code::numeric_conversion) && failure.numeric_reason.has_value())
      {
        return describe(*failure.numeric_reason);
      }

      return describe(failure.code);
    }

    /**
     * @brief Append "line: L, column: C" for one located position.
     * @par Requirements
     * J5-044; issue #96.
     * @param writer Message being assembled.
     * @param position 1-based line and column from locate().
     */
    void append_position(message_writer& writer, text_position position) noexcept
    {
      writer.append("line: ");
      writer.append_number(position.line);
      writer.append(", column: ");
      writer.append_number(position.column);
    }
  } // namespace

  /**
   * @brief Assemble the error line from caller-located positions.
   * @par Requirements
   * J5-043; J5-044; J5-045; issue #96.
   * @note Validates storage once through message_writer::create; every later step cannot fail.
   */
  result<std::size_t> format_error_at(const error& failure, text_position primary,
                                      std::optional<text_position> first_duplicate, char* buffer,
                                      std::size_t capacity) noexcept
  {
    auto created = message_writer::create(buffer, capacity);

    if (!created.has_value())
    {
      return result<std::size_t>::failure(*created.error_if());
    }

    message_writer& writer = *created.value_if();

    // Unlocated errors such as allocation failures have no meaningful position to print.
    if (failure.has_byte_offset)
    {
      append_position(writer, primary);
      writer.append(": ");
    }

    writer.append(describe(failure));

    // Name the earlier occurrence too, so the reader sees both places that clash.
    if (failure.duplicate_names.has_value() && first_duplicate.has_value())
    {
      writer.append(" (first at ");
      append_position(writer, *first_duplicate);
      writer.append(")");
    }

    return result<std::size_t>::success(writer.finish());
  }

  /**
   * @brief Locate the error's positions in input, then format through format_error_at.
   * @par Requirements
   * J5-043; J5-044; J5-045; issue #96.
   * @note Routing through format_error_at guarantees both forms produce identical text.
   */
  result<std::size_t> format_error(const error& failure, input_view input, char* buffer, std::size_t capacity) noexcept
  {
    // Without a location there is nothing to scan; the position argument is ignored.
    if (!failure.has_byte_offset)
    {
      return format_error_at(failure, text_position{}, std::nullopt, buffer, capacity);
    }

    const auto primary = locate(input, failure.byte_offset);

    if (!primary.has_value())
    {
      return result<std::size_t>::failure(*primary.error_if());
    }

    std::optional<text_position> first_duplicate{};

    if (failure.duplicate_names.has_value())
    {
      const auto first = locate(input, failure.duplicate_names->first.byte_offset);

      if (!first.has_value())
      {
        return result<std::size_t>::failure(*first.error_if());
      }

      first_duplicate = *first.value_if();
    }

    return format_error_at(failure, *primary.value_if(), first_duplicate, buffer, capacity);
  }

  /**
   * @brief Assemble the warning line from a caller-located position.
   * @par Requirements
   * J5-017; J5-044; issue #96.
   */
  result<std::size_t> format_warning_at(const string_warning& warning, text_position position, char* buffer,
                                        std::size_t capacity) noexcept
  {
    auto created = message_writer::create(buffer, capacity);

    if (!created.has_value())
    {
      return result<std::size_t>::failure(*created.error_if());
    }

    message_writer& writer = *created.value_if();

    append_position(writer, position);
    writer.append(": warning: ");
    writer.append(describe(warning.code));

    return result<std::size_t>::success(writer.finish());
  }

  /**
   * @brief Locate the warning in input, then format through format_warning_at.
   * @par Requirements
   * J5-017; J5-044; issue #96.
   */
  result<std::size_t> format_warning(const string_warning& warning, input_view input, char* buffer,
                                     std::size_t capacity) noexcept
  {
    const auto position = locate(input, warning.byte_offset);

    if (!position.has_value())
    {
      return result<std::size_t>::failure(*position.error_if());
    }

    return format_warning_at(warning, *position.value_if(), buffer, capacity);
  }
} // namespace adaptive_json5
