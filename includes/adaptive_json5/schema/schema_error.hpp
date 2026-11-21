#ifndef ADAPTIVE_JSON5_SCHEMA_SCHEMA_ERROR_HPP_
#define ADAPTIVE_JSON5_SCHEMA_SCHEMA_ERROR_HPP_

#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"

#include <cstdint>
#include <optional>

namespace adaptive_json5
{
  /**
   * @brief Operational outcome categories of schema compilation and validation.
   * @par Requirements
   * P043; J5-043; J5-063; ADR 0006 decision 3.
   * @note A failed schema assertion is not an error; it is reported as an invalid instance in
   * the validation report. resource_limit, allocation_failure and internal_error reuse the
   * parser's error_code names and meanings.
   */
  enum class schema_error_code : std::uint8_t
  {
    invalid_schema = 1,           // Compile: see invalid_schema_reason().
    unresolved_reference = 2,     // Compile: a `$ref` names no known resource or location.
    unsupported_dialect = 3,      // Compile: root `$schema` names another dialect.
    unsupported_option = 4,       // Compile: a requested assertion mode is not supported.
    unsupported_instance = 5,     // Validate: see instance_reason().
    nonprogressing_reference = 6, // Both: a `$ref` cycle that makes no progress.
    cancelled = 7,                // Validate: the cancellation callback asked to stop.
    resource_limit = 8,           // Both: see limit_reason() or parser_error().
    allocation_failure = 9,       // Both: the storage provider could not supply memory.
    internal_error = 10,          // All phases: a library invariant failed.
    invalid_registration = 11     // Registration: see registration_reason().
  };

  /**
   * @brief Phase in which a schema error is reported.
   * @par Requirements
   * P043; J5-063; ADR 0006 decision 3.
   */
  enum class schema_phase : std::uint8_t
  {
    registration, // Adding a schema resource to a registry.
    compile,
    validate
  };

  /**
   * @brief Sub-reason of schema_error_code::invalid_schema.
   * @par Requirements
   * P043; J5-054; J5-060; ADR 0006 decision 3.
   */
  enum class invalid_schema_reason : std::uint8_t
  {
    syntax = 1,              // The schema text failed strict JSON parsing; see parser_error().
    keyword_shape = 2,       // A keyword value has the wrong type or shape.
    invalid_pattern = 3,     // A regular expression is malformed.
    unsupported_pattern = 4, // A regular expression uses an unsupported construct.
    domain = 5               // Schema text holds a value outside the JSON domain (an unpaired surrogate).
  };

  /**
   * @brief Sub-reason of schema_error_code::invalid_registration.
   * @par Requirements
   * P044; J5-055; ADR 0006 decision 9.
   */
  enum class registration_reason : std::uint8_t
  {
    duplicate_uri = 1, // The normalized base URI is already registered.
    relative_uri = 2   // The base URI is not absolute.
  };

  /**
   * @brief Sub-reason of schema_error_code::unsupported_instance.
   * @par Requirements
   * P043; J5-059; J5-061; ADR 0006 decision 6.
   */
  enum class unsupported_instance_reason : std::uint8_t
  {
    duplicate_name = 1,    // An object repeats a decoded member name.
    non_finite_number = 2, // NaN or Infinity.
    unpaired_surrogate = 3 // A string or member name holds an unpaired UTF-16 surrogate.
  };

  /**
   * @brief Sub-reason of schema_error_code::resource_limit naming the exceeded budget field.
   * @par Requirements
   * P043; J5-047; J5-049; J5-065; ADR 0006 decision 5.
   */
  enum class schema_limit_reason : std::uint8_t
  {
    max_depth = 1,
    max_reference_steps = 2,
    max_branch_evaluations = 3,
    max_equality_steps = 4,
    max_work_steps = 5,
    max_schema_nodes = 6,
    max_compile_steps = 7
  };

  /**
   * @brief Report whether an error code can be returned by a phase.
   * @par Requirements
   * P043; J5-063; ADR 0006 decision 3.
   * @return The reason/phase table of ADR 0006 decision 3; false for an unknown code.
   * @par ID SWU-SCHEMA-002
   */
  [[nodiscard]] constexpr bool occurs_in(schema_error_code code, schema_phase phase) noexcept
  {
    switch (code)
    {
    case schema_error_code::invalid_registration:
      return phase == schema_phase::registration;
    case schema_error_code::invalid_schema:
      return phase != schema_phase::validate;
    case schema_error_code::unresolved_reference:
    case schema_error_code::unsupported_dialect:
    case schema_error_code::unsupported_option:
      return phase == schema_phase::compile;
    case schema_error_code::unsupported_instance:
    case schema_error_code::cancelled:
      return phase == schema_phase::validate;
    case schema_error_code::nonprogressing_reference:
      return phase != schema_phase::registration;
    case schema_error_code::resource_limit:
    case schema_error_code::allocation_failure:
    case schema_error_code::internal_error:
      return true;
    default:
      return false;
    }
  }

  /**
   * @brief Allocation-free schema compilation or validation failure.
   * @par Requirements
   * P043; J5-043; J5-045; J5-063; J5-064; ADR 0006 decision 3.
   * @note Carries a code, at most one sub-reason belonging to that code, an optional source
   * range (an instance value or member name, or a schema location) and, for failures that
   * come from the parser or storage, the parser error unchanged. Construction never
   * allocates.
   */
  class [[nodiscard]] schema_error
  {
    private:
      std::optional<error> m_parser_error;
      source_span m_range;
      schema_error_code m_code;
      std::uint8_t m_reason;
      bool m_has_reason;
      bool m_has_range;

      constexpr schema_error(schema_error_code code, std::optional<std::uint8_t> reason,
                             std::optional<source_span> range, std::optional<error> parser_error) noexcept
          : m_parser_error(parser_error), m_range(range.value_or(source_span{})), m_code(code),
            m_reason(reason.value_or(std::uint8_t{0U})), m_has_reason(reason.has_value()),
            m_has_range(range.has_value())
      {
      }

      /**
       * @brief Read the sub-reason when it belongs to code.
       * @par Requirements
       * P043; J5-043.
       * @par ID SWU-SCHEMA-003
       */
      [[nodiscard]] constexpr std::optional<std::uint8_t> reason_for(schema_error_code code) const noexcept
      {
        if ((m_code != code) || !m_has_reason)
        {
          return std::nullopt;
        }
        return m_reason;
      }

    public:
      /**
       * @brief Return the outcome category.
       * @par Requirements
       * P043; J5-063.
       * @par ID SWU-SCHEMA-004
       */
      [[nodiscard]] constexpr schema_error_code code() const noexcept
      {
        return m_code;
      }

      /**
       * @brief Return the invalid_schema sub-reason, or nullopt for any other code.
       * @par Requirements
       * P043; J5-054; J5-060.
       * @par ID SWU-SCHEMA-005
       */
      [[nodiscard]] constexpr std::optional<adaptive_json5::invalid_schema_reason>
      invalid_schema_reason() const noexcept
      {
        const auto reason = reason_for(schema_error_code::invalid_schema);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<adaptive_json5::invalid_schema_reason>(*reason);
      }

      /**
       * @brief Return the unsupported_instance sub-reason, or nullopt for any other code.
       * @par Requirements
       * P043; J5-059; J5-061.
       * @par ID SWU-SCHEMA-006
       */
      [[nodiscard]] constexpr std::optional<unsupported_instance_reason> instance_reason() const noexcept
      {
        const auto reason = reason_for(schema_error_code::unsupported_instance);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<unsupported_instance_reason>(*reason);
      }

      /**
       * @brief Return the invalid_registration sub-reason, or nullopt for any other code.
       * @par Requirements
       * P044; J5-055.
       * @par ID SWU-SCHEMA-035
       */
      [[nodiscard]] constexpr std::optional<adaptive_json5::registration_reason> registration_reason() const noexcept
      {
        const auto reason = reason_for(schema_error_code::invalid_registration);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<adaptive_json5::registration_reason>(*reason);
      }

      /**
       * @brief Return the exceeded schema budget field, or nullopt.
       * @par Requirements
       * P043; J5-047; J5-065.
       * @note A resource_limit raised by a storage byte ceiling has no schema reason;
       * parser_error() then holds the storage error (owned_bytes).
       * @par ID SWU-SCHEMA-007
       */
      [[nodiscard]] constexpr std::optional<schema_limit_reason> limit_reason() const noexcept
      {
        const auto reason = reason_for(schema_error_code::resource_limit);
        if (!reason.has_value())
        {
          return std::nullopt;
        }
        return static_cast<schema_limit_reason>(*reason);
      }

      /**
       * @brief Return the source range of the offending value, name or schema location.
       * @par Requirements
       * P043; J5-044; J5-064.
       * @note Offsets refer to the text the range was taken from (the instance source or
       * the schema source); nullopt when the location is unknown.
       * @par ID SWU-SCHEMA-008
       */
      [[nodiscard]] constexpr std::optional<source_span> source_range() const noexcept
      {
        if (!m_has_range)
        {
          return std::nullopt;
        }
        return m_range;
      }

      /**
       * @brief Return the unchanged parser or storage error behind this failure, if any.
       * @par Requirements
       * P043; J5-043; J5-044.
       * @par ID SWU-SCHEMA-009
       */
      [[nodiscard]] constexpr const std::optional<error>& parser_error() const noexcept
      {
        return m_parser_error;
      }

      /**
       * @brief Build a failure that has no sub-reason.
       * @par Requirements
       * P043; J5-045; J5-063.
       * @note Intended for unresolved_reference, unsupported_dialect, unsupported_option,
       * nonprogressing_reference, cancelled, allocation_failure and internal_error.
       * @par ID SWU-SCHEMA-010
       */
      [[nodiscard]] static constexpr schema_error of(schema_error_code code,
                                                     std::optional<source_span> range = std::nullopt) noexcept
      {
        return schema_error{code, std::nullopt, range, std::nullopt};
      }

      /**
       * @brief Build an invalid_schema failure.
       * @par Requirements
       * P043; J5-054; J5-060; J5-064.
       * @par ID SWU-SCHEMA-011
       */
      [[nodiscard]] static constexpr schema_error invalid(adaptive_json5::invalid_schema_reason reason,
                                                          std::optional<source_span> range = std::nullopt) noexcept
      {
        return schema_error{schema_error_code::invalid_schema, static_cast<std::uint8_t>(reason), range, std::nullopt};
      }

      /**
       * @brief Build an invalid_schema syntax failure carrying the parser error unchanged.
       * @par Requirements
       * P043; J5-043; J5-044; J5-054.
       * @par ID SWU-SCHEMA-012
       */
      [[nodiscard]] static constexpr schema_error syntax(const error& parser_failure) noexcept
      {
        return schema_error{schema_error_code::invalid_schema,
                            static_cast<std::uint8_t>(adaptive_json5::invalid_schema_reason::syntax),
                            parser_failure.source_range(), parser_failure};
      }

      /**
       * @brief Build an unsupported_instance failure at the offending value or name.
       * @par Requirements
       * P043; J5-059; J5-061; J5-064.
       * @par ID SWU-SCHEMA-013
       */
      [[nodiscard]] static constexpr schema_error unsupported(unsupported_instance_reason reason,
                                                              std::optional<source_span> range) noexcept
      {
        return schema_error{schema_error_code::unsupported_instance, static_cast<std::uint8_t>(reason), range,
                            std::nullopt};
      }

      /**
       * @brief Build an invalid_registration failure.
       * @par Requirements
       * P044; J5-055; ADR 0006 decision 9.
       * @par ID SWU-SCHEMA-036
       */
      [[nodiscard]] static constexpr schema_error registration(adaptive_json5::registration_reason reason) noexcept
      {
        return schema_error{schema_error_code::invalid_registration, static_cast<std::uint8_t>(reason), std::nullopt,
                            std::nullopt};
      }

      /**
       * @brief Build a resource_limit failure naming the exceeded budget field.
       * @par Requirements
       * P043; J5-047; J5-065.
       * @par ID SWU-SCHEMA-014
       */
      [[nodiscard]] static constexpr schema_error limit(schema_limit_reason reason) noexcept
      {
        return schema_error{schema_error_code::resource_limit, static_cast<std::uint8_t>(reason), std::nullopt,
                            std::nullopt};
      }

      /**
       * @brief Translate a storage or document access failure.
       * @par Requirements
       * P043; J5-043; J5-046; J5-051.
       * @return allocation_failure or resource_limit (without a schema reason) for the same
       * parser codes, otherwise internal_error. The parser error is kept unchanged.
       * @par ID SWU-SCHEMA-015
       */
      [[nodiscard]] static constexpr schema_error from_storage(const error& storage_failure) noexcept
      {
        schema_error_code code = schema_error_code::internal_error;
        if (storage_failure.code() == error_code::allocation_failure)
        {
          code = schema_error_code::allocation_failure;
        }
        else if (storage_failure.code() == error_code::resource_limit)
        {
          code = schema_error_code::resource_limit;
        }
        else
        {
          code = schema_error_code::internal_error;
        }
        return schema_error{code, std::nullopt, std::nullopt, storage_failure};
      }
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_SCHEMA_ERROR_HPP_ */
