#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_index.hpp"
#include "common/checked_size.hpp"
#include "value/value_record.hpp"

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace adaptive_json5
{
  namespace detail
  {
    static_assert(std::is_nothrow_move_constructible_v<value_record>);
    static_assert(std::is_nothrow_destructible_v<value_record>);
    /**
     * @brief Address one slot within an already size-checked allocation.
     * @par Requirements
     * P030; J5-040; J5-046.
     * @note Private callers provide index below the allocated capacity.
     * This returns storage, without assuming a record is alive there.
     * @par ID SWU-VALUE-109
     */
    void* value_storage::slot_address(const owned_block& storage, std::size_t index) noexcept
    {
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Compute the record slot as a byte offset within the allocated block before
      // requiring a live value_record at that address.
      // codeql::misra_deviation_next_line(DEV-002.20)
      auto* bytes = static_cast<std::byte*>(storage.data());
      return bytes + index * sizeof(value_record);
    }

    /**
     * @brief Access an already constructed record.
     * @par Requirements
     * P030; J5-039; J5-040.
     * @note Private callers provide index below the constructed record count.
     * @par ID SWU-VALUE-110
     */
    value_record* value_storage::live_record(std::size_t index) noexcept
    {
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: The caller supplies a slot that contains a constructed record. Launder the
      // typed pointer to access the current object at that address.
      // codeql::misra_deviation_next_line(DEV-002.21)
      return std::launder(static_cast<value_record*>(slot_address(m_storage, index)));
    }

    /**
     * @brief Access an already constructed record without mutation.
     * @par Requirements
     * P030; J5-039; J5-040.
     * @note Private callers provide index below the constructed record count.
     * @par ID SWU-VALUE-111
     */
    const value_record* value_storage::live_record(std::size_t index) const noexcept
    {
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: The caller supplies a slot that contains a constructed record. Launder the
      // typed pointer for read-only access to the current object at that address.
      // codeql::misra_deviation_next_line(DEV-002.22)
      return std::launder(static_cast<const value_record*>(slot_address(m_storage, index)));
    }

    /**
     * @brief Destroy constructed records without releasing the raw block.
     * @par Requirements
     * P030; J5-040.
     * @note Reverse-order iteration uses no recursive child traversal.
     * @par ID SWU-VALUE-112
     */
    void value_storage::destroy_records() noexcept
    {
      while (m_size != c_empty_size)
      {
        --m_size;
        // MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used
        // MISRA justification: Destroy each live record in reverse order so its owned blocks are released
        // before the record storage is released.
        // codeql::misra_deviation_next_line(DEV-001.07)
        live_record(m_size)->~value_record();
      }
    }

    /**
     * @brief Ensure capacity before moving an incoming record.
     * @par Requirements
     * P030; J5-040; J5-046.
     * @note Allocation failure leaves existing records unchanged.
     * Growth temporarily retains both old and new raw blocks.
     * @par ID SWU-VALUE-113
     */
    status_result value_storage::reserve(std::size_t required) noexcept
    {
      if (required > m_max_records)
      {
        return status_result::failure(error::limit_without_location(resource_limit_reason::nodes));
      }

      if (required <= m_capacity)
      {
        return status_result::success();
      }

      // An exact reservation is an upper bound, so needing more means the counting pass was wrong.
      if (m_fixed)
      {
        return status_result::failure(error::access_failure(access_error_reason::invalid_argument));
      }

      std::size_t capacity = required;
      const auto doubled = checked_add(m_capacity, m_capacity, resource_limit_reason::nodes);

      // Doubling is optional; an overflowing growth estimate can fall back to required.
      if (doubled.has_value())
      {
        const std::size_t bounded = (*doubled.value_if() > m_max_records) ? m_max_records : *doubled.value_if();

        if (bounded > capacity)
        {
          capacity = bounded;
        }
      }

      owned_block pending_identity{};

      if (identity() == nullptr)
      {
        auto token = allocate_block(m_provider, 1, 1);
        if (!token.has_value())
        {
          return status_result::failure(*token.error_if());
        }

        pending_identity = std::move(*token.value_if());
      }

      auto allocation = allocate_array(m_provider, capacity, sizeof(value_record), alignof(value_record));
      if (!allocation.has_value())
      {
        return status_result::failure(*allocation.error_if());
      }

      if (pending_identity.data() != nullptr)
      {
        m_identity = std::move(pending_identity);
      }

      for (std::size_t index{}; index < m_size; ++index)
      {
        // MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used
        // MISRA justification: Transfer each record's owned payload into a new object in the aligned block.
        // The moved-from records are destroyed before the old block is released.
        // codeql::misra_deviation_next_line(DEV-001.06)
        ::new (slot_address(*allocation.value_if(), index)) value_record(std::move(*live_record(index)));
      }

      const std::size_t retained_size = m_size;

      // All moves are non-throwing, so old object can now be destroyed safely.
      destroy_records();
      m_storage = std::move(*allocation.value_if());
      m_size = retained_size;
      m_capacity = capacity;

      return status_result::success();
    }

    /**
     * @brief Construct empty storage with an explicit record limit.
     * @par Requirements
     * P030; J5-040; J5-046.
     * @note Construction allocates nothing; a zero limit permits no records.
     * @par ID SWU-VALUE-114
     */
    value_storage::value_storage(const storage_callbacks& provider, std::size_t max_records) noexcept
        : m_provider(provider), m_max_records(max_records)
    {
    }

    /**
     * @brief Transfer storage ownership and leave the source empty.
     * @par Requirements
     * P030; J5-040.
     * @note The source retains its provider and limit and can be reused.
     * @par ID SWU-VALUE-115
     */
    value_storage::value_storage(value_storage&& other) noexcept
        : m_provider(other.m_provider), m_identity(std::move(other.m_identity)), m_storage(std::move(other.m_storage)),
          m_size(other.m_size), m_capacity(other.m_capacity), m_max_records(other.m_max_records), m_fixed(other.m_fixed)
    {
      other.m_size = 0;
      other.m_capacity = 0;
      // The emptied source owns no reservation, so it may grow again when reused.
      other.m_fixed = false;
    }

    /**
     * @brief Allocate exactly the given number of records once and stop growing.
     * @par Requirements
     * P030; J5-040; J5-046.
     * @param records Exact number of records this storage will hold.
     * @return Success, resource_limit (nodes), a provider failure, or internal_error when already reserved.
     * @note Nonzero reservations allocate a one-byte identity token before the record block.
     * Both requests use the provider and its accounting. Zero records allocate nothing.
     * Failure leaves the storage growable and unchanged.
     * @par ID SWU-VALUE-116
     */
    status_result value_storage::reserve_exact(std::size_t records) noexcept
    {
      // One reservation, made while empty, so nothing ever needs relocating.
      if (m_fixed || (m_capacity != 0) || (m_size != 0))
      {
        return status_result::failure(error::internal_fault());
      }

      if (records > m_max_records)
      {
        return status_result::failure(error::limit_without_location(resource_limit_reason::nodes));
      }

      if (records != 0)
      {
        auto token = allocate_block(m_provider, 1, 1);

        if (!token.has_value())
        {
          return status_result::failure(*token.error_if());
        }

        auto allocation = allocate_array(m_provider, records, sizeof(value_record), alignof(value_record));

        if (!allocation.has_value())
        {
          return status_result::failure(*allocation.error_if());
        }

        m_identity = std::move(*token.value_if());
        m_storage = std::move(*allocation.value_if());
        m_capacity = records;
      }

      m_fixed = true;
      return status_result::success();
    }

    /**
     * @brief Destroy each live record before releasing its raw storage.
     * @par Requirements
     * P030; J5-040.
     * @note Performs no allocation and does not recursively follow children.
     * @par ID SWU-VALUE-117
     */
    value_storage::~value_storage() noexcept
    {
      destroy_records();
    }

    /**
     * @brief Append an externally held record and return its stable index.
     * @par Requirements
     * P030; J5-028; J5-040; J5-046.
     * @param record Record that does not reside inside this storage.
     * @return Its new index or an explicit limit/allocation error.
     * @note Failure leaves existing records and the incoming record unchanged.
     * Success moves from record. Growth invalidates earlier record pointers.
     * Relationship invariants are enforced by the later document builder.
     * @par ID SWU-VALUE-118
     */
    result<value_index> value_storage::append(value_record&& record) noexcept
    {
      if (m_size >= m_max_records)
      {
        return result<value_index>::failure(error::limit_without_location(resource_limit_reason::nodes));
      }

      // The limit check proves adding one cannot overflow size_t.
      const std::size_t required = m_size + 1;
      const auto reserved = reserve(required);

      if (!reserved.has_value())
      {
        return result<value_index>::failure(*reserved.error_if());
      }

      const value_index index{identity(), m_size};

      // MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used
      // MISRA justification: Start the record's lifetime in accounted storage after checking the record limit
      // and reserving enough capacity for the new slot.
      // codeql::misra_deviation_next_line(DEV-001.05)
      ::new (slot_address(m_storage, m_size)) value_record(std::move(record));
      m_size = required;

      return result<value_index>::success(index);
    }

    /**
     * @brief Access a record after checking its index.
     * @par Requirements
     * P030; J5-039.
     * @note Returned pointers are invalidated by growth or storage destruction.
     * @par ID SWU-VALUE-119
     */
    result<value_record*> value_storage::at(value_index index) noexcept
    {
      if (!index.is_set())
      {
        return result<value_record*>::failure(error::access_failure(access_error_reason::index_out_of_range));
      }

      if (index.m_owner != identity())
      {
        return result<value_record*>::failure(error::access_failure(access_error_reason::foreign_index));
      }

      if (index.position() >= m_size)
      {
        return result<value_record*>::failure(error::access_failure(access_error_reason::index_out_of_range));
      }

      return result<value_record*>::success(live_record(index.position()));
    }

    /**
     * @brief Access a record without permitting mutation.
     * @par Requirements
     * P030; J5-039.
     * @note Returned pointers are invalidated by growth or storage destruction.
     * @par ID SWU-VALUE-120
     */
    result<const value_record*> value_storage::at(value_index index) const noexcept
    {
      if (!index.is_set())
      {
        return result<const value_record*>::failure(error::access_failure(access_error_reason::index_out_of_range));
      }

      if (index.m_owner != identity())
      {
        return result<const value_record*>::failure(error::access_failure(access_error_reason::foreign_index));
      }

      if (index.position() >= m_size)
      {
        return result<const value_record*>::failure(error::access_failure(access_error_reason::index_out_of_range));
      }

      return result<const value_record*>::success(live_record(index.position()));
    }

    /**
     * @brief Return the number of constructed records.
     * @par Requirements
     * P030; J5-028.
     * @par ID SWU-VALUE-121
     */
    std::size_t value_storage::size() const noexcept
    {
      return m_size;
    }

    /**
     * @brief Return the storage identity token without dereferencing it.
     * @par Requirements
     * P030; J5-039; J5-040; J5-046.
     * @return Token address, or nullptr when no token is owned.
     * @note Growth preserves the token; moving storage transfers its ownership.
     */
    const void* value_storage::identity() const noexcept
    {
      return m_identity.data();
    }

    /**
     * @brief Construct an index tagged with this storage's identity.
     * @par Requirements
     * P030; J5-039; J5-040.
     * @param position Record position, or the unset position sentinel.
     * @return Tagged index, or a default unset index for the sentinel.
     * @note Performs no allocation or bounds validation.
     * Unset positions yield value_index{} so every unset index has one representation.
     * Access through at() validates the index before reading a record.
     * @par ID SWU-VALUE-176
     */
    value_index value_storage::issue(std::size_t position) const noexcept
    {
      if (position == value_index::c_unset_position)
      {
        return value_index{};
      }

      return value_index{identity(), position};
    }
  } // namespace detail
} // namespace adaptive_json5
