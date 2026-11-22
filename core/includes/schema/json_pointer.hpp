#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_JSON_POINTER_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_JSON_POINTER_HPP_

#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "schema/pod_vector.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace adaptive_json5::detail
{
  /**
   * @brief Decode the next reference token of a JSON Pointer taken from a URI fragment.
   * @par Requirements
   * P044; J5-055; J5-064; RFC 6901 sections 3, 4 and 6.
   * @param rest Remaining pointer text; must start with '/'. Advanced past the token.
   * @param token Cleared, then receives the decoded token as UTF-8 bytes.
   * @return success, or unresolved_reference for a malformed `%XX` or `~` escape. `%XX`
   * escapes are decoded first, then `~1` to '/' and `~0` to '~'.
   * @par ID SWU-SCHEMA-060
   */
  [[nodiscard]] result<void, schema_error> next_pointer_token(std::string_view& rest, pod_vector<char>& token) noexcept;

  /**
   * @brief Parse a decoded token as an array index (RFC 6901 section 4).
   * @par Requirements
   * P044; J5-055.
   * @return The index, or nullopt for an empty token, a leading zero, a non-digit or overflow.
   * @par ID SWU-SCHEMA-061
   */
  [[nodiscard]] std::optional<std::size_t> pointer_index(std::string_view token) noexcept;

  /**
   * @brief Write one RFC 6901 reference token with '~' as `~0` and '/' as `~1`.
   * @par Requirements
   * P046; J5-064; RFC 6901 section 3.
   * @param token Token text as UTF-8.
   * @param out Destination buffer.
   * @param capacity Bytes available at out.
   * @return Bytes needed; nothing past capacity is written.
   * @par ID SWU-SCHEMA-062
   */
  [[nodiscard]] std::size_t escape_pointer_token(std::string_view token, char* out, std::size_t capacity) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_JSON_POINTER_HPP_ */
