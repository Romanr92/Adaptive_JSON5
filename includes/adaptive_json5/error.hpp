#ifndef ADAPTIVE_JSON5_ERROR_HPP_
#define ADAPTIVE_JSON5_ERROR_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"

#include <cstddef>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Stable categories for explicit library failures.
   * @par Requirements
   * P002; J5-043.
   */
  enum class error_code
  {
    syntax = 1,
    encoding = 2,
    duplicate_policy = 3,
    wrong_type_or_access = 4,
    numeric_conversion = 5,
    resource_limit = 6,
    allocation_failure = 7
  };

  /**
   * @brief Identify the first and repeated occurrences of one decoded name.
   * @par Requirements
   * P036; J5-032; J5-044; J5-045.
   * @note Both spans describe member-name tokens in the original input.
   * Quoted-name spans include their quotation marks.
   * Contains no source pointers and performs no allocation.
   */
  struct [[nodiscard]] duplicate_names_locations
  {
      source_span first{};
      source_span repeated{};
  };

  /**
   * @brief Store explicit failure information without allocation.
   * @par Requirements
   * P002; P036; P038; J5-035; J5-003; J5-032; J5-043; J5-044; J5-045.
   * @note byte_offset is zero-based in the original input.
   * When has_byte_offset is false, byte_offset is unused and set to zero.
   * duplicate_member() sets byte_offset to the repeated member name's start
   * and retains both original name-token spans in duplicate_names.
   * at_offset() and without_location() leave duplicate_names empty.
   * Error metadata contains no source pointers and performs no allocation.
   */
  struct [[nodiscard]] error
  {
      error_code code;
      std::size_t byte_offset;
      bool has_byte_offset;

      // Keep both locations available even when diagnostic allocation is impossible.
      std::optional<duplicate_names_locations> duplicate_names{};

      // Preserve te braod cattegory while exposing the conversion failure.
      std::optional<numeric_conversion_reason> numeric_reason{};

      /**
       * @brief Construct a failure with one original byte location.
       * @par Requirements
       * P002; J5-043; J5-044; J5-045.
       * @note Duplicate-name metadata is absent.
       */
      [[nodiscard]] static constexpr error at_offset(error_code code, std::size_t byte_offset) noexcept
      {
        return error{code, byte_offset, true};
      }

      /**
       * @brief Construct a failure without an available source location.
       * @par Requirements
       * P002; J5-043; J5-045.
       * @note Duplicate-name metadata is absent.
       */
      [[nodiscard]] static constexpr error without_location(error_code code) noexcept
      {
        return error{code, 0, false};
      }

      /**
       * @brief Construct duplicate rejection with both original name spans.
       * @par Requirements
       * P036; J5-032; J5-043; J5-044; J5-045.
       * @param first First matching name in the same object.
       * @param repeated Later name rejected by the selected policy.
       * @note The caller supplies validated original-input spans.
       * The primary location points to the repeated occurrence.
       */
      [[nodiscard]] static constexpr error duplicate_member(source_span first, source_span repeated) noexcept
      {
        return error{error_code::duplicate_policy, repeated.byte_offset, true,
                     duplicate_names_locations{first, repeated}};
      }

      /**
       * @brief Construct an unlocated numeric conversion failure.
       * @par Requirements
       * P038; J5-003; J5-035; J5-038; J5-043; J5-045.
       * @param reason Specific reason the requested conversion failed.
       * @return Numeric-conversion error containing the supplied reason.
       * @note Conversion may inspect numbers without source metadata.
       * Duplicate-name metadata is absent. Performs no allocation.
       */
      [[nodiscard]] static constexpr error numeric_failure(numeric_conversion_reason reason) noexcept
      {
        return error{error_code::numeric_conversion, 0, false, std::nullopt, reason};
      }
  };

} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_ERROR_HPP_ */
