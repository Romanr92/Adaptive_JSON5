#ifndef ADAPTIVE_JSON5_RESOURCE_LIMIT_REASON_HPP_
#define ADAPTIVE_JSON5_RESOURCE_LIMIT_REASON_HPP_

#include <cstdint>

namespace adaptive_json5
{
  /**
   * @brief Name the limit that an operation would have exceeded.
   * @par Requirements
   * J5-043; J5-047; J5-048; J5-049; issue #125.
   * @par Reasons
   * - input: The input is longer than resource_limits::max_input_bytes.
   * - depth: Nesting would exceed resource_limits::max_depth.
   * - nodes: The document would hold more values than resource_limits::max_nodes.
   * - members: The document would hold more members than resource_limits::max_members.
   * - string: One string or name would exceed resource_limits::max_string_units.
   * - total_string: All strings and names together would exceed
   *   resource_limits::max_total_string_units.
   * - number_bytes: One number spelling would exceed resource_limits::max_number_bytes.
   * - exponent_digits: One exponent would exceed resource_limits::max_exponent_digits.
   * - owned_bytes: Outstanding allocations would exceed resource_limits::max_owned_bytes.
   * - diagnostics: The warnings would exceed resource_limits::max_diagnostics.
   * - work: The operation would exceed resource_limits::max_work_units.
   * - output_capacity: A caller-supplied output buffer is too small for the result.
   * - conversion_size: A number is too long for an exact conversion or comparison.
   * @note The reason refines error_code::resource_limit; it never replaces the error code.
   * Callers may switch on it to decide which limit to raise.
   */
  enum class resource_limit_reason : std::uint8_t
  {
    input,
    depth,
    nodes,
    members,
    string,
    total_string,
    number_bytes,
    exponent_digits,
    owned_bytes,
    diagnostics,
    work,
    output_capacity,
    conversion_size
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_RESOURCE_LIMIT_REASON_HPP_ */
