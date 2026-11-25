#ifndef ADAPTIVE_JSON5_CORE_SCHEMA_FORMAT_HPP_
#define ADAPTIVE_JSON5_CORE_SCHEMA_FORMAT_HPP_

#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/value/decoded_string.hpp"
#include "schema/schema_work.hpp"

#include <cstdint>
#include <string_view>

namespace adaptive_json5::detail
{
  /**
   * @brief The `format` values that can be asserted (J5-069); none for every other name.
   * @par Requirements
   * J5-069; Draft-07 Validation section 7.3.
   */
  enum class format_kind : std::uint8_t
  {
    none,
    date_time,
    date,
    time,
    email,
    hostname,
    ipv4,
    ipv6,
    uri,
    uri_reference,
    json_pointer,
    relative_json_pointer,
    regex
  };

  /**
   * @brief Return the format_kind spelled by name, or none for an unsupported format.
   * @par Requirements
   * J5-069.
   * @par ID SWU-SCHEMA-320
   */
  [[nodiscard]] format_kind format_from_name(std::string_view name) noexcept;

  /**
   * @brief Return the format_kind spelled by a decoded name, or none.
   * @par Requirements
   * J5-069.
   * @par ID SWU-SCHEMA-321
   */
  [[nodiscard]] format_kind format_from_name(const decoded_string& name) noexcept;

  /**
   * @brief Report whether text is a valid value of format.
   * @par Requirements
   * J5-069; J5-065; Draft-07 Validation section 7.3; RFC 3339; RFC 5321; RFC 1123; RFC 2673;
   * RFC 4291; RFC 3986; RFC 6901; draft-handrews-relative-json-pointer-01; ECMA-262.
   * @note Charged to max_regex_steps, one unit per code unit; the scratch storage holds the ASCII
   * copy (and the regex program for regex) and is released before returning.
   * @return The verdict, resource_limit or a storage failure.
   * @par ID SWU-SCHEMA-322
   */
  [[nodiscard]] result<bool, schema_error> check_format(format_kind format, const decoded_string& text,
                                                        schema_work& work, const storage_callbacks& scratch) noexcept;
} // namespace adaptive_json5::detail

#endif /* ADAPTIVE_JSON5_CORE_SCHEMA_FORMAT_HPP_ */
