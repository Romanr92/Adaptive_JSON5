#ifndef ADAPTIVE_JSON5_ERROR_HPP_
#define ADAPTIVE_JSON5_ERROR_HPP_

#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Stable categories for explicit library failures.
   * @par Requirements
   * P002; J5-043; issue #125.
   * @note The values are stable; new categories are only appended.
   */
  enum class error_code : std::uint8_t
  {
    syntax = 1,               // Input violates the selected grammar; see syntax_reason().
    encoding = 2,             // Input is not well-formed UTF-8.
    duplicate_policy = 3,     // reject_duplicates found a repeated decoded member name.
    wrong_type_or_access = 4, // Wrong value kind, absent member, index or argument; see access_reason().
    numeric_conversion = 5,   // No exact conversion for the destination; see numeric_reason().
    resource_limit = 6,       // A configured limit or the work budget would be exceeded; see limit_reason().
    allocation_failure = 7,   // The storage provider could not supply memory.
    internal_error = 8        // A library invariant failed; the document is rejected. Not caused by the caller.
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
   * P002; P036; P038; J5-035; J5-003; J5-032; J5-043; J5-044; J5-045; issue #96; issue #125.
   * @note Only the factories create an error, and each one fixes which details are present,
   * so a reason can never belong to a different category than code().
   * byte_offset() is zero-based in the original input; without has_byte_offset() it is zero.
   * source_range(), when present, covers the offending token in the original input;
   * at_range() and syntax_at_range() set it, and every other factory leaves it empty.
   * duplicate_names() is present only for duplicate_member(); byte_offset() is then the
   * repeated name's start and source_range() stays empty.
   * Each reason accessor returns a value only for its own category: syntax_reason() for
   * syntax, numeric_reason() for numeric_conversion, limit_reason() for resource_limit and
   * access_reason() for wrong_type_or_access.
   * Limit, access and internal failures use limit_at(), limit_without_location(),
   * access_failure() and internal_fault(), so they always carry their reason; at_offset(),
   * at_range() and without_location() are for the other categories.
   * Error metadata contains no source pointers and performs no allocation.
   */
  struct [[nodiscard]] error
  {
    private:
      // Members are ordered by size, so the struct has no internal padding on common targets.
      std::size_t m_byte_offset;
      source_span m_range;           // source_range, or the repeated name of a duplicate
      source_span m_first_duplicate; // only for duplicate_policy
      error_code m_code;
      std::uint8_t m_reason; // read according to m_code
      std::uint8_t m_flags;  // which details are present, see the c_has_ constants

      // Bits of m_flags.
      static constexpr std::uint32_t c_has_offset = 1U;
      static constexpr std::uint32_t c_has_range = 2U;
      static constexpr std::uint32_t c_has_reason = 4U;
      static constexpr std::uint32_t c_has_duplicate = 8U;

      /**
       * @brief Store one failure exactly as a factory describes it.
       * @par Requirements
       * J5-043; J5-045; issue #125.
       * @param code Failure category.
       * @param byte_offset Primary zero-based location, or zero without c_has_offset.
       * @param flags Combination of the c_has_ constants for the details that are present.
       * @param range Offending token, or the repeated name of a duplicate.
       * @param reason Category-specific reason, read only with c_has_reason.
       * @param first_duplicate First occurrence of a duplicate name.
       * @note Private, so only the factories decide which details belong together.
       * @par ID SWU-API-023
       */
      constexpr error(error_code code, std::size_t byte_offset, std::uint32_t flags, source_span range = {},
                      std::uint8_t reason = 0U, source_span first_duplicate = {}) noexcept
          : m_byte_offset(byte_offset), m_range(range), m_first_duplicate(first_duplicate), m_code(code),
            m_reason(reason), m_flags(static_cast<std::uint8_t>(flags))
      {
      }

      /**
       * @brief Test whether a detail is present.
       * @par Requirements
       * J5-043; issue #125.
       * @param flag One of the c_has_ constants.
       * @return True when the flag is set.
       * @par ID SWU-API-024
       */
      [[nodiscard]] constexpr bool has(std::uint32_t flag) const noexcept
      {
        return (m_flags & flag) != 0U;
      }

      /**
       * @brief Return the stored reason when it belongs to the requested category.
       * @par Requirements
       * J5-043; issue #96; issue #125.
       * @param code Category whose reason the caller wants.
       * @return The raw reason, or empty for another category or an error without a reason.
       * @par ID SWU-API-025
       */
      [[nodiscard]] constexpr std::optional<std::uint8_t> reason_for(error_code code) const noexcept
      {
        if ((m_code != code) || !has(c_has_reason))
        {
          return std::nullopt;
        }
        return m_reason;
      }

    public:
      /**
       * @brief Return the failure category.
       * @par Requirements
       * P002; J5-043.
       * @par ID SWU-API-026
       */
      [[nodiscard]] constexpr error_code code() const noexcept
      {
        return m_code;
      }

      /**
       * @brief Return the primary zero-based location in the original input.
       * @par Requirements
       * J5-043; J5-044.
       * @return The location, or zero when has_byte_offset() is false.
       * @par ID SWU-API-027
       */
      [[nodiscard]] constexpr std::size_t byte_offset() const noexcept
      {
        return m_byte_offset;
      }

      /**
       * @brief Report whether byte_offset() is a real location.
       * @par Requirements
       * J5-043; J5-044; J5-045.
       * @par ID SWU-API-028
       */
      [[nodiscard]] constexpr bool has_byte_offset() const noexcept
      {
        return has(c_has_offset);
      }

      /**
       * @brief Return the extent of the offending token, when known.
       * @par Requirements
       * P039; J5-043; J5-044.
       * @return The range set by at_range() or syntax_at_range(); empty otherwise.
       * @par ID SWU-API-029
       */
      [[nodiscard]] constexpr std::optional<source_span> source_range() const noexcept
      {
        if (!has(c_has_range))
        {
          return std::nullopt;
        }
        return m_range;
      }

      /**
       * @brief Return both name spans of a rejected duplicate.
       * @par Requirements
       * P036; J5-032; J5-044; J5-045.
       * @return The first and repeated name-token spans for duplicate_member(); empty otherwise.
       * @par ID SWU-API-030
       */
      [[nodiscard]] constexpr std::optional<duplicate_names_locations> duplicate_names() const noexcept
      {
        if (!has(c_has_duplicate))
        {
          return std::nullopt;
        }
        return duplicate_names_locations{m_first_duplicate, m_range};
      }

      /**
       * @brief Return why the input violates the grammar.
       * @par Requirements
       * J5-043; J5-044; issue #96.
       * @return The reason for error_code::syntax created by syntax_at() or syntax_at_range();
       * empty otherwise.
       * @par ID SWU-API-031
       */
      [[nodiscard]] constexpr std::optional<syntax_error_reason> syntax_reason() const noexcept
      {
        const auto reason = reason_for(error_code::syntax);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<syntax_error_reason>(*reason);
      }

      /**
       * @brief Return why a numeric conversion failed.
       * @par Requirements
       * P038; J5-035; J5-038; J5-043.
       * @return The reason for error_code::numeric_conversion; empty otherwise.
       * @par ID SWU-API-032
       */
      [[nodiscard]] constexpr std::optional<numeric_conversion_reason> numeric_reason() const noexcept
      {
        const auto reason = reason_for(error_code::numeric_conversion);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<numeric_conversion_reason>(*reason);
      }

      /**
       * @brief Return which limit would have been exceeded.
       * @par Requirements
       * J5-043; J5-047; J5-048; J5-049; issue #125.
       * @return The reason for error_code::resource_limit; empty otherwise.
       * @par ID SWU-API-033
       */
      [[nodiscard]] constexpr std::optional<resource_limit_reason> limit_reason() const noexcept
      {
        const auto reason = reason_for(error_code::resource_limit);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<resource_limit_reason>(*reason);
      }

      /**
       * @brief Return why a value access or argument was rejected.
       * @par Requirements
       * J5-031; J5-039; J5-043; issue #125.
       * @return The reason for error_code::wrong_type_or_access; empty otherwise.
       * @par ID SWU-API-034
       */
      [[nodiscard]] constexpr std::optional<access_error_reason> access_reason() const noexcept
      {
        const auto reason = reason_for(error_code::wrong_type_or_access);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<access_error_reason>(*reason);
      }

      /**
       * @brief Copy this failure with a new primary location.
       * @par Requirements
       * J5-043; J5-044; issue #125.
       * @param byte_offset New zero-based location in the original input.
       * @return The same failure located at byte_offset.
       * @note Code, reason, source range and duplicate spans are kept, so a reason survives
       * when a caller reports an inner failure at its own position. The source range is not
       * moved. Performs no allocation.
       * @par ID SWU-API-039
       */
      [[nodiscard]] constexpr error relocated(std::size_t byte_offset) const noexcept
      {
        error moved{*this};
        moved.m_byte_offset = byte_offset;
        moved.m_flags = static_cast<std::uint8_t>(moved.m_flags | c_has_offset);
        return moved;
      }

      /**
       * @brief Copy this failure without any input location.
       * @par Requirements
       * J5-043; J5-045; issue #125.
       * @return The same failure with neither a byte offset nor a source range.
       * @note Code, reason and duplicate spans are kept. Used where an inner location has no
       * meaning for the caller, e.g. an allocation failure while copying a number.
       * Performs no allocation.
       * @par ID SWU-API-040
       */
      [[nodiscard]] constexpr error unlocated() const noexcept
      {
        error moved{*this};
        moved.m_byte_offset = 0U;
        moved.m_flags = static_cast<std::uint8_t>(moved.m_flags & ~(c_has_offset | c_has_range));
        return moved;
      }

      /**
       * @brief Construct a failure with one original byte location.
       * @par Requirements
       * P002; J5-043; J5-044; J5-045.
       * @note For syntax, encoding and allocation failures; no reason or duplicate spans are set.
       * Limit, access and internal failures use their own factories.
       * @par ID SWU-API-001
       */
      [[nodiscard]] static constexpr error at_offset(error_code code, std::size_t byte_offset) noexcept
      {
        return error{code, byte_offset, c_has_offset};
      }

      /**
       * @brief Construct a failure with a primary location and the offending token's range.
       * @par Requirements
       * P039; J5-043; J5-044; J5-045.
       * @param code Failure category.
       * @param byte_offset Primary zero-based location, e.g. where a closing delimiter was expected.
       * @param range Original-input extent of the offending token.
       * @note For encoding failures with a known token extent; no reason or duplicate spans are
       * set. Performs no allocation.
       * @par ID SWU-API-002
       */
      [[nodiscard]] static constexpr error at_range(error_code code, std::size_t byte_offset,
                                                    source_span range) noexcept
      {
        return error{code, byte_offset, c_has_offset | c_has_range, range};
      }

      /**
       * @brief Construct a failure without an available source location.
       * @par Requirements
       * P002; J5-043; J5-045.
       * @note For syntax, encoding and allocation failures; no reason or duplicate spans are set.
       * Limit, access and internal failures use their own factories.
       * @par ID SWU-API-003
       */
      [[nodiscard]] static constexpr error without_location(error_code code) noexcept
      {
        return error{code, 0U, 0U};
      }

      /**
       * @brief Construct duplicate rejection with both original name spans.
       * @par Requirements
       * P036; J5-032; J5-043; J5-044; J5-045.
       * @param first First matching name in the same object.
       * @param repeated Later name rejected by the selected policy.
       * @note The caller supplies validated original-input spans.
       * The primary location points to the repeated occurrence.
       * @par ID SWU-API-004
       */
      [[nodiscard]] static constexpr error duplicate_member(source_span first, source_span repeated) noexcept
      {
        return error{
          error_code::duplicate_policy, repeated.byte_offset, c_has_offset | c_has_duplicate, repeated, 0U, first};
      }

      /**
       * @brief Construct an unlocated numeric conversion failure.
       * @par Requirements
       * P038; J5-003; J5-035; J5-038; J5-043; J5-045.
       * @param reason Specific reason the requested conversion failed.
       * @return Numeric-conversion error containing the supplied reason.
       * @note Conversion may inspect numbers without source metadata.
       * Duplicate-name metadata is absent. Performs no allocation.
       * @par ID SWU-API-005
       */
      [[nodiscard]] static constexpr error numeric_failure(numeric_conversion_reason reason) noexcept
      {
        return error{error_code::numeric_conversion, 0U, c_has_reason, source_span{},
                     static_cast<std::uint8_t>(reason)};
      }

      /**
       * @brief Construct a located syntax failure with its specific reason.
       * @par Requirements
       * J5-043; J5-044; issue #96.
       * @param byte_offset Zero-based location of the offending character or expected token.
       * @param reason Why the input violates the selected grammar.
       * @return Syntax error carrying the reason; range and duplicate spans are absent.
       * @note Performs no allocation.
       * @par ID SWU-API-006
       */
      [[nodiscard]] static constexpr error syntax_at(std::size_t byte_offset, syntax_error_reason reason) noexcept
      {
        // The code is fixed, so a reason can never be attached to a non-syntax category.
        return error{error_code::syntax, byte_offset, c_has_offset | c_has_reason, source_span{},
                     static_cast<std::uint8_t>(reason)};
      }

      /**
       * @brief Construct a syntax failure with its reason and the offending token's range.
       * @par Requirements
       * J5-043; J5-044; issue #96.
       * @param byte_offset Primary zero-based location, e.g. where a closing delimiter was expected.
       * @param range Original-input extent of the offending token.
       * @param reason Why the input violates the selected grammar.
       * @return Syntax error carrying both the range and the reason.
       * @note Performs no allocation.
       * @par ID SWU-API-007
       */
      [[nodiscard]] static constexpr error syntax_at_range(std::size_t byte_offset, source_span range,
                                                           syntax_error_reason reason) noexcept
      {
        return error{error_code::syntax, byte_offset, c_has_offset | c_has_range | c_has_reason, range,
                     static_cast<std::uint8_t>(reason)};
      }

      /**
       * @brief Construct a located resource-limit failure.
       * @par Requirements
       * J5-043; J5-044; J5-047; J5-048; J5-049; issue #125.
       * @param byte_offset Zero-based start of the token that would exceed the limit.
       * @param reason Which limit would have been exceeded.
       * @return Resource-limit error carrying the location and the reason.
       * @note Performs no allocation.
       * @par ID SWU-API-035
       */
      [[nodiscard]] static constexpr error limit_at(std::size_t byte_offset, resource_limit_reason reason) noexcept
      {
        return error{error_code::resource_limit, byte_offset, c_has_offset | c_has_reason, source_span{},
                     static_cast<std::uint8_t>(reason)};
      }

      /**
       * @brief Construct a resource-limit failure that has no input location.
       * @par Requirements
       * J5-043; J5-045; J5-047; J5-048; issue #125.
       * @param reason Which limit would have been exceeded.
       * @return Resource-limit error carrying the reason.
       * @note For limits not tied to one token, e.g. the input size or owned bytes.
       * Performs no allocation.
       * @par ID SWU-API-036
       */
      [[nodiscard]] static constexpr error limit_without_location(resource_limit_reason reason) noexcept
      {
        return error{error_code::resource_limit, 0U, c_has_reason, source_span{}, static_cast<std::uint8_t>(reason)};
      }

      /**
       * @brief Construct a rejected value access or argument.
       * @par Requirements
       * J5-031; J5-039; J5-043; J5-045; issue #125.
       * @param reason Why the access or argument was rejected.
       * @return Wrong-type-or-access error carrying the reason.
       * @note Performs no allocation.
       * @par ID SWU-API-037
       */
      [[nodiscard]] static constexpr error access_failure(access_error_reason reason) noexcept
      {
        return error{error_code::wrong_type_or_access, 0U, c_has_reason, source_span{},
                     static_cast<std::uint8_t>(reason)};
      }

      /**
       * @brief Construct a failed library invariant.
       * @par Requirements
       * J5-043; J5-045; issue #125.
       * @return Internal error without location or reason.
       * @note Reports a library defect or a corrupted document or accounting state, never a
       * caller error. The operation is rejected. Performs no allocation.
       * @par ID SWU-API-038
       */
      [[nodiscard]] static constexpr error internal_fault() noexcept
      {
        return error{error_code::internal_error, 0U, 0U};
      }
  };

} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_ERROR_HPP_ */
