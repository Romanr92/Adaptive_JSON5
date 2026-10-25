#include "../shared_infrastructure/document_provider.hpp"
#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/value/integer_conversion.hpp"
#include "adaptive_json5/value/numeric_conversion.hpp"
#include "adaptive_json5/value/numeric_conversion_reason.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <optional>
#include <ostream>
#include <string_view>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::document_provider;
  using adaptive_json5_test::guard_provider;

  constexpr std::size_t c_max_spelling_bytes = 4096;

  /**
   * @brief Validate and own one JSON5 numeric spelling for a unit.
   * @par Requirements
   * P038; J5-034.
   * @param provider Guarded storage that must outlive the returned number.
   * @param text NUL-terminated numeric spelling.
   * @return Owned number or the validation/allocation error.
   */
  result<owned_number> make_number(guard_provider& provider, const char* text)
  {
    return owned_number::copy_spelling(input_view{text, std::strlen(text)}, syntax_mode::json5, provider.callbacks(),
                                       c_max_spelling_bytes);
  }

  /**
   * @brief Select the fixed-width destination used by one conversion case.
   * @par Requirements
   * P038; J5-035.
   */
  enum class destination
  {
    int8,
    uint8,
    int16,
    uint16,
    int32,
    uint32,
    int64,
    uint64
  };

  /**
   * @brief Record a typed conversion outcome in a destination-independent form.
   * @par Requirements
   * P038; J5-035.
   * @note Successful signed values use signed_value; unsigned values use unsigned_value.
   */
  struct conversion_outcome
  {
      bool succeeds{};
      std::intmax_t signed_value{};
      std::uintmax_t unsigned_value{};
      error_code code{};
      std::optional<numeric_conversion_reason> reason{};
  };

  /**
   * @brief Convert through to_integer<T> and widen the outcome for comparison.
   * @par Requirements
   * P038; J5-035.
   * @tparam T Fixed-width destination type.
   * @param number Number to convert.
   * @return Widened success value or the reported error.
   */
  template <typename T>
  conversion_outcome convert_as(const owned_number& number)
  {
    const auto converted = to_integer<T>(number);
    conversion_outcome outcome{};
    outcome.succeeds = converted.has_value();
    if (outcome.succeeds)
    {
      outcome.signed_value = static_cast<std::intmax_t>(*converted.value_if());
      outcome.unsigned_value = static_cast<std::uintmax_t>(*converted.value_if());
      return outcome;
    }
    outcome.code = converted.error_if()->code;
    outcome.reason = converted.error_if()->numeric_reason;
    return outcome;
  }

  /**
   * @brief Dispatch a case to the selected destination type.
   * @par Requirements
   * P038; J5-035.
   * @param number Number to convert.
   * @param target Selected destination.
   * @return Widened outcome of to_integer for that destination.
   */
  conversion_outcome convert_to(const owned_number& number, destination target)
  {
    switch (target)
    {
    case destination::int8:
      return convert_as<std::int8_t>(number);
    case destination::uint8:
      return convert_as<std::uint8_t>(number);
    case destination::int16:
      return convert_as<std::int16_t>(number);
    case destination::uint16:
      return convert_as<std::uint16_t>(number);
    case destination::int32:
      return convert_as<std::int32_t>(number);
    case destination::uint32:
      return convert_as<std::uint32_t>(number);
    case destination::int64:
      return convert_as<std::int64_t>(number);
    case destination::uint64:
      return convert_as<std::uint64_t>(number);
    }
    return conversion_outcome{};
  }

  /**
   * @brief Report whether a destination is signed.
   * @par Requirements
   * P038; J5-035.
   */
  bool is_signed_destination(destination target)
  {
    return (target == destination::int8) || (target == destination::int16) || (target == destination::int32) ||
           (target == destination::int64);
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_035 =================================== */
  static_assert(noexcept(to_integer<int>(std::declval<const owned_number&>())));
  static_assert(noexcept(to_unsigned_integer(std::declval<const owned_number&>())));
  static_assert(noexcept(to_signed_integer(std::declval<const owned_number&>())));

  /**
   * @brief Describe one fixed-width integer conversion and its outcome.
   * @par Requirements
   * P038; J5-035; J5-036; J5-038.
   */
  struct integer_case
  {
      const char* name;
      const char* spelling;             // Input JSON5 numeric spelling.
      destination target;               // Requested destination type.
      bool succeeds;                    // Whether conversion succeeds.
      std::intmax_t signed_value;       // Expected value for signed destinations.
      std::uintmax_t unsigned_value;    // Expected value for unsigned destinations.
      numeric_conversion_reason reason; // Expected conversion reason on failure.
  };

  std::ostream& operator<<(std::ostream& stream, const integer_case& value)
  {
    return stream << value.name;
  }

  class IntegerConversionUnits : public ::testing::TestWithParam<integer_case>
  {
  };

  constexpr auto c_out_of_range = numeric_conversion_reason::out_of_range;
  constexpr auto c_non_integral = numeric_conversion_reason::non_integral;
  constexpr auto c_non_finite = numeric_conversion_reason::non_finite;

  /**
   * @brief Build a successful signed-destination case.
   * @par Requirements
   * P038; J5-035.
   */
  constexpr integer_case signed_ok(const char* name, const char* spelling, destination target, std::intmax_t value)
  {
    return integer_case{name, spelling, target, true, value, 0U, c_out_of_range};
  }

  /**
   * @brief Build a successful unsigned-destination case.
   * @par Requirements
   * P038; J5-035.
   */
  constexpr integer_case unsigned_ok(const char* name, const char* spelling, destination target, std::uintmax_t value)
  {
    return integer_case{name, spelling, target, true, 0, value, c_out_of_range};
  }

  /**
   * @brief Build a failing case with its expected conversion reason.
   * @par Requirements
   * P038; J5-035.
   */
  constexpr integer_case fails(const char* name, const char* spelling, destination target,
                               numeric_conversion_reason reason)
  {
    return integer_case{name, spelling, target, false, 0, 0U, reason};
  }

  /* Verify exact conversion at every fixed-width bound without rounding, wrapping or clamping.
   * Arrange: Own the case spelling.
   * Act: Convert it to the case destination through to_integer.
   * Expect: The exact value, or numeric_conversion with the case reason.
   */
  TEST_P(IntegerConversionUnits, ConvertsExactlyOrReportsReason)
  {
    /* Arrange */
    const integer_case& param = GetParam();
    guard_provider provider;
    const auto number = make_number(provider, param.spelling);
    ASSERT_TRUE(number.has_value());
    /* Act */
    const conversion_outcome outcome = convert_to(*number.value_if(), param.target);
    /* Expect */
    ASSERT_EQ(outcome.succeeds, param.succeeds);
    if (param.succeeds)
    {
      if (is_signed_destination(param.target))
      {
        EXPECT_EQ(outcome.signed_value, param.signed_value);
      }
      else
      {
        EXPECT_EQ(outcome.unsigned_value, param.unsigned_value);
      }
      return;
    }
    EXPECT_EQ(outcome.code, error_code::numeric_conversion);
    ASSERT_TRUE(outcome.reason.has_value());
    EXPECT_EQ(*outcome.reason, param.reason);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_035_IntegerConversion, IntegerConversionUnits,
    ::testing::Values(signed_ok("Int8Max", "127", destination::int8, 127),
                      fails("Int8AboveMax", "128", destination::int8, c_out_of_range),
                      signed_ok("Int8Min", "-128", destination::int8, -128),
                      fails("Int8BelowMin", "-129", destination::int8, c_out_of_range),
                      signed_ok("Int8MaxExponent", "1.27e2", destination::int8, 127),
                      signed_ok("Int8MaxHex", "0x7f", destination::int8, 127),
                      signed_ok("Int8MinHex", "-0x80", destination::int8, -128),
                      fails("Int8AboveMaxHex", "0x80", destination::int8, c_out_of_range),
                      unsigned_ok("Uint8Max", "255", destination::uint8, 255U),
                      fails("Uint8AboveMax", "256", destination::uint8, c_out_of_range),
                      fails("Uint8NegativeOne", "-1", destination::uint8, c_out_of_range),
                      unsigned_ok("Uint8NegativeZero", "-0", destination::uint8, 0U),
                      unsigned_ok("Uint8MaxExponent", "2.55e2", destination::uint8, 255U),
                      signed_ok("Int16Max", "32767", destination::int16, 32767),
                      fails("Int16AboveMax", "32768", destination::int16, c_out_of_range),
                      signed_ok("Int16Min", "-32768", destination::int16, -32768),
                      fails("Int16BelowMin", "-32769", destination::int16, c_out_of_range),
                      unsigned_ok("Uint16Max", "65535", destination::uint16, 65535U),
                      fails("Uint16AboveMax", "65536", destination::uint16, c_out_of_range),
                      signed_ok("Int32Max", "2147483647", destination::int32, 2147483647),
                      fails("Int32AboveMax", "2147483648", destination::int32, c_out_of_range),
                      signed_ok("Int32Min", "-2147483648", destination::int32, -2147483647 - 1),
                      fails("Int32BelowMin", "-2147483649", destination::int32, c_out_of_range),
                      unsigned_ok("Uint32Max", "4294967295", destination::uint32, 4294967295U),
                      fails("Uint32AboveMax", "4294967296", destination::uint32, c_out_of_range),
                      signed_ok("Int64Max", "9223372036854775807", destination::int64, INT64_MAX),
                      fails("Int64AboveMax", "9223372036854775808", destination::int64, c_out_of_range),
                      signed_ok("Int64Min", "-9223372036854775808", destination::int64, INT64_MIN),
                      fails("Int64BelowMin", "-9223372036854775809", destination::int64, c_out_of_range),
                      unsigned_ok("Uint64Max", "18446744073709551615", destination::uint64, UINT64_MAX),
                      unsigned_ok("Uint64MaxHex", "0xFFFFFFFFFFFFFFFF", destination::uint64, UINT64_MAX),
                      fails("Uint64AboveMax", "18446744073709551616", destination::uint64, c_out_of_range),
                      fails("Uint64AboveMaxHex", "0x10000000000000001", destination::uint64, c_out_of_range),
                      signed_ok("PlusSign", "+1", destination::int32, 1),
                      signed_ok("PointZero", "1.0", destination::int32, 1),
                      signed_ok("Exponent", "1e2", destination::int32, 100),
                      signed_ok("FractionScaled", "1.5e1", destination::int32, 15),
                      signed_ok("TrailingZerosCancelExponent", "100e-2", destination::int32, 1),
                      signed_ok("NegativeZero", "-0.0", destination::int32, 0),
                      fails("Fraction", "1.5", destination::int32, c_non_integral),
                      fails("NegativeFractionUnsigned", "-1.5", destination::uint32, c_non_integral),
                      fails("NegativeTenth", "-1e-1", destination::int32, c_non_integral),
                      fails("CloseFraction", "1.0000000000000000000001", destination::int64, c_non_integral),
                      fails("HugeNegativeExponent", "1e-99999999999999999999999", destination::int32, c_non_integral),
                      fails("HugePositiveExponent", "1e99999999999999999999999", destination::int32, c_out_of_range),
                      fails("Infinity", "Infinity", destination::int32, c_non_finite),
                      fails("NegativeInfinityUnsigned", "-Infinity", destination::uint32, c_non_finite),
                      fails("NaN", "NaN", destination::int64, c_non_finite)),
    [](const ::testing::TestParamInfo<integer_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify the widest conversions report sign and range outcomes directly.
   * Arrange: Own "-1", "-0" and "42".
   * Act: Convert each with to_unsigned_integer and to_signed_integer.
   * Expect: Unsigned rejects -1 as out_of_range and accepts -0 as 0; signed returns exact values.
   */
  TEST(J5_035_IntegerConversion, WidestConversionsKeepSignSemantics)
  {
    /* Arrange */
    guard_provider provider;
    const auto minus_one = make_number(provider, "-1");
    const auto minus_zero = make_number(provider, "-0");
    const auto answer = make_number(provider, "42");
    ASSERT_TRUE(minus_one.has_value());
    ASSERT_TRUE(minus_zero.has_value());
    ASSERT_TRUE(answer.has_value());
    /* Act */
    const auto unsigned_minus_one = to_unsigned_integer(*minus_one.value_if());
    const auto unsigned_minus_zero = to_unsigned_integer(*minus_zero.value_if());
    const auto unsigned_answer = to_unsigned_integer(*answer.value_if());
    const auto signed_minus_one = to_signed_integer(*minus_one.value_if());
    const auto signed_minus_zero = to_signed_integer(*minus_zero.value_if());
    /* Expect */
    ASSERT_FALSE(unsigned_minus_one.has_value());
    EXPECT_EQ(unsigned_minus_one.error_if()->code, error_code::numeric_conversion);
    ASSERT_TRUE(unsigned_minus_one.error_if()->numeric_reason.has_value());
    EXPECT_EQ(*unsigned_minus_one.error_if()->numeric_reason, c_out_of_range);
    ASSERT_TRUE(unsigned_minus_zero.has_value());
    EXPECT_EQ(*unsigned_minus_zero.value_if(), 0U);
    ASSERT_TRUE(unsigned_answer.has_value());
    EXPECT_EQ(*unsigned_answer.value_if(), 42U);
    ASSERT_TRUE(signed_minus_one.has_value());
    EXPECT_EQ(*signed_minus_one.value_if(), -1);
    ASSERT_TRUE(signed_minus_zero.has_value());
    EXPECT_EQ(*signed_minus_zero.value_if(), 0);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify conversions of a moved-from number report wrong_type_or_access.
   * Arrange: Own "1" and move it into another number.
   * Act: Convert and query the moved-from number.
   * Expect: wrong_type_or_access from every operation; the moved-to number still converts.
   */
  TEST(J5_035_IntegerConversion, RejectsEmptySpelling)
  {
    /* Arrange */
    guard_provider provider;
    auto number = make_number(provider, "1");
    ASSERT_TRUE(number.has_value());
    const owned_number moved{std::move(*number.value_if())};
    const owned_number& empty = *number.value_if();
    /* Act */
    const auto as_unsigned = to_unsigned_integer(empty);
    const auto as_signed = to_signed_integer(empty);
    const auto as_int = to_integer<int>(empty);
    const auto integral = is_integral_number(empty);
    const auto moved_value = to_integer<int>(moved);
    /* Expect */
    ASSERT_FALSE(as_unsigned.has_value());
    EXPECT_EQ(as_unsigned.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(as_signed.has_value());
    EXPECT_EQ(as_signed.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(as_int.has_value());
    EXPECT_EQ(as_int.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(integral.has_value());
    EXPECT_EQ(integral.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(moved_value.has_value());
    EXPECT_EQ(*moved_value.value_if(), 1);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a zero work ceiling is enforced by every numeric query.
   * Arrange: Own "1" and options permitting no work.
   * Act: Convert and query with those options.
   * Expect: resource_limit from every operation.
   */
  TEST(J5_035_IntegerConversion, EnforcesWorkCeiling)
  {
    /* Arrange */
    guard_provider provider;
    const auto number = make_number(provider, "1");
    ASSERT_TRUE(number.has_value());
    numeric_conversion_options options{};
    options.max_work_units = 0U;
    /* Act */
    const auto as_unsigned = to_unsigned_integer(*number.value_if(), options);
    const auto as_signed = to_signed_integer(*number.value_if(), options);
    const auto as_short = to_integer<short>(*number.value_if(), options);
    const auto integral = is_integral_number(*number.value_if(), options);
    /* Expect */
    ASSERT_FALSE(as_unsigned.has_value());
    EXPECT_EQ(as_unsigned.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(as_signed.has_value());
    EXPECT_EQ(as_signed.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(as_short.has_value());
    EXPECT_EQ(as_short.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(integral.has_value());
    EXPECT_EQ(integral.error_if()->code, error_code::resource_limit);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify conversion leaves the original spelling unchanged.
   * Arrange: Own "1.0e2".
   * Act: Convert it to int twice.
   * Expect: 100 both times and the spelling still reads "1.0e2".
   */
  TEST(J5_035_IntegerConversion, PreservesSpellingAndRepeats)
  {
    /* Arrange */
    guard_provider provider;
    const auto number = make_number(provider, "1.0e2");
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto first = to_integer<int>(*number.value_if());
    const auto second = to_integer<int>(*number.value_if());
    /* Expect */
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first.value_if(), 100);
    EXPECT_EQ(*second.value_if(), 100);
    const auto spelling = number.value_if()->spelling();
    EXPECT_EQ(std::string_view(spelling.data, spelling.size), "1.0e2");
  }
  /* ================================= End Test_J5_035 ================================= */
  /* =================================== Test_J5_036 =================================== */
  /**
   * @brief Describe one integrality query and its expected answer.
   * @par Requirements
   * P038; J5-036.
   */
  struct integral_case
  {
      const char* name;
      const char* spelling; // Input JSON5 numeric spelling.
      bool integral;        // Expected mathematical integrality.
  };

  std::ostream& operator<<(std::ostream& stream, const integral_case& value)
  {
    return stream << value.name;
  }

  class IntegralNumberUnits : public ::testing::TestWithParam<integral_case>
  {
  };

  /* Verify integrality is mathematical and independent of lexical form or machine range.
   * Arrange: Own the case spelling.
   * Act: Query is_integral_number.
   * Expect: The case integrality as a successful result.
   */
  TEST_P(IntegralNumberUnits, ReportsMathematicalIntegrality)
  {
    /* Arrange */
    const integral_case& param = GetParam();
    guard_provider provider;
    const auto number = make_number(provider, param.spelling);
    ASSERT_TRUE(number.has_value());
    /* Act */
    const auto integral = is_integral_number(*number.value_if());
    /* Expect */
    ASSERT_TRUE(integral.has_value());
    EXPECT_EQ(*integral.value_if(), param.integral);
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_036_IntegralNumber, IntegralNumberUnits,
    ::testing::Values(integral_case{"One", "1", true}, integral_case{"PointZero", "1.0", true},
                      integral_case{"ExponentZero", "1e0", true}, integral_case{"Hexadecimal", "0x1", true},
                      integral_case{"PlusSign", "+1", true}, integral_case{"NegativeZero", "-0.0", true},
                      integral_case{"BeyondMachineRange", "1e400", true}, integral_case{"Tenth", "1e-1", false},
                      integral_case{"CloseFraction", "1.0000000000000000000001", false},
                      integral_case{"TinyMagnitude", "1e-400", false}, integral_case{"Infinity", "Infinity", false},
                      integral_case{"NaN", "NaN", false}),
    [](const ::testing::TestParamInfo<integral_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /**
   * @brief Describe one document element and its view-based conversion outcomes.
   * @par Requirements
   * P038; J5-035; J5-036.
   */
  struct view_case
  {
      const char* name;
      std::size_t element;        // Input element position in the fixed array document.
      bool converts;              // Whether to_integer<int> succeeds.
      int value;                  // Expected integer on success.
      error_code conversion_code; // Expected to_integer error code on failure.
      bool queries;               // Whether is_integral_number succeeds.
      bool integral;              // Expected integrality on success.
  };

  std::ostream& operator<<(std::ostream& stream, const view_case& value)
  {
    return stream << value.name;
  }

  class NumericViewUnits : public ::testing::TestWithParam<view_case>
  {
  };

  constexpr const char* c_view_document = "[7, true, '7', 7.5, null, 1e400]";

  /* Verify document views convert numbers only, without boolean or string coercion.
   * Arrange: Parse a fixed array of numbers, a boolean, a numeric string and null.
   * Act: Convert and query the case element through its view.
   * Expect: Exact numeric outcomes; wrong_type_or_access for non-number values.
   */
  TEST_P(NumericViewUnits, ConvertsOnlyNumberValues)
  {
    /* Arrange */
    const view_case& param = GetParam();
    document_provider provider;
    parse_options options{syntax_mode::json5};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const std::string_view source{c_view_document};
    const auto parsed = parse_document({source.data(), source.size()}, options, accounting);
    ASSERT_TRUE(parsed.has_value());
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto element = root.value_if()->element_at(param.element);
    ASSERT_TRUE(element.has_value());
    /* Act */
    const auto converted = to_integer<int>(*element.value_if());
    const auto integral = is_integral_number(*element.value_if());
    /* Expect */
    ASSERT_EQ(converted.has_value(), param.converts);
    if (param.converts)
    {
      EXPECT_EQ(*converted.value_if(), param.value);
    }
    else
    {
      EXPECT_EQ(converted.error_if()->code, param.conversion_code);
    }
    ASSERT_EQ(integral.has_value(), param.queries);
    if (param.queries)
    {
      EXPECT_EQ(*integral.value_if(), param.integral);
    }
    else
    {
      EXPECT_EQ(integral.error_if()->code, error_code::wrong_type_or_access);
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    J5_036_NumericView, NumericViewUnits,
    ::testing::Values(view_case{"Integer", 0U, true, 7, error_code::numeric_conversion, true, true},
                      view_case{"Boolean", 1U, false, 0, error_code::wrong_type_or_access, false, false},
                      view_case{"NumericString", 2U, false, 0, error_code::wrong_type_or_access, false, false},
                      view_case{"Fraction", 3U, false, 0, error_code::numeric_conversion, true, false},
                      view_case{"Null", 4U, false, 0, error_code::wrong_type_or_access, false, false},
                      view_case{"BeyondRange", 5U, false, 0, error_code::numeric_conversion, true, true}),
    [](const ::testing::TestParamInfo<view_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify an invalid default view is rejected by view-based numeric operations.
   * Arrange: Default-construct a value view.
   * Act: Convert and query it.
   * Expect: wrong_type_or_access from both operations.
   */
  TEST(J5_036_NumericView, RejectsInvalidView)
  {
    /* Arrange */
    const value_view view{};
    /* Act */
    const auto converted = to_integer<long>(view);
    const auto integral = is_integral_number(view);
    /* Expect */
    ASSERT_FALSE(converted.has_value());
    EXPECT_EQ(converted.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(integral.has_value());
    EXPECT_EQ(integral.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_036 ================================= */
} // namespace
