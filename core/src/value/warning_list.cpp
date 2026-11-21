#include "adaptive_json5/value/warning_list.hpp"

#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "common/checked_size.hpp"

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    // Entries are copied as raw bytes, which is only valid for trivially copyable items.
    static_assert(std::is_trivially_copyable_v<string_warning>, "warning entries are copied bytewise");

    constexpr std::size_t c_growth_factor = 2;

    /**
     * @brief Read one stored entry from raw owned storage.
     * @par Requirements
     * P039; J5-046.
     * @param storage Block holding at least index + 1 entries.
     * @param index Initialized entry position.
     * @return Copy of the stored entry.
     * @note Byte copying avoids constructing objects inside raw storage.
     * @par ID SWU-VALUE-136
     */
    [[nodiscard]] string_warning read_entry(const owned_block& storage, std::size_t index) noexcept
    {
      string_warning entry{};
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Warnings are stored as bytes. Copy the chosen entry into a live local object
      // without assuming that the block contains an aligned string_warning.
      // codeql::misra_deviation_next_line(DEV-002.23)
      const auto* bytes = static_cast<const std::byte*>(storage.data());

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: index is below the slot count of the block, and the block size (count * sizeof)
      // was computed with checked_multiply when it was allocated, so this product fits std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.64)
      const std::size_t offset = index * sizeof(string_warning);
      // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
      // MISRA justification: std::memcpy copies bytes between non-overlapping buffers whose sizes were checked
      // when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object
      // representation to or from untyped storage without alignment or aliasing assumptions.
      // codeql::misra_deviation_next_line(DEV-023.04)
      static_cast<void>(std::memcpy(&entry, bytes + offset, sizeof(entry)));

      return entry;
    }

    /**
     * @brief Store one entry into raw owned storage.
     * @par Requirements
     * P039; J5-046.
     * @param storage Block with room for at least index + 1 entries.
     * @param index Entry position to write.
     * @param entry Value to store.
     * @par ID SWU-VALUE-137
     */
    // MISRA rule: RULE-10-1-1 cpp/misra/pointer-or-ref-param-not-const
    // MISRA justification: The function writes into the bytes owned by the block. owned_block::data() is const-
    // qualified, but a non-const reference documents that the call modifies the block's contents.
    // codeql::misra_deviation_next_line(DEV-018.02)
    void write_entry(owned_block& storage, std::size_t index, const string_warning& entry) noexcept
    {
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Write the warning's object representation to the reserved byte slot with memcpy,
      // so the block need not hold an aligned string_warning object.
      // codeql::misra_deviation_next_line(DEV-002.24)
      auto* bytes = static_cast<std::byte*>(storage.data());

      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: index is below the slot count of the block, and the block size (count * sizeof)
      // was computed with checked_multiply when it was allocated, so this product fits std::size_t.
      // codeql::misra_deviation_next_line(DEV-004.63)
      const std::size_t offset = index * sizeof(string_warning);
      // MISRA rule: RULE-24-5-2 cpp/misra/no-memory-functions-from-c-string
      // MISRA rule: RULE-8-7-1 cpp/misra/pointer-argument-to-cstring-function-is-invalid
      // MISRA justification: std::memcpy copies bytes between non-overlapping buffers whose sizes were checked
      // when the storage was allocated. In C++17 this is the defined way to copy a trivially copyable object
      // representation to or from untyped storage without alignment or aliasing assumptions. entry is a
      // reference to a string_warning, so &entry points to a whole string_warning and sizeof(entry) is the size
      // of that object. The tool takes the size of a pointer instead.
      // codeql::misra_deviation_next_line(DEV-023.03)
      static_cast<void>(std::memcpy(bytes + offset, &entry, sizeof(entry)));
    }

    /**
     * @brief Choose the capacity of the next allocation.
     * @par Requirements
     * P039; J5-046.
     * @param policy Storage policy of the list.
     * @param current Current capacity.
     * @param required Entries needed; not above maximum.
     * @param maximum Maximum entries of the list.
     * @return A capacity between required and maximum.
     * @par ID SWU-VALUE-138
     */
    [[nodiscard]] std::size_t next_capacity(warning_storage_policy policy, std::size_t current, std::size_t required,
                                            std::size_t maximum) noexcept
    {
      // The fixed policy allocates the worst case once, so the table never grows.
      if (policy == warning_storage_policy::fixed)
      {
        return maximum;
      }

      std::size_t capacity = warning_list::c_adaptive_initial_entries;

      if (current != std::size_t{})
      {
        // Doubling keeps reallocations logarithmic; on overflow the maximum is the answer anyway.
        const auto doubled = checked_multiply(current, c_growth_factor, resource_limit_reason::diagnostics);
        capacity = doubled.has_value() ? *doubled.value_if() : maximum;
      }

      if (capacity > maximum)
      {
        capacity = maximum;
      }

      return ((capacity < required) ? required : capacity);
    }
  } // namespace

  /**
   * @brief Borrow the provider and record the maximum and policy without allocating.
   * @par Requirements
   * P039; J5-046; J5-047.
   * @par ID SWU-VALUE-139
   */
  warning_list::warning_list(const storage_callbacks& provider, std::size_t max_entries,
                             warning_storage_policy policy) noexcept
      : m_provider(provider), m_max_entries(max_entries), m_policy(policy)
  {
  }

  /**
   * @brief Take over the storage and counts, then empty the source.
   * @par Requirements
   * P039; J5-040.
   * @par ID SWU-VALUE-140
   */
  // MISRA rule: RULE-15-0-2 cpp/misra/invalid-signature-for-special-member-function
  // MISRA rule: RULE-18-4-1 cpp/misra/exception-unfriendly-function-must-be-noexcept
  // MISRA justification: The move operation is declared noexcept in both its declaration and its definition.
  // The tool does not see the specifier in the -fno-exceptions analysis build.
  // codeql::misra_deviation_next_line(DEV-021.01)
  warning_list::warning_list(warning_list&& other) noexcept
      : m_provider(other.m_provider), m_storage(std::move(other.m_storage)), m_size(other.m_size),
        m_capacity(other.m_capacity), m_max_entries(other.m_max_entries), m_policy(other.m_policy)
  {
    // The moved-from list must not report entries whose storage it no longer owns.
    other.m_size = 0;
    other.m_capacity = 0;
  }

  /**
   * @brief Grow into a replacement block and publish it only after copying succeeds.
   * @par Requirements
   * P039; J5-003; J5-046.
   * @par ID SWU-VALUE-141
   */
  status_result warning_list::reserve(std::size_t additional) noexcept
  {
    const auto required = checked_add(m_size, additional, resource_limit_reason::diagnostics);

    if (!required.has_value())
    {
      return status_result::failure(*required.error_if());
    }

    // Capacity belongs to the list, so no caller can grow it past its maximum.
    if (*required.value_if() > m_max_entries)
    {
      return status_result::failure(error::limit_without_location(resource_limit_reason::diagnostics));
    }

    if (*required.value_if() <= m_capacity)
    {
      return status_result::success();
    }

    const std::size_t capacity = next_capacity(m_policy, m_capacity, *required.value_if(), m_max_entries);

    auto allocation = allocate_array(m_provider, capacity, sizeof(string_warning), alignof(string_warning));

    if (!allocation.has_value())
    {
      return status_result::failure(*allocation.error_if());
    }

    owned_block& replacement = *allocation.value_if();

    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: The loop is simple: one counter declared in the init-statement, compared with a
    // bound that the body does not modify, and changed only in the increment expression. The counter is
    // std::size_t or std::ptrdiff_t, which are integer types; the tool does not resolve the typedef.
    // codeql::misra_deviation_next_line(DEV-017.14)
    for (std::size_t index{}; index < m_size; ++index)
    {
      write_entry(replacement, index, read_entry(m_storage, index));
    }

    // Publishing last means any earlier failure left the existing entries untouched.
    m_storage = std::move(replacement);
    m_capacity = capacity;

    return status_result::success();
  }

  /**
   * @brief Write into reserved space without allocating.
   * @par Requirements
   * P039; J5-003; J5-017.
   * @par ID SWU-VALUE-142
   */
  status_result warning_list::append_reserved(const string_warning& warning) noexcept
  {
    if (m_size >= m_capacity)
    {
      return status_result::failure(error::access_failure(access_error_reason::invalid_argument));
    }

    write_entry(m_storage, m_size, warning);
    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: append_reserved() checked m_size against the reserved capacity, so the increment
    // stays within it.
    // codeql::misra_deviation_next_line(DEV-004.62)
    ++m_size;

    return status_result::success();
  }

  /**
   * @brief Report the retained entry count.
   * @par Requirements
   * P039; J5-017.
   * @par ID SWU-VALUE-143
   */
  std::size_t warning_list::size() const noexcept
  {
    return m_size;
  }

  /**
   * @brief Return a copy of one checked entry.
   * @par Requirements
   * P039; J5-003; J5-017; J5-044.
   * @par ID SWU-VALUE-144
   */
  result<string_warning> warning_list::at(std::size_t index) const noexcept
  {
    if (index >= m_size)
    {
      return result<string_warning>::failure(error::access_failure(access_error_reason::index_out_of_range));
    }

    return result<string_warning>::success(read_entry(m_storage, index));
  }
} // namespace adaptive_json5
