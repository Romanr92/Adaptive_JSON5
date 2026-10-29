#include "adaptive_json5/json5_document.hpp"

#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/retained_source.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "memory/buffer_document_storage.hpp"
#include "memory/json5_document_access.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    /**
     * @brief Report whether the input bytes and the storage buffer share any byte.
     * @par Requirements
     * J5-040; J5-046.
     * @param input Input bytes.
     * @param buffer Start of the caller's storage buffer.
     * @param buffer_size Bytes in the storage buffer.
     * @return True when the two byte ranges share at least one byte; false when either is
     * empty.
     * @note Compares address differences, so no range end is computed and nothing overflows.
     * Writing parse storage over the input would corrupt it while it is still read.
     */
    [[nodiscard]] bool overlaps(input_view input, const void* buffer, std::size_t buffer_size) noexcept
    {
      if ((input.size == 0) || (buffer_size == 0) || (input.data == nullptr) || (buffer == nullptr))
      {
        return false;
      }

      std::uintptr_t input_begin = reinterpret_cast<std::uintptr_t>(input.data);
      std::uintptr_t buffer_begin = reinterpret_cast<std::uintptr_t>(buffer);

      // Two ranges overlap when each starts before the other one ends; subtraction avoids overflow.
      return (input_begin < buffer_begin) ? ((buffer_begin - input_begin) < input.size)
                                          : ((input_begin - buffer_begin) < buffer_size);
    }

    /**
     * @brief Turn a formatter's full length into a view of the text kept in the buffer.
     * @par Requirements
     * J5-043; J5-044.
     * @param length Full message length reported by format_error() or format_warning().
     * @return A view of at most capacity - 1 bytes, the part that fit before the NUL.
     */
    [[nodiscard]] std::string_view kept_text(const char* buffer, std::size_t capacity, std::size_t length) noexcept
    {
      // The formatters report the full length even when they had to truncate.
      const std::size_t kept = (capacity == 0) ? 0 : std::min(length, capacity - 1);

      return std::string_view{buffer, kept};
    }
  } // namespace

  /**
   * @brief Adopt parsed data and the storage supporting it.
   * @par Requirements
   * J5-040; J5-051.
   * @param storage Owner of the control block whose accounting allocated parsed.
   * @param parsed Document, warnings and retained source parsed through that accounting.
   */
  json5_document::json5_document(detail::document_storage_owner&& storage, parsed_document&& parsed) noexcept
      : m_storage(std::move(storage)), m_parsed(std::move(parsed))
  {
  }

  /**
   * @brief Parse one complete document using a caller-owned buffer.
   * @par Requirements
   * P037; J5-026; J5-040; J5-046; J5-047; J5-051; J5-101.
   * @param input Borrowed input bytes.
   * @param buffer Caller storage reserved for this document.
   * @param buffer_size Bytes in buffer.
   * @param options Syntax, policies and limits, passed unchanged to parse_document.
   * @return The document; wrong_type_or_access when input and buffer overlap;
   * allocation_failure when the buffer cannot hold the control block or the document;
   * or the error of parse_document, unchanged.
   * @note The arena and the accounting are placed at the start of the buffer; parsing and
   * the failure cleanup are done by json5_document_access::parse_with_storage.
   */
  result<json5_document> json5_document::parse(input_view input, void* buffer, std::size_t buffer_size,
                                               const parse_options& options) noexcept
  {
    using document_result = result<json5_document>;

    if (overlaps(input, buffer, buffer_size))
    {
      return document_result::failure(error::without_location(error_code::wrong_type_or_access));
    }

    auto storage = detail::make_buffer_document_storage(buffer, buffer_size, options.limits.max_owned_bytes);

    if (!storage.has_value())
    {
      return document_result::failure(*storage.error_if());
    }

    return detail::json5_document_access::parse_with_storage(input, options, std::move(*storage.value_if()));
  }

  /**
   * @brief Start navigating at the root value.
   * @par Requirements
   * J5-026; J5-028; J5-040.
   */
  json5_value json5_document::root() const noexcept
  {
    // A moved-from document has no accounting; its empty document makes root() fail anyway.
    const storage_callbacks scratch =
      (m_storage.accounting() != nullptr) ? m_storage.accounting()->callbacks() : storage_callbacks{};

    return json5_value{m_parsed.values.root(), scratch};
  }

  /**
   * @brief Report how many warnings the parse retained.
   * @par Requirements
   * J5-017; J5-041.
   */
  std::size_t json5_document::warning_count() const noexcept
  {
    return m_parsed.warnings.size();
  }

  /**
   * @brief Read one retained warning, in source order.
   * @par Requirements
   * J5-017; J5-041.
   */
  result<string_warning> json5_document::warning(std::size_t index) const noexcept
  {
    return m_parsed.warnings.at(index);
  }

  /**
   * @brief Transfer parsed data and ownership, leaving other empty.
   * @par Requirements
   * J5-040; J5-051.
   * @note The storage owner empties other, so the accounting keeps its address.
   */
  json5_document::json5_document(json5_document&& other) noexcept = default;

  /**
   * @brief Release parsed allocations before destroying their storage.
   * @par Requirements
   * J5-040; J5-051.
   * @note Member order guarantees it: m_parsed is destroyed before m_storage.
   */
  json5_document::~json5_document() noexcept = default;

  /**
   * @brief Borrow the parsed values through the advanced document API.
   * @par Requirements
   * J5-028; J5-040.
   * @return A reference to m_parsed.values.
   */
  const document& json5_document::values() const noexcept
  {
    return m_parsed.values;
  }

  /**
   * @brief Borrow the retained source copy.
   * @par Requirements
   * J5-041.
   * @return A reference to m_parsed.source.
   */
  const retained_source& json5_document::source() const noexcept
  {
    return m_parsed.source;
  }

  /**
   * @brief Report outstanding requested allocation bytes.
   * @par Requirements
   * J5-046; J5-047.
   * @return The accounted bytes, or zero after this document was moved from.
   */
  std::size_t json5_document::current_bytes() const noexcept
  {
    return ((m_storage.accounting() != nullptr) ? m_storage.accounting()->current_bytes() : 0);
  }

  /**
   * @brief Report the highest outstanding requested allocation byte count.
   * @par Requirements
   * J5-046; J5-047.
   * @return The peak, or zero after this document was moved from.
   */
  std::size_t json5_document::peak_bytes() const noexcept
  {
    return ((m_storage.accounting() != nullptr) ? m_storage.accounting()->peak_bytes() : 0);
  }

  /**
   * @brief Describe an error as "line L, column C: message" in a caller buffer.
   * @par Requirements
   * J5-043; J5-044.
   */
  result<std::string_view> describe(const error& failure, input_view input, char* buffer, std::size_t capacity) noexcept
  {
    const auto length = format_error(failure, input, buffer, capacity);

    if (!length.has_value())
    {
      return result<std::string_view>::failure(*length.error_if());
    }

    return result<std::string_view>::success(kept_text(buffer, capacity, *length.value_if()));
  }

  /**
   * @brief Describe a warning as "line: L, column: C: warning: message" in a caller buffer.
   * @par Requirements
   * J5-017; J5-044.
   */
  result<std::string_view> describe(const string_warning& warning, input_view input, char* buffer,
                                    std::size_t capacity) noexcept
  {
    const auto length = format_warning(warning, input, buffer, capacity);

    if (!length.has_value())
    {
      return result<std::string_view>::failure(*length.error_if());
    }

    return result<std::string_view>::success(kept_text(buffer, capacity, *length.value_if()));
  }

  namespace detail
  {
    /**
     * @brief Parse one complete document through the storage of an owner.
     * @par Requirements
     * P037; J5-026; J5-040; J5-046; J5-051.
     */
    result<json5_document> json5_document_access::parse_with_storage(input_view input, const parse_options& options,
                                                                     document_storage_owner&& storage) noexcept
    {
      using document_result = result<json5_document>;

      auto parsed = parse_document(input, options, *storage.accounting());

      // On failure the parser already released its allocations; storage then destroys the block.
      if (!parsed.has_value())
      {
        return document_result::failure(*parsed.error_if());
      }

      return document_result::success(json5_document{std::move(storage), std::move(*parsed.value_if())});
    }
  } // namespace detail
} // namespace adaptive_json5
