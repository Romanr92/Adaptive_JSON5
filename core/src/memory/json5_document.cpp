#include "adaptive_json5/json5_document.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/document_storage_owner.hpp"
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
     * @param input Input bytes: input.data and input.size.
     * @param buffer Start of the caller's storage buffer.
     * @param buffer_size Bytes in the storage buffer.
     * @return True when the two byte ranges share at least one byte.
     * @note Steps:
     * 1. If input.size is 0, buffer_size is 0, input.data is nullptr or buffer is nullptr,
     *    return false: an empty range cannot overlap anything.
     * 2. Convert both start pointers to std::uintptr_t with reinterpret_cast
     *    (input_begin, buffer_begin).
     * 3. If input_begin < buffer_begin, the ranges overlap when
     *    (buffer_begin - input_begin) < input.size: the buffer starts inside the input.
     * 4. Otherwise they overlap when (input_begin - buffer_begin) < buffer_size: the input
     *    starts inside the buffer.
     * Compare differences rather than computing begin + size, so nothing can overflow.
     * Why: writing parse storage over the input would corrupt it while it is still read.
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
   * @note Use the member initializer list: move storage into m_storage and parsed into
   * m_parsed (std::move for both). The body stays empty.
   * Initializer order follows the declaration order in the header (m_storage first).
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
   * @return The document, or the first failure.
   * @note Use an alias: using document_result = result<json5_document>.
   * Steps, each returning on failure:
   * 1. If overlaps(input, buffer, buffer_size), return
   *    document_result::failure(error::without_location(error_code::wrong_type_or_access)).
   * 2. Call detail::make_buffer_document_storage(buffer, buffer_size,
   *    options.limits.max_owned_bytes). It places the arena and the accounting at the start
   *    of the buffer. If it failed, return its error unchanged (document_result::failure
   *    with *storage.error_if()); a buffer too small for the control block reports
   *    allocation_failure there.
   * 3. Call parse_document(input, options, accounting), where accounting is
   *    *storage.value_if()->accounting() (accounting() returns a pointer).
   * 4. If parsing failed, return its error unchanged. Nothing to clean up: the parser has
   *    already released its allocations, and the storage owner destroys the control
   *    block when it goes out of scope.
   * 5. Return document_result::success(json5_document{...}), constructed from
   *    std::move(*storage.value_if()) and std::move(*parsed.value_if()). parse may call
   *    the private constructor because it is a member of the class.
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
   * @brief Transfer parsed data and ownership, leaving other empty.
   * @par Requirements
   * J5-040; J5-051.
   * @note No body needed: write "= default;" instead of the braces. The defaulted move
   * moves m_storage (its move empties the source through std::exchange) and m_parsed,
   * which is exactly what is required. noexcept must match the header.
   */
  json5_document::json5_document(json5_document&& other) noexcept = default;

  /**
   * @brief Release parsed allocations before destroying their storage.
   * @par Requirements
   * J5-040; J5-051.
   * @note No body needed: write "= default;" instead of the braces. Members are destroyed
   * in reverse declaration order, so m_parsed releases its allocations before m_storage
   * destroys the control block holding the accounting they are released through.
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
   * @return m_storage.accounting()->current_bytes(), or 0 when m_storage.accounting() is
   * nullptr (this object was moved from).
   * @note One expression with the conditional operator is enough.
   */
  std::size_t json5_document::current_bytes() const noexcept
  {
    return ((m_storage.accounting() != nullptr) ? m_storage.accounting()->current_bytes() : 0);
  }

  /**
   * @brief Report the highest outstanding requested allocation byte count.
   * @par Requirements
   * J5-046; J5-047.
   * @return m_storage.accounting()->peak_bytes(), or 0 when m_storage.accounting() is
   * nullptr (this object was moved from).
   * @note Same shape as current_bytes().
   */
  std::size_t json5_document::peak_bytes() const noexcept
  {
    return ((m_storage.accounting() != nullptr) ? m_storage.accounting()->peak_bytes() : 0);
  }
} // namespace adaptive_json5
