#ifndef ADAPTIVE_JSON5_SCHEMA_VALIDATION_REPORT_HPP_
#define ADAPTIVE_JSON5_SCHEMA_VALIDATION_REPORT_HPP_

#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/schema/schema_keyword.hpp"

#include <cstddef>

namespace adaptive_json5
{
  namespace detail
  {
    struct validation_report_access;
  } // namespace detail

  /**
   * @brief One assertion failure, as a fixed-size record.
   * @par Requirements
   * P046; J5-044; J5-063; J5-064; ADR 0006 decision 13, O6.
   * @note keyword is the failed keyword (false_schema for the boolean schema false).
   * instance_position is the failing value's position in the instance document;
   * schema_node is the compiled node holding the keyword, in schema_resource. Source spans
   * refer to the parsed instance text and the registered schema text; they stay meaningful
   * only while the caller keeps those texts. JSON Pointers are formatted on request.
   */
  struct [[nodiscard]] schema_diagnostic
  {
      schema_keyword keyword{schema_keyword::none};
      std::size_t instance_position{};
      source_span instance_span{};
      bool has_instance_span{};
      std::size_t schema_node{};
      std::size_t schema_resource{};
      source_span schema_span{};
      bool has_schema_span{};
  };

  /**
   * @brief Verdict of one validation and its diagnostics.
   * @par Requirements
   * P046; J5-063; ADR 0006 decisions 3, 12 and 13.
   * @note valid() is the verdict; an invalid instance is a normal outcome, not an error.
   * Fail-fast reports exactly one diagnostic when invalid. Collect-all keeps at most
   * validation_budget::max_errors and sets truncated() when more failures existed; the
   * verdict is still invalid. Diagnostics live in validation_options::report_storage,
   * which must outlive the report. Move-only.
   */
  class [[nodiscard]] validation_report
  {
    private:
      owned_block m_diagnostics{};
      std::size_t m_count{};
      bool m_valid{true};
      bool m_truncated{};

      validation_report() noexcept = default;

      friend struct detail::validation_report_access;

    public:
      /**
       * @brief Take over another report; other becomes an empty valid report.
       * @par Requirements
       * P046.
       * @par ID SWU-SCHEMA-162
       */
      validation_report(validation_report&& other) noexcept;

      /**
       * @brief Release the diagnostics through the report storage.
       * @par Requirements
       * P046; J5-040.
       */
      ~validation_report() noexcept = default;

      validation_report(const validation_report&) = delete;
      validation_report& operator=(const validation_report&) = delete;
      validation_report& operator=(validation_report&&) = delete;

      /**
       * @brief Return true when the instance satisfied the schema.
       * @par Requirements
       * P046; J5-063.
       * @par ID SWU-SCHEMA-135
       */
      [[nodiscard]] bool valid() const noexcept;

      /**
       * @brief Return true when collect-all stopped at max_errors.
       * @par Requirements
       * P046; J5-063; ADR 0006 decision 12.
       * @par ID SWU-SCHEMA-136
       */
      [[nodiscard]] bool truncated() const noexcept;

      /**
       * @brief Return the number of stored diagnostics.
       * @par Requirements
       * P046; J5-063.
       * @par ID SWU-SCHEMA-137
       */
      [[nodiscard]] std::size_t diagnostic_count() const noexcept;

      /**
       * @brief Borrow diagnostic index, or nullptr when index >= diagnostic_count().
       * @par Requirements
       * P046; J5-064.
       * @par ID SWU-SCHEMA-138
       */
      [[nodiscard]] const schema_diagnostic* diagnostic(std::size_t index) const noexcept;
  };
} // namespace adaptive_json5

#endif /* ADAPTIVE_JSON5_SCHEMA_VALIDATION_REPORT_HPP_ */
