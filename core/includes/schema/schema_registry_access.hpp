#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_REGISTRY_ACCESS_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_REGISTRY_ACCESS_HPP_

#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema/schema_registry.hpp"

#include <cstddef>
#include <string_view>

namespace adaptive_json5::detail
{
  /**
   * @brief One registered resource: its normalized base URI and parsed document.
   * @par Requirements
   * P044; J5-055.
   */
  struct registry_entry
  {
      owned_block uri;
      std::size_t uri_size;
      parsed_document parsed;
  };

  /**
   * @brief Read access to registry entries for compilation.
   * @par Requirements
   * P044; J5-055.
   */
  struct schema_registry_access
  {
      /**
       * @brief Borrow entry index; requires index < registry.size().
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-073
       */
      [[nodiscard]] static const registry_entry& entry(const schema_registry& registry, std::size_t index) noexcept;

      /**
       * @brief Return an entry's normalized base URI.
       * @par Requirements
       * P044.
       * @par ID SWU-SCHEMA-074
       */
      [[nodiscard]] static std::string_view uri(const registry_entry& entry) noexcept;
  };
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_SCHEMA_REGISTRY_ACCESS_HPP_ */
