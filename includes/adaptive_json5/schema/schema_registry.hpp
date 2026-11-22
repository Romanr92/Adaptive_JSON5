#ifndef ADAPTIVE_JSON5_SCHEMA_SCHEMA_REGISTRY_HPP_
#define ADAPTIVE_JSON5_SCHEMA_SCHEMA_REGISTRY_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"

#include <cstddef>
#include <string_view>

namespace adaptive_json5
{
  namespace detail
  {
    struct schema_registry_access;
  } // namespace detail

  /**
   * @brief Base URI given to schema text registered without one (ADR 0006, O4).
   * @par Requirements
   * P044; J5-055.
   */
  inline constexpr std::string_view c_default_schema_base_uri{"adaptive-json5:/root.json"};

  /**
   * @brief Caller-filled set of schema resources, each a base URI and strict JSON text.
   * @par Requirements
   * P044; J5-054; J5-055; J5-061; J5-066; J5-067; ADR 0006 decision 9.
   * @note The registry never reads files or the network. Each text is parsed when it is
   * added, as strict JSON with duplicate names rejected, into documents allocated from the
   * caller's accounted storage, which must outlive the registry. The capacity is fixed at
   * creation. Compilation copies what it needs, so the registry and the texts can be
   * destroyed once compile_schema() has returned. Move-only; not thread-safe.
   */
  class [[nodiscard]] schema_registry
  {
    private:
      accounted_storage* m_storage;
      owned_block m_entries;
      std::size_t m_capacity;
      std::size_t m_size;

      /**
       * @brief Store the borrowed storage and the empty entry table.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-121
       */
      schema_registry(accounted_storage& storage, owned_block&& entries, std::size_t capacity) noexcept;

      friend struct detail::schema_registry_access;

    public:
      /**
       * @brief Create an empty registry for up to capacity resources.
       * @par Requirements
       * P044; J5-046; J5-055.
       * @return The registry, or a storage failure for its entry table.
       * @par ID SWU-SCHEMA-066
       */
      [[nodiscard]] static result<schema_registry, schema_error> create(accounted_storage& storage,
                                                                        std::size_t capacity) noexcept;

      /**
       * @brief Take over another registry's resources; other becomes empty.
       * @par Requirements
       * P044; J5-066.
       * @par ID SWU-SCHEMA-067
       */
      schema_registry(schema_registry&& other) noexcept;

      /**
       * @brief Release every parsed resource and the entry table.
       * @par Requirements
       * P044; J5-040; J5-051.
       * @par ID SWU-SCHEMA-068
       */
      ~schema_registry() noexcept;

      schema_registry(const schema_registry&) = delete;
      schema_registry& operator=(const schema_registry&) = delete;
      schema_registry& operator=(schema_registry&&) = delete;

      /**
       * @brief Register schema text under an absolute base URI.
       * @par Requirements
       * P044; J5-043; J5-044; J5-054; J5-055; J5-061; ADR 0006 decision 9, O3 and O5.
       * @return success, or (registration phase):
       * invalid_registration (relative_uri) when base_uri has no scheme;
       * invalid_registration (duplicate_uri) when the normalized URI is already registered;
       * invalid_schema (syntax) with the parser's error unchanged;
       * invalid_schema (domain) at a string or name holding an unpaired surrogate;
       * resource_limit (max_resources) when the registry is full; storage failures.
       * @note The URI is normalized as in ADR 0006 decision 8 and any fragment is dropped.
       * A failed registration leaves the registry unchanged.
       * @par ID SWU-SCHEMA-069
       */
      [[nodiscard]] result<void, schema_error> add(std::string_view base_uri, input_view text) noexcept;

      /**
       * @brief Register schema text under c_default_schema_base_uri.
       * @par Requirements
       * P044; J5-055; ADR 0006 O4.
       * @par ID SWU-SCHEMA-070
       */
      [[nodiscard]] result<void, schema_error> add(input_view text) noexcept;

      /**
       * @brief Return the number of registered resources.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-071
       */
      [[nodiscard]] std::size_t size() const noexcept;

      /**
       * @brief Return the fixed capacity.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-072
       */
      [[nodiscard]] std::size_t capacity() const noexcept;
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_SCHEMA_REGISTRY_HPP_ */
