#include "adaptive_json5/schema/schema_registry.hpp"

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_budget.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/validation_options.hpp"
#include "schema/instance_domain.hpp"
#include "schema/pod_vector.hpp"
#include "schema/schema_registry_access.hpp"
#include "schema/schema_work.hpp"
#include "schema/uri.hpp"

#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <string_view>
#include <utility>

namespace adaptive_json5
{
  namespace
  {
    using status = result<void, schema_error>;
    using detail::registry_entry;

    /**
     * @brief Address of entry slot index in the entry table.
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-075
     */
    registry_entry* slot(const owned_block& entries, std::size_t index) noexcept
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds. Address
      // computation inside an owned block from caller storage, as in the parser storage layer (DEV-002/DEV-003); the
      // offset is within the block size that was allocated.
      // codeql::misra_deviation_next_line(DEV-101.68)
      void* const address = static_cast<unsigned char*>(entries.data()) + (index * sizeof(registry_entry));
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Address computation inside an owned block from caller storage, as in the parser storage
      // layer (DEV-002/DEV-003); the offset is within the block size that was allocated.
      // codeql::misra_deviation_next_line(DEV-107.10)
      return std::launder(static_cast<registry_entry*>(address));
    }

    /**
     * @brief Translate a strict schema parse failure.
     * @par Requirements
     * P044; J5-043; J5-054; ADR 0006 decision 9.
     * @return Operational parser failures keep their category (an access failure, a
     * misconfigured call, becomes internal_error); all others are invalid_schema (syntax)
     * carrying the parser error.
     * @par ID SWU-SCHEMA-076
     */
    schema_error parse_failure(const error& failure) noexcept
    {
      const error_code code = failure.code();
      if ((code == error_code::allocation_failure) || (code == error_code::resource_limit) ||
          (code == error_code::internal_error) || (code == error_code::wrong_type_or_access))
      {
        return schema_error::from_storage(failure);
      }
      return schema_error::syntax(failure);
    }

    /**
     * @brief Reject schema text holding an unpaired surrogate (ADR 0006 O5).
     * @par Requirements
     * P044; J5-061.
     * @return success, invalid_schema (domain) with the offending range, or a storage
     * failure. Duplicate names and non-finite numbers cannot occur in strict JSON parsed
     * with reject_duplicates.
     * @par ID SWU-SCHEMA-077
     */
    status check_schema_domain(const document& values, accounted_storage& storage) noexcept
    {
      validation_budget budget{};
      budget.max_work_steps = std::numeric_limits<std::size_t>::max();
      detail::schema_work work{budget, cancellation_callback{}};
      const auto checked = detail::check_instance_domain(values, work, storage.callbacks());
      if (checked.has_value())
      {
        return checked;
      }
      const schema_error& failure = *checked.error_if();
      if (failure.code() == schema_error_code::unsupported_instance)
      {
        return status::failure(schema_error::invalid(invalid_schema_reason::domain, failure.source_range()));
      }
      return checked;
    }
  } // namespace

  /**
   * @brief Store the borrowed storage and the empty entry table.
   * @par Requirements
   * P044.
   * @par ID SWU-SCHEMA-121
   */
  schema_registry::schema_registry(accounted_storage& storage, owned_block&& entries, std::size_t capacity) noexcept
      : m_storage(&storage), m_entries(std::move(entries)), m_capacity(capacity), m_size(0U)
  {
  }

  /**
   * @brief Create an empty registry for up to capacity resources.
   * @par Requirements
   * P044; J5-046; J5-055.
   * @par ID SWU-SCHEMA-066
   */
  result<schema_registry, schema_error> schema_registry::create(accounted_storage& storage,
                                                                std::size_t capacity) noexcept
  {
    auto entries = storage.allocate_array(capacity, sizeof(registry_entry), alignof(registry_entry));
    if (!entries.has_value())
    {
      return result<schema_registry, schema_error>::failure(schema_error::from_storage(*entries.error_if()));
    }
    return result<schema_registry, schema_error>::success(
      // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
      // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result is moved out of
      // once.
      // codeql::misra_deviation_next_line(DEV-108.5)
      schema_registry{storage, std::move(*entries.value_if()), capacity});
  }

  /**
   * @brief Take over another registry's resources; other becomes empty.
   * @par Requirements
   * P044; J5-066.
   * @par ID SWU-SCHEMA-067
   */
  // MISRA rule: RULE-18-4-1 cpp/misra/exception-unfriendly-function-must-be-noexcept
  // MISRA rule: RULE-15-0-2 cpp/misra/invalid-signature-for-special-member-function
  // MISRA justification: Move construction and accessors are noexcept and leave the source empty; members have
  // default member initializers, so none is read before it is set.
  // codeql::misra_deviation_next_line(DEV-109.5)
  schema_registry::schema_registry(schema_registry&& other) noexcept
      : m_storage(other.m_storage), m_entries(std::move(other.m_entries)), m_capacity(other.m_capacity),
        m_size(other.m_size)
  {
    other.m_capacity = 0U;
    other.m_size = 0U;
  }

  /**
   * @brief Release every parsed resource and the entry table.
   * @par Requirements
   * P044; J5-040; J5-051.
   * @par ID SWU-SCHEMA-068
   */
  schema_registry::~schema_registry() noexcept
  {
    while (m_size != 0U)
    {
      // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
      // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
      // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
      // codeql::misra_deviation_next_line(DEV-101.67)
      --m_size;
      // MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used
      // MISRA justification: Placement new constructs trivially destructible or explicitly destroyed objects in owned
      // blocks from caller storage, as in the parser (DEV-001).
      // codeql::misra_deviation_next_line(DEV-112.3)
      slot(m_entries, m_size)->~registry_entry();
    }
  }

  /**
   * @brief Register schema text under an absolute base URI.
   * @par Requirements
   * P044; J5-043; J5-044; J5-054; J5-055; J5-061; ADR 0006 decision 9, O3 and O5.
   * @par ID SWU-SCHEMA-069
   */
  result<void, schema_error> schema_registry::add(std::string_view base_uri, input_view text) noexcept
  {
    if (!detail::split_uri(base_uri).has_scheme)
    {
      return status::failure(schema_error::registration(registration_reason::relative_uri));
    }
    if (m_size >= m_capacity)
    {
      return status::failure(schema_error::limit(schema_limit_reason::max_resources));
    }

    detail::pod_vector<char> normalized{m_storage->callbacks()};
    const auto resolved = detail::resolve_uri(base_uri, std::string_view{}, normalized, m_storage->callbacks());
    if (!resolved.has_value())
    {
      return resolved;
    }
    // MISRA rule: RULE-7-0-6 cpp/misra/numeric-assignment-type-mismatch
    // MISRA justification: Comparisons and arithmetic on std::uint16_t code units promote to int without changing the
    // value; the operands are unsigned values below 0x110000 and the results are used as such.
    // codeql::misra_deviation_next_line(DEV-106.19)
    const std::string_view uri = detail::text_of(normalized, 0U, normalized.size());
    // MISRA rule: RULE-9-5-1 cpp/misra/legacy-for-statements-should-be-simple
    // MISRA justification: A counting loop over a container or range whose bound is a size read through a member
    // call; the counter is only incremented by the loop statement.
    // codeql::misra_deviation_next_line(DEV-105.20)
    for (std::size_t index = 0U; index < m_size; ++index)
    {
      if (detail::schema_registry_access::uri(*slot(m_entries, index)) == uri)
      {
        return status::failure(schema_error::registration(registration_reason::duplicate_uri));
      }
    }

    parse_options options{};
    options.syntax = syntax_mode::strict_json;
    options.duplicate_names = duplicate_name_policy::reject_duplicates;
    // parse_document() requires the accounting ceiling to equal the configured byte limit.
    options.limits.max_owned_bytes = m_storage->byte_limit();
    auto parsed = parse_document(text, options, *m_storage);
    if (!parsed.has_value())
    {
      return status::failure(parse_failure(*parsed.error_if()));
    }
    const auto domain = check_schema_domain(parsed.value_if()->values, *m_storage);
    if (!domain.has_value())
    {
      return domain;
    }

    // MISRA rule: RULE-0-1-1 cpp/misra/unnecessary-write-to-local-object
    // MISRA justification: Initialized at the declaration so that no path can read an indeterminate value (as
    // DEV-007).
    // codeql::misra_deviation_next_line(DEV-111.2)
    const std::size_t uri_size = normalized.size();
    owned_block uri_block = normalized.release();
    // MISRA rule: RULE-21-6-3 cpp/misra/advanced-memory-management-used
    // MISRA justification: Placement new constructs trivially destructible or explicitly destroyed objects in owned
    // blocks from caller storage, as in the parser (DEV-001).
    // codeql::misra_deviation_next_line(DEV-112.2)
    static_cast<void>(::new (static_cast<void*>(slot(m_entries, m_size)))
                      // MISRA rule: RULE-28-6-3 cpp/misra/object-used-while-in-potentially-moved-from-state
                      // MISRA justification: The moved-from object is reassigned, reset or only destroyed; a result
                      // is moved out of once.
                      // codeql::misra_deviation_next_line(DEV-108.4)
                      registry_entry{std::move(uri_block), uri_size, std::move(*parsed.value_if())});
    // MISRA rule: RULE-8-20-1 cpp/misra/unsigned-operation-with-constant-operands-wraps
    // MISRA justification: Index, offset and count arithmetic on std::size_t is bounded by a preceding check, the
    // loop condition or a container size, so it cannot wrap; CodeQL does not follow these run-time bounds.
    // codeql::misra_deviation_next_line(DEV-101.66)
    ++m_size;
    return status::success();
  }

  /**
   * @brief Register schema text under c_default_schema_base_uri.
   * @par Requirements
   * P044; J5-055; ADR 0006 O4.
   * @par ID SWU-SCHEMA-070
   */
  result<void, schema_error> schema_registry::add(input_view text) noexcept
  {
    return add(c_default_schema_base_uri, text);
  }

  /**
   * @brief Return the number of registered resources.
   * @par Requirements
   * P044.
   * @par ID SWU-SCHEMA-071
   */
  std::size_t schema_registry::size() const noexcept
  {
    return m_size;
  }

  /**
   * @brief Return the fixed capacity.
   * @par Requirements
   * P044.
   * @par ID SWU-SCHEMA-072
   */
  std::size_t schema_registry::capacity() const noexcept
  {
    return m_capacity;
  }

  namespace detail
  {
    /**
     * @brief Borrow entry index; requires index < registry.size().
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-073
     */
    const registry_entry& schema_registry_access::entry(const schema_registry& registry, std::size_t index) noexcept
    {
      return *slot(registry.m_entries, index);
    }

    /**
     * @brief Return an entry's normalized base URI.
     * @par Requirements
     * P044.
     * @par ID SWU-SCHEMA-074
     */
    std::string_view schema_registry_access::uri(const registry_entry& entry) noexcept
    {
      if (entry.uri_size == 0U)
      {
        return std::string_view{};
      }
      // MISRA rule: RULE-8-2-6 cpp/misra/int-to-pointer-cast-prohibited
      // MISRA justification: Address computation inside an owned block from caller storage, as in the parser storage
      // layer (DEV-002/DEV-003); the offset is within the block size that was allocated.
      // codeql::misra_deviation_next_line(DEV-107.9)
      return std::string_view{static_cast<const char*>(entry.uri.data()), entry.uri_size};
    }
  } // namespace detail
} // namespace adaptive_json5
