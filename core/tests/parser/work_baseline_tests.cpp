#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/result.hpp"
#include "parser/parser_engine.hpp"

#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <ostream>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Unbounded host provider for documents with many small allocations.
   * @par Requirements
   * J5-046; J5-049.
   * @note Only these test callbacks use malloc; live bytes must return to zero.
   */
  class heap_provider
  {
    private:
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<heap_provider*>(context);

        if (alignment > alignof(std::max_align_t))
        {
          return nullptr;
        }

        void* address = std::malloc(bytes);
        self.m_live_bytes += (address != nullptr) ? bytes : 0U;
        return address;
      }

      static void release(void* context, void* address, std::size_t bytes, std::size_t) noexcept
      {
        static_cast<heap_provider*>(context)->m_live_bytes -= bytes;
        std::free(address);
      }

    public:
      std::size_t m_live_bytes{};

      [[nodiscard]] storage_callbacks callbacks() noexcept
      {
        return storage_callbacks{this, &allocate, &release};
      }
  };

  /**
   * @brief Parse a complete document with default limits, optionally with another work ceiling.
   * @par Requirements
   * J5-047; J5-049.
   * @param input Complete document bytes.
   * @param options Parse options; only max_work_units differs from the defaults in these units.
   * @return The engine result; owned values are released before returning.
   */
  result<std::size_t> parse_completely(const std::string& input, const parse_options& options)
  {
    heap_provider provider;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    const auto parsed = engine.run_complete();

    if (!parsed.has_value())
    {
      return result<std::size_t>::failure(*parsed.error_if());
    }

    return result<std::size_t>::success(parsed.value_if()->values.size());
  }

  /**
   * @brief Pad a document with trailing spaces to exactly size bytes.
   * @par Requirements
   * J5-047.
   */
  std::string padded(std::string text, std::size_t size)
  {
    if (text.size() < size)
    {
      text.append(size - text.size(), ' ');
    }

    return text;
  }

  constexpr std::size_t c_input = 16384;

  /**
   * @brief Build the few-nodes, plain-strings worst case.
   * @par Requirements
   * J5-047; J5-049.
   */
  std::string plain_strings()
  {
    // Four strings of 4090 units: just under max_string_units and the total of 16384 units.
    std::string text = "[";
    for (std::size_t index{}; index < 4; ++index)
    {
      text += (index == 0) ? "\"" : ",\"";
      text += std::string(4090, 'a');
      text += "\"";
    }
    return padded(text + "]", c_input);
  }

  /**
   * @brief Build the case where every string carries a warning, each re-scanned once.
   * @par Requirements
   * J5-047; J5-049.
   */
  std::string warning_strings()
  {
    // Sixteen strings (max_diagnostics) of about 1000 bytes, each with one raw U+2028 warning.
    std::string text = "[";
    for (std::size_t index{}; index < 16; ++index)
    {
      text += (index == 0) ? "\"" : ",\"";
      text += std::string(1000, 'a');
      text += "\xE2\x80\xA8\"";
    }
    return padded(text + "]", c_input);
  }

  /**
   * @brief Build the many-numbers worst case.
   * @par Requirements
   * J5-047; J5-049.
   */
  std::string numbers()
  {
    // 1023 numbers (max_nodes minus the root) of 15 bytes each.
    std::string text = "[";
    for (std::size_t index{}; index < 1023; ++index)
    {
      text += (index == 0) ? "" : ",";
      text += "1.234567890e+99";
    }
    return padded(text + "]", c_input);
  }

  /**
   * @brief Build the many-small-nodes, many-tokens worst case.
   * @par Requirements
   * J5-047; J5-049.
   */
  std::string small_nodes()
  {
    // An object with 1023 members whose padding between tokens fills the input.
    std::string text = "{";
    for (std::size_t index{}; index < 1023; ++index)
    {
      text += (index == 0) ? "" : ", ";
      text += "a  :  0 ";
    }
    return padded(text + "}", c_input);
  }
  /* ================================== End Fixtures =================================== */
  /**
   * @brief Describe one maximum-size worst-case document and its measured work requirement.
   * @par Requirements
   * P037; J5-047; J5-049.
   */
  struct baseline_case
  {
      const char* name;
      std::string (*build)();    // Builds the 16384-byte document.
      std::size_t values;        // Expected number of document values.
      std::size_t required_work; // Measured minimum max_work_units (ADR 0003, decision 7b).
  };

  std::ostream& operator<<(std::ostream& stream, const baseline_case& value)
  {
    return stream << value.name;
  }

  class WorkBaselineUnits : public ::testing::TestWithParam<baseline_case>
  {
  };

  /* =================================== Test_J5_049 =================================== */

  /* Verify each worst-case document parses within the default work budget, exactly at its
   * measured requirement, and not one unit below it.
   * Arrange: The case document, exactly max_input_bytes long, and otherwise default limits.
   * Act: Parse completely with the default budget, the measured requirement and one less.
   * Expect: Success, success and resource_limit; the requirement stays below the default
   *         (ADR 0003, decision 7b; these values replace the ADR's estimates).
   */
  TEST_P(WorkBaselineUnits, FitsDefaultAndFailsJustBelowMeasuredRequirement)
  {
    /* Arrange */
    const baseline_case& param = GetParam();
    const std::string input = param.build();
    const parse_options defaults{};
    parse_options exact{};
    exact.limits.max_work_units = param.required_work;
    parse_options below{};
    below.limits.max_work_units = param.required_work - 1U;
    /* Act */
    const auto with_defaults = parse_completely(input, defaults);
    const auto at_requirement = parse_completely(input, exact);
    const auto below_requirement = parse_completely(input, below);
    /* Expect */
    EXPECT_EQ(input.size(), defaults.limits.max_input_bytes);
    EXPECT_LE(param.required_work, defaults.limits.max_work_units);
    ASSERT_TRUE(with_defaults.has_value());
    EXPECT_EQ(*with_defaults.value_if(), param.values);
    EXPECT_TRUE(at_requirement.has_value());
    ASSERT_FALSE(below_requirement.has_value());
    EXPECT_EQ(below_requirement.error_if()->code, error_code::resource_limit);
  }

  INSTANTIATE_TEST_SUITE_P(J5_049_WorkBaseline, WorkBaselineUnits,
                           ::testing::Values(baseline_case{"PlainStrings", &plain_strings, 5, 65527},
                                             baseline_case{"WarningStrings", &warning_strings, 17, 65011},
                                             baseline_case{"Numbers", &numbers, 1024, 53231},
                                             baseline_case{"SmallNodes", &small_nodes, 1024, 41978}),
                           [](const ::testing::TestParamInfo<baseline_case>& info)
                           {
                             return info.param.name;
                           });
  /* ================================= End Test_J5_049 ================================= */
} // namespace
