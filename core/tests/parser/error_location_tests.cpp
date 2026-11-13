#include "adaptive_json5/access_error_reason.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/resource_limit_reason.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "common/internal_link.hpp"
#include "input/cursor.hpp"
#include "lexer/member_name_lexer.hpp"
#include "lexer/string/string_lexer.hpp"
#include "memory/resource_budget.hpp"
#include "parser/parser_work.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <ostream>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;

  /* =================================== Test_J5_043 =================================== */
  // Only access failures met while following a recorded link become internal faults.
  static_assert(as_internal_fault(error::access_failure(access_error_reason::index_out_of_range)).code() ==
                error_code::internal_error);
  static_assert(as_internal_fault(error::access_failure(access_error_reason::wrong_type)).code() ==
                error_code::internal_error);
  static_assert(as_internal_fault(error::limit_without_location(resource_limit_reason::work)).limit_reason() ==
                resource_limit_reason::work);
  static_assert(as_internal_fault(error::without_location(error_code::allocation_failure)).code() ==
                error_code::allocation_failure);
  static_assert(as_internal_fault(error::syntax_at(4U, syntax_error_reason::missing_comma)).byte_offset() == 4U);

  /*
   * Keep a real failure from a cut scan instead of reporting it as work exhaustion.
   * Arrange: A scan cut after four of eight visible bytes.
   * Act: Map an allocation failure, an internal fault, a string limit, and syntax and encoding
   *      failures near the cut.
   * Expect: Only the syntax and encoding failures become resource_limit (work) at the cut; the
   *         other failures are returned unchanged.
   */
  TEST(J5_043_CutScanError, KeepsRealFailuresAndRemapsOnlyCutArtifacts)
  {
    /* Arrange */
    const std::string text = "[\"abcdef\"]";
    const limited_scan scan{cursor{input_view{text.data(), 4U}}, 0U, 4U, true};
    /* Act */
    const error allocation = limited_scan_error(scan, error::without_location(error_code::allocation_failure));
    const error internal = limited_scan_error(scan, error::internal_fault());
    const error string_limit = limited_scan_error(scan, error::limit_at(3U, resource_limit_reason::string));
    const error syntax = limited_scan_error(scan, error::syntax_at(4U, syntax_error_reason::unterminated_string));
    const error encoding = limited_scan_error(scan, error::at_offset(error_code::encoding, 3U));
    /* Expect */
    EXPECT_EQ(allocation.code(), error_code::allocation_failure);
    EXPECT_FALSE(allocation.has_byte_offset());
    EXPECT_EQ(internal.code(), error_code::internal_error);
    EXPECT_EQ(string_limit.limit_reason(), resource_limit_reason::string);
    EXPECT_EQ(string_limit.byte_offset(), 3U);
    EXPECT_EQ(syntax.code(), error_code::resource_limit);
    EXPECT_EQ(syntax.limit_reason(), resource_limit_reason::work);
    EXPECT_EQ(syntax.byte_offset(), 4U);
    EXPECT_EQ(encoding.limit_reason(), resource_limit_reason::work);
    EXPECT_EQ(encoding.byte_offset(), 4U);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Name the binding string limit and locate token-bound limits at the failing token.
   * Arrange: Unlocated and located limit failures and one non-limit failure.
   * Act: Pass each through locate_limit at token offset 6, with and without a binding total.
   * Expect: Token limits are located at 6 unless already located; string becomes total_string
   *         only when the total was binding; input and owned_bytes stay unlocated; other codes
   *         pass through.
   */
  TEST(J5_043_LocateLimit, NamesBindingLimitAndLocatesTokenLimits)
  {
    /* Arrange */
    const error depth = error::limit_without_location(resource_limit_reason::depth);
    const error string = error::limit_at(9U, resource_limit_reason::string);
    const error input = error::limit_without_location(resource_limit_reason::input);
    const error owned = error::limit_without_location(resource_limit_reason::owned_bytes);
    const error syntax = error::syntax_at(2U, syntax_error_reason::missing_colon);
    /* Act */
    const error located_depth = locate_limit(depth, 6U, false);
    const error own_string = locate_limit(string, 6U, false);
    const error total_string = locate_limit(string, 6U, true);
    const error same_input = locate_limit(input, 6U, false);
    const error same_owned = locate_limit(owned, 6U, true);
    const error same_syntax = locate_limit(syntax, 6U, true);
    /* Expect */
    EXPECT_EQ(located_depth.limit_reason(), resource_limit_reason::depth);
    EXPECT_TRUE(located_depth.has_byte_offset());
    EXPECT_EQ(located_depth.byte_offset(), 6U);
    EXPECT_EQ(own_string.limit_reason(), resource_limit_reason::string);
    EXPECT_EQ(own_string.byte_offset(), 9U);
    EXPECT_EQ(total_string.limit_reason(), resource_limit_reason::total_string);
    EXPECT_EQ(total_string.byte_offset(), 9U);
    EXPECT_FALSE(same_input.has_byte_offset());
    EXPECT_EQ(same_input.limit_reason(), resource_limit_reason::input);
    EXPECT_FALSE(same_owned.has_byte_offset());
    EXPECT_EQ(same_owned.limit_reason(), resource_limit_reason::owned_bytes);
    EXPECT_EQ(same_syntax.code(), error_code::syntax);
    EXPECT_EQ(same_syntax.byte_offset(), 2U);
  }
  /* ================================= End Test_J5_043 ================================= */

  /* =================================== Test_J5_017 =================================== */
  /*
   * Report how many diagnostic entries still fit.
   * Arrange: A budget allowing two diagnostics.
   * Act: Reserve two entries, then release one.
   * Expect: The allowance counts down to zero and back up, and never wraps.
   */
  TEST(J5_017_DiagnosticAllowance, CountsRemainingEntries)
  {
    /* Arrange */
    resource_limits limits{};
    limits.max_diagnostics = 2U;
    resource_budget budget{limits};
    const std::size_t initial = budget.diagnostic_allowance();
    /* Act */
    ASSERT_TRUE(budget.reserve_diagnostic().has_value());
    const std::size_t after_one = budget.diagnostic_allowance();
    ASSERT_TRUE(budget.reserve_diagnostic().has_value());
    const std::size_t after_two = budget.diagnostic_allowance();
    const auto third = budget.reserve_diagnostic();
    ASSERT_TRUE(budget.release_diagnostic().has_value());
    const std::size_t after_release = budget.diagnostic_allowance();
    /* Expect */
    EXPECT_EQ(initial, 2U);
    EXPECT_EQ(after_one, 1U);
    EXPECT_EQ(after_two, 0U);
    ASSERT_FALSE(third.has_value());
    EXPECT_EQ(third.error_if()->limit_reason(), resource_limit_reason::diagnostics);
    EXPECT_EQ(after_release, 1U);
  }
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief One string measurement with a warning allowance.
   * @par Requirements
   * P021; J5-017; J5-047; issue #125.
   */
  struct excess_case
  {
      const char* name;                     // Case identifier.
      std::string text;                     // Input quoted string; LS is a raw U+2028.
      std::size_t allowance;                // Input warnings that still fit.
      std::size_t warnings;                 // Expected total warning count.
      std::optional<std::size_t> excess_at; // Expected byte offset of excess_warning; nullopt means none.
  };

  /** @brief Name an excess case for parameterized test output. */
  std::ostream& operator<<(std::ostream& stream, const excess_case& value)
  {
    return stream << value.name;
  }

  class J5_017_ExcessMeasure : public testing::TestWithParam<excess_case>
  {
  };

  const std::string c_ls = "\xE2\x80\xA8";

  /*
   * Mark the first warning past the allowance while measuring, for strings and member names.
   * Arrange: The case string and allowance.
   * Act: Measure it as a string value and as a quoted member name.
   * Expect: The total warning count is unchanged by the allowance, and excess_warning is the
   *         warning at the allowance's index, or absent when every warning fits.
   */
  TEST_P(J5_017_ExcessMeasure, MarksFirstWarningPastAllowance)
  {
    /* Arrange */
    const excess_case& param = GetParam();
    cursor as_value{input_view{param.text.data(), param.text.size()}};
    cursor as_name{input_view{param.text.data(), param.text.size()}};
    /* Act */
    const auto value = measure_string(as_value, syntax_mode::json5, 64U, param.allowance);
    const auto name = measure_member_name(as_name, syntax_mode::json5, 64U, param.allowance);
    /* Expect */
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(value.value_if()->has_value());
    ASSERT_TRUE(name.has_value());
    ASSERT_TRUE(name.value_if()->has_value());
    const auto& measured_value = **value.value_if();
    const auto& measured_name = **name.value_if();
    EXPECT_EQ(measured_value.warning_count, param.warnings);
    EXPECT_EQ(measured_name.warning_count, param.warnings);
    EXPECT_EQ(measured_value.excess_warning.has_value(), param.excess_at.has_value());
    EXPECT_EQ(measured_name.excess_warning.has_value(), param.excess_at.has_value());
    if (param.excess_at.has_value())
    {
      EXPECT_EQ(measured_value.excess_warning->byte_offset, *param.excess_at);
      EXPECT_EQ(measured_name.excess_warning->byte_offset, *param.excess_at);
    }
  }
  INSTANTIATE_TEST_SUITE_P(Issue125, J5_017_ExcessMeasure,
                           testing::Values(excess_case{"NoWarnings", "\"ab\"", 0U, 0U, std::nullopt},
                                           excess_case{"ZeroAllowance", "\"a" + c_ls + "\"", 0U, 1U, 2U},
                                           excess_case{"AllFit", "\"a" + c_ls + "b" + c_ls + "\"", 2U, 2U,
                                                       std::nullopt},
                                           excess_case{"SecondExceeds", "\"a" + c_ls + "b" + c_ls + "\"", 1U, 2U, 6U},
                                           excess_case{"DefaultNeverLimits", "\"" + c_ls + c_ls + "\"",
                                                       std::numeric_limits<std::size_t>::max(), 2U, std::nullopt}),
                           testing::PrintToStringParamName());
  /* ================================= End Test_J5_017 ================================= */
} // namespace
