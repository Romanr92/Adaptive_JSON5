// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{
  using namespace adaptive_json5;

  /**
   * @brief Test-only heap provider that checks every release against an outstanding total.
   * @par Requirements
   * P046; J5-065; J5-097.
   */
  struct heap_provider
  {
      std::size_t live{};

      static result<void*> allocate(void* context, std::size_t bytes, std::size_t /* alignment */) noexcept
      {
        void* const address = std::malloc(bytes);
        if (address == nullptr)
        {
          return result<void*>::failure(error::without_location(error_code::allocation_failure));
        }
        static_cast<heap_provider*>(context)->live += bytes;
        return result<void*>::success(address);
      }

      static void release(void* context, void* address, std::size_t bytes, std::size_t /* alignment */) noexcept
      {
        auto& self = *static_cast<heap_provider*>(context);
        if (bytes > self.live)
        {
          std::abort();
        }
        self.live -= bytes;
        std::free(address);
      }

      storage_callbacks callbacks() noexcept
      {
        return storage_callbacks{this, &allocate, &release};
      }
  };

  /**
   * @brief Abort unless a report is internally consistent and its locations format.
   * @par Requirements
   * P046; J5-063; J5-064.
   */
  void check_report(const compiled_schema& schema, const document& instance, const validation_report& report,
                    const validation_options& options, heap_provider& scratch)
  {
    if (report.valid() != (report.diagnostic_count() == 0U))
    {
      std::abort();
    }
    const std::size_t limit = (options.mode == evaluation_mode::fail_fast) ? 1U : options.budget.max_errors;
    if ((report.diagnostic_count() > limit) || (report.truncated() && (options.mode == evaluation_mode::fail_fast)))
    {
      std::abort();
    }
    std::array<char, 256> buffer{};
    for (std::size_t index = 0U; index < report.diagnostic_count(); ++index)
    {
      const schema_diagnostic* diagnostic = report.diagnostic(index);
      if ((diagnostic == nullptr) || (diagnostic->schema_node >= schema.node_count()))
      {
        std::abort();
      }
      static_cast<void>(format_schema_location(schema, *diagnostic, buffer.data(), buffer.size()));
      const auto pointer =
        format_instance_location(instance, *diagnostic, buffer.data(), buffer.size(), scratch.callbacks());
      if (!pointer.has_value() && (pointer.error_if()->code() == schema_error_code::internal_error))
      {
        std::abort();
      }
    }
    if (report.diagnostic(report.diagnostic_count()) != nullptr)
    {
      std::abort();
    }
  }

  /**
   * @brief Validate one instance in both modes with small budgets.
   * @par Requirements
   * P046; P053; J5-056; J5-058; J5-065.
   * @note With collect, an invalid report must carry no annotations and every record must
   * name an annotated node.
   */
  void validate_both_modes(const compiled_schema& schema, const document& instance, bool collect)
  {
    for (const evaluation_mode mode : {evaluation_mode::fail_fast, evaluation_mode::collect_all})
    {
      heap_provider work;
      heap_provider report_storage;
      heap_provider scratch;
      validation_options options{};
      options.work_storage = work.callbacks();
      options.report_storage = report_storage.callbacks();
      options.mode = mode;
      options.budget.max_work_steps = 100000U;
      options.budget.max_errors = 8U;
      options.collect_annotations = collect;
      {
        const auto outcome = validate(schema, instance, options);
        if (work.live != 0U)
        {
          std::abort();
        }
        if (outcome.has_value())
        {
          check_report(schema, instance, *outcome.value_if(), options, scratch);
          const validation_report& report = *outcome.value_if();
          if (!report.valid() && (report.annotation_count() != 0U))
          {
            std::abort();
          }
          for (std::size_t index = 0U; index < report.annotation_count(); ++index)
          {
            if (!annotation_values(schema, *report.annotation(index)).has_value())
            {
              std::abort();
            }
          }
        }
      }
      if ((report_storage.live != 0U) || (scratch.live != 0U))
      {
        std::abort();
      }
    }
  }
} // namespace

/**
 * @brief Fuzz registration, compilation and validation: schema text, a NUL byte, instance text.
 * @par Requirements
 * P046; P053; P054; J5-054; J5-055; J5-056; J5-058; J5-065; J5-069; J5-097.
 * @note Every failure must be a structured result; storage must balance; reports must be consistent.
 */
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  const char* const text = reinterpret_cast<const char*>(data);
  const void* const separator = std::memchr(data, 0, size);
  const std::size_t schema_size =
    (separator == nullptr) ? size : static_cast<std::size_t>(static_cast<const std::uint8_t*>(separator) - data);
  const std::size_t instance_offset = (separator == nullptr) ? size : (schema_size + 1U);

  heap_provider registry_provider;
  heap_provider result_provider;
  heap_provider scratch_provider;
  heap_provider instance_provider;
  {
    accounted_storage registry_storage{registry_provider.callbacks(), 1U << 24U};
    accounted_storage instance_storage{instance_provider.callbacks(), parse_options{}.limits.max_owned_bytes};
    auto registry = schema_registry::create(registry_storage, 1U);
    if (!registry.has_value())
    {
      std::abort();
    }
    if (!registry.value_if()->add({text, schema_size}).has_value())
    {
      return 0;
    }
    compile_options options{};
    options.limits.max_schema_nodes = 4096U;
    options.limits.max_compile_steps = 200000U;
    options.result_storage = result_provider.callbacks();
    options.scratch_storage = scratch_provider.callbacks();
    // Input lengths select the P053 options, so existing seeds stay valid inputs.
    options.assert_format = (schema_size % 2U) == 0U;
    const auto compiled =
      compile_schema(*registry.value_if(), c_default_schema_base_uri, schema_dialect::draft_07, options);
    if (scratch_provider.live != 0U)
    {
      std::abort();
    }
    if (compiled.has_value())
    {
      const auto instance =
        parse_document({text + instance_offset, size - instance_offset}, parse_options{}, instance_storage);
      if (instance.has_value())
      {
        validate_both_modes(*compiled.value_if(), instance.value_if()->values, ((size - instance_offset) % 2U) == 1U);
      }
    }
  }
  if ((registry_provider.live != 0U) || (result_provider.live != 0U) || (instance_provider.live != 0U))
  {
    std::abort();
  }
  return 0;
}
