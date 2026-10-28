#include "adaptive_json5/json5_document.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "memory/buffer_document_storage.hpp"

#include <cstddef>
#include <cstdint>
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
   * @note The arena and the accounting are placed at the start of the buffer. On failure
   * the parser has released its allocations and the control block is destroyed.
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

    auto parsed = parse_document(input, options, *storage.value_if()->accounting());

    if (!parsed.has_value())
    {
      return document_result::failure(*parsed.error_if());
    }

    return document_result::success(json5_document{std::move(*storage.value_if()), std::move(*parsed.value_if())});
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
} // namespace adaptive_json5
