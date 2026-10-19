// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/number/decimal_lexer.hpp"
#include "adaptive_json5/detail/lexer/number/number_lexer.hpp"
#include "adaptive_json5/detail/lexer/scalar_lexer.hpp"
#include "adaptive_json5/detail/parser/primitive_parser.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ===================================== Fixtures ==================================== */
  /**
   * @brief Host storage with per-request failure injection and release validation.
   * @par Requirements
   * P032; J5-040; J5-046; J5-051.
   * @note Fixed records track allocations; only these test callbacks use malloc.
   */
  class primitive_provider
  {
    private:
      struct allocation_record
      {
          void* address{};
          std::size_t bytes{};
          std::size_t alignment{};
      };
      std::array<allocation_record, 64> m_records{};

      /**
       * @brief Allocate or inject failure on the selected request.
       * @par Requirements
       * P032; J5-046.
       */
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<primitive_provider*>(context);
        ++self.m_attempts;
        if (self.m_attempts == self.m_fail_on || alignment > alignof(std::max_align_t))
        {
          return nullptr;
        }
        for (auto& record : self.m_records)
        {
          if (record.address == nullptr)
          {
            record.address = std::malloc(bytes);
            if (record.address != nullptr)
            {
              record.bytes = bytes;
              record.alignment = alignment;
              ++self.m_allocations;
              self.m_live_bytes += bytes;
            }
            return record.address;
          }
        }
        return nullptr;
      }

      /**
       * @brief Verify and release one original allocation exactly once.
       * @par Requirements
       * P032; J5-040; J5-051.
       */
      static void release(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<primitive_provider*>(context);
        for (auto& record : self.m_records)
        {
          if (record.address == address && address != nullptr)
          {
            EXPECT_EQ(record.bytes, bytes);
            EXPECT_EQ(record.alignment, alignment);
            self.m_live_bytes -= record.bytes;
            ++self.m_releases;
            std::free(address);
            record = {};
            return;
          }
        }
        ADD_FAILURE() << "Unknown allocation or duplicate release";
      }

      /**
       * @brief Inject explicit errors or forward a successful host allocation.
       * @par Requirements
       * P032; J5-046; J5-051.
       */
      static result<void*> allocate_checked(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<primitive_provider*>(context);
        ++self.m_checked_attempts;
        if (self.m_checked_attempts == self.m_checked_error_on)
        {
          return result<void*>::failure(self.m_failure);
        }
        if (self.m_null_success)
        {
          return result<void*>::success(nullptr);
        }
        void* address = allocate(context, bytes, alignment);
        if (address == nullptr)
        {
          return result<void*>::failure(error::without_location(error_code::allocation_failure));
        }
        return result<void*>::success(address);
      }

    public:
      std::size_t m_checked_attempts{};
      std::size_t m_checked_error_on{};
      bool m_null_success{};
      error m_failure = error::at_offset(error_code::resource_limit, 73);
      std::size_t m_attempts{};
      std::size_t m_fail_on{};
      std::size_t m_allocations{};
      std::size_t m_releases{};
      std::size_t m_live_bytes{};

      /**
       * @brief Confirm owners released all allocations before provider destruction.
       * @par Requirements
       * P032; J5-040.
       */
      ~primitive_provider()
      {
        EXPECT_EQ(m_live_bytes, 0u);
        EXPECT_EQ(m_allocations, m_releases);
        for (const auto& record : m_records)
        {
          // Reclaim leaked test storage after reporting a production ownership failure.
          std::free(record.address);
        }
      }

      /**
       * @brief Borrow the test provider through the public callback interface.
       * @par Requirements
       * P032; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &release};
      }

      /**
       * @brief Expose result-only or dual callbacks for dispatch regression units.
       * @par Requirements
       * P032; J5-046.
       */
      storage_callbacks checked_callbacks(bool both = false) noexcept
      {
        return {this, both ? &allocate : nullptr, &release, &allocate_checked};
      }
  };

  /* =================================== End Fixtures ================================== */
  /* =================================== Test_J5_028 =================================== */
  static_assert(std::is_nothrow_move_constructible_v<primitive_document>);
  static_assert(!std::is_copy_constructible_v<primitive_document>);
  static_assert(noexcept(parse_primitive_document({}, std::declval<const parse_options&>(),
                                                  std::declval<accounted_storage&>())));
  static_assert(noexcept(std::declval<const value_view&>().source_span()));
  /**
   * @brief Independent spelling and decoded-value expectations for each root.
   * @par Requirements
   * P032; J5-023; J5-028; J5-034.
   */
  struct root_case
  {
      const char* name;     // Fixture label for diagnostics.
      std::string input;    // Input source bytes without surrounding trivia.
      value_kind kind;      // Expected stored type.
      std::u16string units; // Expected decoded string units when applicable.
      number_kind number;   // Expected numeric spelling family when applicable.
      bool boolean;         // Expected Boolean content when applicable.
      bool strict;          // Expected strict JSON acceptance.
  };
  class J5_028_PrimitiveRoot : public testing::TestWithParam<std::tuple<root_case, syntax_mode>>
  {
  };
  /*
   * Construct exact owned roots in both syntax modes.
   * Arrange: Choose an independent primitive fixture and mode.
   * Act: Parse the complete root through caller-owned accounting.
   * Expect: Exact content, kind, span and mode-specific rejection.
   */
  TEST_P(J5_028_PrimitiveRoot, ConstructsOwnedRoot)
  {
    /* Arrange */
    primitive_provider provider;
    const auto& input = std::get<0>(GetParam());
    SCOPED_TRACE(input.name);
    parse_options options{std::get<1>(GetParam())};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_primitive_document({input.input.data(), input.input.size()}, options, accounting);
    /* Expect */
    if (options.syntax == syntax_mode::strict_json && !input.strict)
    {
      ASSERT_FALSE(parsed.has_value());
      EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
      EXPECT_EQ(accounting.current_bytes(), 0u);
      return;
    }
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed.value_if()->values.size(), 1u);
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(*root.value_if()->kind().value_if(), input.kind);
    EXPECT_EQ(parsed.value_if()->root.byte_offset, 0u);
    EXPECT_EQ(parsed.value_if()->root.byte_length, input.input.size());
    const auto span = root.value_if()->source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ(span.value_if()->value().byte_offset, 0u);
    EXPECT_EQ(span.value_if()->value().byte_length, input.input.size());
    EXPECT_EQ(parsed.value_if()->root.warning_count, 0u);
    EXPECT_FALSE(parsed.value_if()->root.first_warning.has_value());
    if (input.kind == value_kind::boolean)
    {
      const auto value = root.value_if()->as_boolean();
      ASSERT_TRUE(value.has_value());
      EXPECT_EQ(*value.value_if(), input.boolean);
    }
    if (input.kind == value_kind::string)
    {
      const auto value = root.value_if()->as_string();
      ASSERT_TRUE(value.has_value());
      ASSERT_EQ((*value.value_if())->size(), input.units.size());
      for (std::size_t i{}; i < input.units.size(); ++i)
      {
        const auto unit = (*value.value_if())->unit_at(i);
        ASSERT_TRUE(unit.has_value());
        EXPECT_EQ(*unit.value_if(), input.units[i]);
      }
    }
    if (input.kind == value_kind::number)
    {
      const auto value = root.value_if()->as_number();
      ASSERT_TRUE(value.has_value());
      EXPECT_EQ((*value.value_if())->kind(), input.number);
      const auto spelling = (*value.value_if())->spelling();
      EXPECT_EQ(std::string(spelling.data, spelling.size), input.input);
    }
    EXPECT_EQ(accounting.current_bytes(), provider.m_live_bytes);
  }
  INSTANTIATE_TEST_SUITE_P(
    P032, J5_028_PrimitiveRoot,
    testing::Combine(
      testing::Values(root_case{"Null", "null", value_kind::null, {}, number_kind::decimal, false, true},
                      root_case{"True", "true", value_kind::boolean, {}, number_kind::decimal, true, true},
                      root_case{"False", "false", value_kind::boolean, {}, number_kind::decimal, false, true},
                      root_case{"EmptyString", "\"\"", value_kind::string, u"", number_kind::decimal, false, true},
                      root_case{"EscapedString", "\"a\\n\\u0000b\"", value_kind::string,
                                std::u16string{u'a', u'\n', u'\0', u'b'}, number_kind::decimal, false, true},
                      root_case{"LoneSurrogate", "\"\\uD800\"", value_kind::string,
                                std::u16string{static_cast<char16_t>(0xD800)}, number_kind::decimal, false, true},
                      root_case{"NegativeZero", "-0", value_kind::number, {}, number_kind::decimal, false, true},
                      root_case{
                        "LargeInteger", "9007199254740993", value_kind::number, {}, number_kind::decimal, false, true},
                      root_case{"Fraction", "1.2500", value_kind::number, {}, number_kind::decimal, false, true},
                      root_case{"Exponent", "1E+02", value_kind::number, {}, number_kind::decimal, false, true},
                      root_case{"SingleQuoted", "'x'", value_kind::string, u"x", number_kind::decimal, false, false},
                      root_case{"LeadingPlus", "+1", value_kind::number, {}, number_kind::decimal, false, false},
                      root_case{"LeadingPoint", ".5", value_kind::number, {}, number_kind::decimal, false, false},
                      root_case{"TrailingPoint", "1.", value_kind::number, {}, number_kind::decimal, false, false},
                      root_case{"Hexadecimal", "-0x10", value_kind::number, {}, number_kind::hexadecimal, false, false},
                      root_case{"Infinity", "-Infinity", value_kind::number, {}, number_kind::infinity, false, false},
                      root_case{"NaN", "+NaN", value_kind::number, {}, number_kind::nan, false, false}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)));
  /* ================================= End Test_J5_028 ================================= */
  /* =================================== Test_J5_026 =================================== */
  /**
   * @brief Invalid complete-root inputs with independent original error offsets.
   * @par Requirements
   * P032; J5-023; J5-024; J5-026.
   */
  struct invalid_case
  {
      std::string input;  // Input source bytes.
      std::size_t offset; // Expected original error byte offset.
  };
  class J5_026_PrimitiveInput : public testing::TestWithParam<std::tuple<invalid_case, syntax_mode>>
  {
  };
  /*
   * Reject invalid complete input without publishing a partial document.
   * Arrange: Choose malformed input and syntax mode.
   * Act: Parse through the accounting adapter.
   * Expect: Exact syntax location and no outstanding allocations.
   */
  TEST_P(J5_026_PrimitiveInput, RejectsInvalidCompleteInput)
  {
    /* Arrange */
    primitive_provider provider;
    const auto& input = std::get<0>(GetParam());
    parse_options options{std::get<1>(GetParam())};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_primitive_document({input.input.data(), input.input.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
    ASSERT_TRUE(parsed.error_if()->has_byte_offset);
    EXPECT_EQ(parsed.error_if()->byte_offset, input.offset);
    EXPECT_EQ(accounting.current_bytes(), 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  INSTANTIATE_TEST_SUITE_P(
    P032, J5_026_PrimitiveInput,
    testing::Combine(testing::Values(invalid_case{"", 0}, invalid_case{" \t\r\n", 4}, invalid_case{"[]", 0},
                                     invalid_case{"{}", 0}, invalid_case{"]", 0}, invalid_case{"true false", 5},
                                     invalid_case{"1 2", 2}, invalid_case{"\"x\" \"y\"", 4}, invalid_case{"true,", 4},
                                     invalid_case{"\"x\"}", 3}, invalid_case{"unknown", 0}, invalid_case{"TRUE", 0},
                                     invalid_case{"True", 0}, invalid_case{"truex", 0}, invalid_case{"1foo", 1},
                                     invalid_case{"01", 1}, invalid_case{"1e", 2}, invalid_case{"\"unterminated", 13},
                                     invalid_case{"\\u0074rue", 0}, invalid_case{std::string{"true\0", 5}, 4}),
                     testing::Values(syntax_mode::json5, syntax_mode::strict_json)));
  /*
   * HandlesModeSpecificCommentsAndCompleteSuffix.
   * Arrange: Prepare JSON5 comments around a Boolean root.
   * Act: Parse in JSON5, then strict JSON, and inspect malformed trailing comments.
   * Expect: Trivia is excluded from spans and strict comments remain errors.
   */
  TEST(J5_026_PrimitiveTrivia, HandlesModeSpecificCommentsAndCompleteSuffix)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options{};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const std::string text = "/*a*/ true //end";
    /* Act */
    const auto json5 = parse_primitive_document({text.data(), text.size()}, options, accounting);
    options.syntax = syntax_mode::strict_json;
    const auto strict = parse_primitive_document({text.data(), text.size()}, options, accounting);
    options.syntax = syntax_mode::json5;
    const auto trailing = parse_primitive_document({"true /*", 7}, options, accounting);
    const auto empty = parse_primitive_document({"/*x*/", 5}, options, accounting);
    /* Expect */
    ASSERT_TRUE(json5.has_value());
    EXPECT_EQ(json5.value_if()->root.byte_offset, 6u);
    EXPECT_EQ(json5.value_if()->root.byte_length, 4u);
    ASSERT_FALSE(strict.has_value());
    EXPECT_EQ(strict.error_if()->code, error_code::syntax);
    EXPECT_EQ(strict.error_if()->byte_offset, 0u);
    ASSERT_FALSE(trailing.has_value());
    EXPECT_EQ(trailing.error_if()->code, error_code::syntax);
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error_if()->byte_offset, 5u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_026 ================================= */
  /* =================================== Test_J5_041 =================================== */
  /*
   * ValidatesReplacesCopiesAndSurvivesGrowth.
   * Arrange: Create a manually constructed value with no source metadata.
   * Act: Set and copy a span, reject invalid replacements and force growth.
   * Expect: Failed replacements retain metadata and copied spans are independent.
   */
  TEST(J5_041_PrimitiveSpan, ValidatesReplacesCopiesAndSurvivesGrowth)
  {
    /* Arrange */
    primitive_provider provider;
    document doc{provider.callbacks(), 32};
    const auto index = doc.make_null();
    ASSERT_TRUE(index.has_value());
    const auto view = doc.view(*index.value_if());
    ASSERT_TRUE(view.has_value());
    const auto absent = view.value_if()->source_span();
    ASSERT_TRUE(absent.has_value());
    EXPECT_FALSE(absent.value_if()->has_value());
    /* Act */
    ASSERT_TRUE(doc.set_source_span(*index.value_if(), {2, 4}, 6).has_value());
    const auto copied = view.value_if()->source_span();
    ASSERT_TRUE(copied.has_value());
    EXPECT_FALSE(doc.set_source_span(*index.value_if(), {2, 5}, 6).has_value());
    EXPECT_FALSE(doc.set_source_span(*index.value_if(), {7, 0}, 6).has_value());
    EXPECT_FALSE(doc.set_source_span(*index.value_if(), {1, std::numeric_limits<std::size_t>::max()}, 6).has_value());
    EXPECT_FALSE(doc.set_source_span(value_index{}, {0, 0}, 0).has_value());
    EXPECT_FALSE(doc.set_source_span(value_index{32}, {0, 0}, 0).has_value());
    for (int i{}; i < 20; ++i)
    {
      ASSERT_TRUE(doc.make_null().has_value());
    }
    const auto retained = view.value_if()->source_span();
    /* Expect */
    ASSERT_TRUE(retained.has_value());
    ASSERT_TRUE(retained.value_if()->has_value());
    EXPECT_EQ(retained.value_if()->value().byte_offset, 2u);
    EXPECT_EQ(retained.value_if()->value().byte_length, 4u);
    ASSERT_TRUE(doc.set_source_span(*index.value_if(), {6, 0}, 6).has_value());
    ASSERT_TRUE(copied.value_if()->has_value());
    EXPECT_EQ(copied.value_if()->value().byte_length, 4u);
    const auto replaced = view.value_if()->source_span();
    ASSERT_TRUE(replaced.has_value());
    EXPECT_EQ(replaced.value_if()->value().byte_offset, 6u);
    EXPECT_EQ(replaced.value_if()->value().byte_length, 0u);
    value_view invalid;
    EXPECT_FALSE(invalid.source_span().has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_041 ================================= */
  /* =================================== Test_J5_047 =================================== */
  /**
   * @brief One-limit overrides with literal acceptance expectations.
   * @par Requirements
   * P032; J5-047; J5-049.
   */
  struct limit_case
  {
      const char* name;                    // Fixture name.
      std::string input;                   // Input source bytes.
      std::size_t resource_limits::*field; // Input limit to override.
      std::size_t limit;                   // Input configured ceiling.
      bool accepted;                       // Expected complete-root success.
  };
  class J5_047_PrimitiveLimit : public testing::TestWithParam<limit_case>
  {
  };
  /*
   * Enforce independently configured boundaries through the parser.
   * Arrange: Override one ceiling while leaving all others at defaults.
   * Act: Parse the selected primitive root.
   * Expect: Literal success or resource_limit with complete cleanup.
   */
  TEST_P(J5_047_PrimitiveLimit, EnforcesConfiguredBoundary)
  {
    /* Arrange */
    primitive_provider provider;
    const auto& input = GetParam();
    SCOPED_TRACE(input.name);
    parse_options options;
    options.limits.*(input.field) = input.limit;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    {
      const auto parsed = parse_primitive_document({input.input.data(), input.input.size()}, options, accounting);
      /* Expect */
      EXPECT_EQ(parsed.has_value(), input.accepted);
      if (!input.accepted)
      {
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
        EXPECT_EQ(accounting.current_bytes(), 0u);
      }
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  INSTANTIATE_TEST_SUITE_P(
    P032, J5_047_PrimitiveLimit,
    testing::Values(
      limit_case{"InputBelow", "true", &resource_limits::max_input_bytes, 3, false},
      limit_case{"InputExact", "true", &resource_limits::max_input_bytes, 4, true},
      limit_case{"InputAbove", "true", &resource_limits::max_input_bytes, 5, true},
      limit_case{"NoNodes", "true", &resource_limits::max_nodes, 0, false},
      limit_case{"OneNode", "true", &resource_limits::max_nodes, 1, true},
      limit_case{"NoContainersNeeded", "true", &resource_limits::max_depth, 0, true},
      limit_case{"NoMembersNeeded", "true", &resource_limits::max_members, 0, true},
      limit_case{"StringBelow", "\"ab\"", &resource_limits::max_string_units, 1, false},
      limit_case{"StringExact", "\"ab\"", &resource_limits::max_string_units, 2, true},
      limit_case{"StringAbove", "\"ab\"", &resource_limits::max_string_units, 3, true},
      limit_case{"TotalBelow", "\"ab\"", &resource_limits::max_total_string_units, 1, false},
      limit_case{"TotalExact", "\"ab\"", &resource_limits::max_total_string_units, 2, true},
      limit_case{"TotalAbove", "\"ab\"", &resource_limits::max_total_string_units, 3, true},
      limit_case{"EmptyStringZero", "\"\"", &resource_limits::max_total_string_units, 0, true},
      limit_case{"LiteralZeroString", "false", &resource_limits::max_string_units, 0, true},
      limit_case{"LiteralZeroTotal", "null", &resource_limits::max_total_string_units, 0, true},
      limit_case{"NumberBelow", "123", &resource_limits::max_number_bytes, 2, false},
      limit_case{"NumberExact", "123", &resource_limits::max_number_bytes, 3, true},
      limit_case{"NumberAbove", "123", &resource_limits::max_number_bytes, 4, true},
      limit_case{"ExponentBelow", "1e+012", &resource_limits::max_exponent_digits, 2, false},
      limit_case{"ExponentExact", "1e+012", &resource_limits::max_exponent_digits, 3, true},
      limit_case{"ExponentAbove", "1e+012", &resource_limits::max_exponent_digits, 4, true},
      limit_case{"ZeroExponentDigits", "1e0", &resource_limits::max_exponent_digits, 0, false},
      limit_case{"ExponentFree", "1", &resource_limits::max_exponent_digits, 0, true},
      limit_case{"HexDigitsAreNotExponent", "0xE1", &resource_limits::max_exponent_digits, 0, true},
      limit_case{"WorkZero", "true", &resource_limits::max_work_units, 0, false},
      limit_case{"WorkBeforeLexing", "true", &resource_limits::max_work_units, 4, false},
      limit_case{"WorkBeforeConstruction", "true", &resource_limits::max_work_units, 12, false},
      limit_case{"WorkExact", "true", &resource_limits::max_work_units, 13, true},
      limit_case{"WorkAbove", "true", &resource_limits::max_work_units, 14, true},
      limit_case{"WorkTrailingDenied", " true ", &resource_limits::max_work_units, 16, false},
      limit_case{"WorkTrailingExact", " true ", &resource_limits::max_work_units, 17, true},
      limit_case{"NoWarningDiagnostic", "\"x\"", &resource_limits::max_diagnostics, 0, true},
      limit_case{"WarningDiagnosticDenied", "\"\xE2\x80\xA8\"", &resource_limits::max_diagnostics, 0, false},
      limit_case{"OneRetainedWarning", "\"\xE2\x80\xA8\xE2\x80\xA9\"", &resource_limits::max_diagnostics, 1, true},
      limit_case{"NoOwnedBytes", "1", &resource_limits::max_owned_bytes, 0, false}));
  /*
   * ChecksPeakBoundaryAndSharedOutstandingBytes.
   * Arrange: Measure a representative string parse peak with the fixed-record provider.
   * Act: Repeat with byte limits immediately below, equal to and above the measured peak.
   * Expect: A separately live allocation shares the same inclusive ceiling.
   */
  TEST(J5_047_PrimitiveBytes, ChecksPeakBoundaryAndSharedOutstandingBytes)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    const std::string input = "\"abcdef\"";
    std::size_t peak{};
    {
      accounted_storage measured{provider.callbacks(), options.limits.max_owned_bytes};
      {
        const auto parsed = parse_primitive_document({input.data(), input.size()}, options, measured);
        ASSERT_TRUE(parsed.has_value());
        peak = measured.peak_bytes();
      }
      EXPECT_EQ(measured.current_bytes(), 0u);
    }
    ASSERT_GT(peak, 0u);
    /* Act */
    for (const auto limit : std::array<std::size_t, 3>{peak - 1, peak, peak + 1})
    {
      options.limits.max_owned_bytes = limit;
      accounted_storage accounting{provider.callbacks(), limit};
      {
        const auto parsed = parse_primitive_document({input.data(), input.size()}, options, accounting);
        if (limit < peak)
        {
          ASSERT_FALSE(parsed.has_value());
          EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
        }
        else
        {
          EXPECT_TRUE(parsed.has_value());
        }
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
    /* Expect */
    options.limits.max_owned_bytes = peak;
    accounted_storage accounting{provider.callbacks(), peak};
    {
      const auto held = accounting.allocate(1, 1);
      ASSERT_TRUE(held.has_value());
      const auto denied = parse_primitive_document({input.data(), input.size()}, options, accounting);
      ASSERT_FALSE(denied.has_value());
      EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(accounting.current_bytes(), 1u);
    }
    const auto retry = parse_primitive_document({input.data(), input.size()}, options, accounting);
    EXPECT_TRUE(retry.has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * RejectsInvalidViewsAndMismatchedAccounting.
   * Arrange: Prepare valid accounting and a different configured byte limit.
   * Act: Reject a null nonempty view and inconsistent allocation ceiling.
   * Expect: Configuration errors precede provider calls and carry no fabricated location.
   */
  TEST(J5_047_PrimitiveConfiguration, RejectsInvalidViewsAndMismatchedAccounting)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto invalid = parse_primitive_document({nullptr, 1}, options, accounting);
    --options.limits.max_owned_bytes;
    const auto mismatch = parse_primitive_document({"1", 1}, options, accounting);
    /* Expect */
    ASSERT_FALSE(invalid.has_value());
    ASSERT_FALSE(mismatch.has_value());
    EXPECT_EQ(invalid.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(mismatch.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_FALSE(invalid.error_if()->has_byte_offset);
    EXPECT_FALSE(mismatch.error_if()->has_byte_offset);
    EXPECT_EQ(provider.m_attempts, 0u);
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_047 ================================= */
  /* =================================== Test_J5_017 =================================== */
  /*
   * RetainsCountAndFirstOriginalSpan.
   * Arrange: Prepare JSON5 source with two raw separator warnings.
   * Act: Parse while retaining only the first descriptor.
   * Expect: The count and first descriptor retain exact source offsets.
   */
  TEST(J5_017_PrimitiveWarnings, RetainsCountAndFirstOriginalSpan)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    options.limits.max_diagnostics = 1;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const std::string text = "  \"\xE2\x80\xA8\xE2\x80\xA9\" ";
    /* Act */
    const auto parsed = parse_primitive_document({text.data(), text.size()}, options, accounting);
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed.value_if()->root.warning_count, 2u);
    ASSERT_TRUE(parsed.value_if()->root.first_warning.has_value());
    EXPECT_EQ(parsed.value_if()->root.first_warning->code, string_warning_code::raw_line_separator);
    EXPECT_EQ(parsed.value_if()->root.first_warning->byte_offset, 3u);
    EXPECT_EQ(parsed.value_if()->root.first_warning->byte_length, 3u);
    EXPECT_EQ(parsed.value_if()->root.byte_offset, 2u);
    EXPECT_EQ(parsed.value_if()->root.byte_length, 8u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_017 ================================= */
  /* =================================== Test_J5_040 =================================== */
  /*
   * RetainsValuesAndSpansAfterInputDestructionAndMoves.
   * Arrange: Parse owned string and number roots through one persistent accounting object.
   * Act: Move both owners and acquire new views from their destinations.
   * Expect: Owned content and persistent spans survive without retaining input.
   */
  TEST(J5_040_PrimitiveOwnership, RetainsValuesAndSpansAfterInputDestructionAndMoves)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    auto string_result = [&]() noexcept
    {
      std::string text = "  \"abc\" ";
      auto parsed = parse_primitive_document({text.data(), text.size()}, options, accounting);
      text.assign(text.size(), 'x');
      return parsed;
    }();
    auto number_result = [&]() noexcept
    {
      std::string text = " 9007199254740993 ";
      auto parsed = parse_primitive_document({text.data(), text.size()}, options, accounting);
      text.assign(text.size(), 'x');
      return parsed;
    }();
    ASSERT_TRUE(string_result.has_value());
    ASSERT_TRUE(number_result.has_value());
    const auto bytes = accounting.current_bytes();
    /* Act */
    primitive_document string_owner{std::move(*string_result.value_if())};
    primitive_document number_owner{std::move(*number_result.value_if())};
    const auto string = string_owner.values.root();
    const auto number = number_owner.values.root();
    ASSERT_TRUE(string.has_value());
    ASSERT_TRUE(number.has_value());
    /* Expect */
    const auto content = string.value_if()->as_string();
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ((*content.value_if())->size(), 3u);
    EXPECT_EQ(*(*content.value_if())->unit_at(0).value_if(), u'a');
    const auto numeric = number.value_if()->as_number();
    ASSERT_TRUE(numeric.has_value());
    const auto spelling = (*numeric.value_if())->spelling();
    EXPECT_EQ(std::string(spelling.data, spelling.size), "9007199254740993");
    const auto span = number.value_if()->source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ(span.value_if()->value().byte_offset, 1u);
    EXPECT_EQ(span.value_if()->value().byte_length, 16u);
    EXPECT_EQ(accounting.current_bytes(), bytes);
    EXPECT_EQ(string_result.value_if()->values.size(), 0u);
    EXPECT_EQ(number_result.value_if()->values.size(), 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_040 ================================= */
  /* =================================== Test_J5_051 =================================== */
  class J5_051_PrimitiveFailure : public testing::TestWithParam<const char*>
  {
  };
  /*
   * Every reachable host allocation fails cleanly and permits retry.
   * Arrange: Measure the representative root allocation count.
   * Act: Fail each allocation independently while another document remains live.
   * Expect: Exact allocation failure, retained baseline accounting and successful reuse.
   */
  TEST_P(J5_051_PrimitiveFailure, FailsEachAllocationAndAllowsRetry)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const std::string input = GetParam();
    const auto held = parse_primitive_document({"1", 1}, options, accounting);
    ASSERT_TRUE(held.has_value());
    const auto baseline = accounting.current_bytes();
    std::size_t requests{};
    {
      const auto before = provider.m_attempts;
      const auto measured = parse_primitive_document({input.data(), input.size()}, options, accounting);
      ASSERT_TRUE(measured.has_value());
      requests = provider.m_attempts - before;
    }
    ASSERT_GT(requests, 0u);
    EXPECT_EQ(accounting.current_bytes(), baseline);
    /* Act */
    for (std::size_t fail = 1; fail <= requests; ++fail)
    {
      provider.m_fail_on = provider.m_attempts + fail;
      const auto failed = parse_primitive_document({input.data(), input.size()}, options, accounting);
      /* Expect */
      ASSERT_FALSE(failed.has_value()) << fail;
      EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
      EXPECT_EQ(accounting.current_bytes(), baseline);
      EXPECT_EQ(provider.m_live_bytes, baseline);
      const auto root = held.value_if()->values.root();
      ASSERT_TRUE(root.has_value());
      EXPECT_EQ(*root.value_if()->kind().value_if(), value_kind::number);
      provider.m_fail_on = 0;
      {
        const auto retry = parse_primitive_document({input.data(), input.size()}, options, accounting);
        ASSERT_TRUE(retry.has_value());
      }
      EXPECT_EQ(accounting.current_bytes(), baseline);
    }
  }
  INSTANTIATE_TEST_SUITE_P(P032, J5_051_PrimitiveFailure,
                           testing::Values("true", "false", "null", "9007199254740993",
                                           "\"a long string forcing several growth allocations\""));
  /*
   * PreservesCursorAndDestinationAcrossRecognitionAndConstructionFailures.
   * Arrange: Hold one Boolean and parse from a nonzero input offset.
   * Act: Deny recognition, construction and record-growth allocations separately.
   * Expect: Failures publish no appended record, preserve old content and release temporaries.
   */
  TEST(J5_051_PrimitiveTransaction, PreservesCursorAndDestinationAcrossRecognitionAndConstructionFailures)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    document doc{accounting.callbacks(), 16};
    const auto held = doc.make_boolean(true);
    ASSERT_TRUE(held.has_value());
    const std::string input = " 123";
    const auto baseline = accounting.current_bytes();
    /* Act */
    {
      cursor source{{input.data(), input.size()}};
      source.advance(1);
      work_budget work{2};
      const auto denied =
        consume_primitive(doc, source, {input.data(), input.size()}, options, accounting.callbacks(), work);
      ASSERT_FALSE(denied.has_value());
      EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(source.offset(), 1u);
      EXPECT_EQ(doc.size(), 1u);
    }
    {
      cursor source{{input.data(), input.size()}};
      source.advance(1);
      work_budget work{3};
      const auto denied =
        consume_primitive(doc, source, {input.data(), input.size()}, options, accounting.callbacks(), work);
      ASSERT_FALSE(denied.has_value());
      EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(source.offset(), 1u);
      EXPECT_EQ(work.consumed(), 3u);
      EXPECT_EQ(doc.size(), 1u);
    }
    {
      cursor source{{input.data(), input.size()}};
      source.advance(1);
      work_budget work{100};
      provider.m_fail_on = provider.m_attempts + 2;
      const auto failed =
        consume_primitive(doc, source, {input.data(), input.size()}, options, accounting.callbacks(), work);
      ASSERT_FALSE(failed.has_value());
      EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
      EXPECT_EQ(source.offset(), 1u);
      EXPECT_EQ(doc.size(), 1u);
      provider.m_fail_on = 0;
    }
    /* Expect */
    EXPECT_EQ(accounting.current_bytes(), baseline);
    const auto old = doc.view(*held.value_if());
    ASSERT_TRUE(old.has_value());
    EXPECT_TRUE(*old.value_if()->as_boolean().value_if());
    cursor source{{input.data(), input.size()}};
    source.advance(1);
    work_budget work{100};
    const auto retry =
      consume_primitive(doc, source, {input.data(), input.size()}, options, accounting.callbacks(), work);
    ASSERT_TRUE(retry.has_value());
    ASSERT_TRUE(retry.value_if()->has_value());
    EXPECT_EQ(source.offset(), 4u);
    EXPECT_EQ(doc.size(), 2u);
    EXPECT_EQ(retry.value_if()->value().byte_offset, 1u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_051 ================================= */
  /* =================================== Test_J5_046 =================================== */
  /*
   * PrefersResultCallbackAndPreservesExplicitError.
   * Arrange: Expose both callbacks and inject a located resource error.
   * Act: Allocate through dispatch while the preferred callback fails.
   * Expect: No fallback is called and code/location remain unchanged.
   */
  TEST(J5_046_ResultStorage, PrefersResultCallbackAndPreservesExplicitError)
  {
    /* Arrange */
    primitive_provider provider;
    provider.m_checked_error_on = 1;
    const auto callbacks = provider.checked_callbacks(true);
    /* Act */
    const auto failed = allocate_block(callbacks, 8, 1);
    /* Expect */
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::resource_limit);
    EXPECT_TRUE(failed.error_if()->has_byte_offset);
    EXPECT_EQ(failed.error_if()->byte_offset, 73u);
    EXPECT_EQ(provider.m_checked_attempts, 1u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * AcceptsResultOnlyAndRejectsNullSuccess.
   * Arrange: Expose a result-only provider.
   * Act: Allocate one owned block, move it and inject null success.
   * Expect: Ownership releases once and successful null addresses become allocation failures.
   */
  TEST(J5_046_ResultStorage, AcceptsResultOnlyAndRejectsNullSuccess)
  {
    /* Arrange */
    primitive_provider provider;
    const auto callbacks = provider.checked_callbacks();
    /* Act */
    {
      auto block = allocate_array(callbacks, 4, 2, 2);
      ASSERT_TRUE(block.has_value());
      owned_block moved{std::move(*block.value_if())};
      EXPECT_EQ(moved.size(), 8u);
      EXPECT_EQ(block.value_if()->size(), 0u);
    }
    provider.m_null_success = true;
    const auto null = allocate_block(callbacks, 1, 1);
    /* Expect */
    ASSERT_FALSE(null.has_value());
    EXPECT_EQ(null.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
    const auto calls = provider.m_checked_attempts;
    EXPECT_TRUE(allocate_block(callbacks, 0, 1).has_value());
    EXPECT_FALSE(allocate_block(callbacks, 1, 3).has_value());
    EXPECT_EQ(provider.m_checked_attempts, calls);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * AccountsResultOnlyUpstreamAndNestedAdapters.
   * Arrange: Nest accounting layers over a result-only host provider.
   * Act: Allocate through both layers and exceed the upstream ceiling.
   * Expect: Both layers release matching bytes and preserve independent peaks.
   */
  TEST(J5_046_ResultStorage, AccountsResultOnlyUpstreamAndNestedAdapters)
  {
    /* Arrange */
    primitive_provider provider;
    accounted_storage upstream{provider.checked_callbacks(), 12};
    accounted_storage downstream{upstream.callbacks(), 16};
    /* Act */
    {
      const auto held = downstream.allocate(8, 1);
      ASSERT_TRUE(held.has_value());
      EXPECT_EQ(upstream.current_bytes(), 8u);
      EXPECT_EQ(downstream.current_bytes(), 8u);
      const auto denied = downstream.allocate(5, 1);
      ASSERT_FALSE(denied.has_value());
      EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(upstream.current_bytes(), 8u);
      EXPECT_EQ(downstream.current_bytes(), 8u);
    }
    /* Expect */
    EXPECT_EQ(upstream.current_bytes(), 0u);
    EXPECT_EQ(downstream.current_bytes(), 0u);
    EXPECT_EQ(upstream.peak_bytes(), 8u);
    EXPECT_EQ(downstream.peak_bytes(), 8u);
    EXPECT_EQ(provider.m_checked_attempts, 1u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * DoesNotRebaseLocatedAllocationFailures.
   * Arrange: Fail numeric spelling allocation with a provider-origin location beyond the substring.
   * Act: Parse a number starting at byte ten.
   * Expect: The resource code survives and allocation location is deliberately removed.
   */
  TEST(J5_046_PrimitiveAllocation, DoesNotRebaseLocatedAllocationFailures)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    accounted_storage accounting{provider.checked_callbacks(), options.limits.max_owned_bytes};
    const std::string input = "          123";
    provider.m_failure = error::at_offset(error_code::resource_limit, 10);
    provider.m_checked_error_on = 1;
    /* Act */
    const auto parsed = parse_primitive_document({input.data(), input.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
    EXPECT_FALSE(parsed.error_if()->has_byte_offset);
    EXPECT_EQ(parsed.error_if()->byte_offset, 0u);
    EXPECT_EQ(accounting.current_bytes(), 0u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * RejectsMissingProviderAndCleansTrailingFailures.
   * Arrange: Use empty storage and a valid accounting context.
   * Act: Attempt missing storage and fail after constructing a string.
   * Expect: Explicit errors leave no outstanding temporary ownership.
   */
  TEST(J5_046_PrimitiveAllocation, RejectsMissingProviderAndCleansTrailingFailures)
  {
    /* Arrange */
    parse_options options;
    accounted_storage missing{{}, options.limits.max_owned_bytes};
    primitive_provider provider;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto absent = parse_primitive_document({"1", 1}, options, missing);
    const std::string extra = "\"abc\" false";
    const auto trailing = parse_primitive_document({extra.data(), extra.size()}, options, accounting);
    /* Expect */
    ASSERT_FALSE(absent.has_value());
    EXPECT_EQ(absent.error_if()->code, error_code::allocation_failure);
    ASSERT_FALSE(trailing.has_value());
    EXPECT_EQ(trailing.error_if()->code, error_code::syntax);
    EXPECT_EQ(accounting.current_bytes(), 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_046 ================================= */
  /* =================================== Test_J5_024 =================================== */
  /*
   * RebasesValidationErrorsAndRejectsInvalidNumberRanges.
   * Arrange: Prepare a numeric metadata token and existing document.
   * Act: Reject a numeric length limit, invalid ranges and inconsistent kinds.
   * Expect: Only substring-relative validation errors are rebased and no record is appended.
   */
  TEST(J5_024_PrimitiveMetadata, RebasesValidationErrorsAndRejectsInvalidNumberRanges)
  {
    /* Arrange */
    primitive_provider provider;
    parse_options options;
    document doc{provider.callbacks(), 8};
    const std::string input = " 123";
    options.limits.max_number_bytes = 2;
    /* Act */
    const auto limited = detail::append_primitive_token(doc, scalar_token{number_token{number_kind::decimal, 1, 3}},
                                                        {input.data(), input.size()}, options, provider.callbacks());
    options.limits.max_number_bytes = 10;
    const auto offset = detail::append_primitive_token(doc, scalar_token{number_token{number_kind::decimal, 5, 1}},
                                                       {input.data(), input.size()}, options, provider.callbacks());
    const auto length = detail::append_primitive_token(
      doc, scalar_token{number_token{number_kind::decimal, 1, std::numeric_limits<std::size_t>::max()}},
      {input.data(), input.size()}, options, provider.callbacks());
    const auto kind = detail::append_primitive_token(doc, scalar_token{number_token{number_kind::hexadecimal, 1, 3}},
                                                     {input.data(), input.size()}, options, provider.callbacks());
    /* Expect */
    ASSERT_FALSE(limited.has_value());
    EXPECT_EQ(limited.error_if()->code, error_code::resource_limit);
    EXPECT_TRUE(limited.error_if()->has_byte_offset);
    EXPECT_EQ(limited.error_if()->byte_offset, 1u);
    ASSERT_FALSE(offset.has_value());
    ASSERT_FALSE(length.has_value());
    ASSERT_FALSE(kind.has_value());
    EXPECT_EQ(offset.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(length.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(kind.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(doc.size(), 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * PreservesCursorAtPunctuationAndEnd.
   * Arrange: Create an empty destination and shared work budget.
   * Act: Consume punctuation and empty input as primitive no-match.
   * Expect: Both optionals are empty and no cursor, record or allocation is published.
   */
  TEST(J5_024_PrimitiveNoMatch, PreservesCursorAtPunctuationAndEnd)
  {
    /* Arrange */
    primitive_provider provider;
    document doc{provider.callbacks(), 2};
    parse_options options;
    work_budget work{100};
    const std::string text = "[";
    cursor punctuation{{text.data(), text.size()}};
    cursor empty;
    /* Act */
    const auto first =
      consume_primitive(doc, punctuation, {text.data(), text.size()}, options, provider.callbacks(), work);
    const auto end = consume_primitive(doc, empty, {}, options, provider.callbacks(), work);
    /* Expect */
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(end.has_value());
    EXPECT_FALSE(first.value_if()->has_value());
    EXPECT_FALSE(end.value_if()->has_value());
    EXPECT_EQ(punctuation.offset(), 0u);
    EXPECT_EQ(empty.offset(), 0u);
    EXPECT_EQ(doc.size(), 0u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_024 ================================= */
  /* =================================== Test_J5_047 =================================== */
  /**
   * @brief Direct numeric entry-point exponent boundaries.
   * @par Requirements
   * P032; J5-047.
   */
  struct exponent_case
  {
      const char* input;  // Input numeric spelling.
      std::size_t limit;  // Input maximum exponent digits.
      bool accepted;      // Expected recognition success.
      std::size_t offset; // Expected first excess digit byte offset on failure.
  };
  class J5_047_ExponentEntry : public testing::TestWithParam<std::tuple<exponent_case, syntax_mode>>
  {
  };
  /*
   * Direct decimal, number and scalar entry points share exponent accounting.
   * Arrange: Select a literal boundary case and syntax mode.
   * Act: Recognize independently through each limited entry point.
   * Expect: Same acceptance or located resource failure without cursor advancement.
   */
  TEST_P(J5_047_ExponentEntry, ForwardsLimitAndPreservesCursor)
  {
    /* Arrange */
    primitive_provider provider;
    const auto input = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    const std::string text = input.input;
    cursor decimal{{text.data(), text.size()}};
    cursor number{{text.data(), text.size()}};
    cursor scalar{{text.data(), text.size()}};
    /* Act */
    const auto first = consume_decimal(decimal, mode, input.limit);
    const auto second = consume_number(number, mode, input.limit);
    const auto third = consume_scalar(scalar, mode, provider.callbacks(), 0, text.size(), input.limit);
    /* Expect */
    EXPECT_EQ(first.has_value(), input.accepted);
    EXPECT_EQ(second.has_value(), input.accepted);
    EXPECT_EQ(third.has_value(), input.accepted);
    if (input.accepted)
    {
      ASSERT_TRUE(first.has_value());
      ASSERT_TRUE(first.value_if()->has_value());
      ASSERT_TRUE(second.has_value());
      ASSERT_TRUE(second.value_if()->has_value());
      ASSERT_TRUE(third.has_value());
      ASSERT_TRUE(third.value_if()->has_value());
      EXPECT_EQ(decimal.offset(), text.size());
      EXPECT_EQ(number.offset(), text.size());
      EXPECT_EQ(scalar.offset(), text.size());
    }
    else
    {
      ASSERT_FALSE(first.has_value());
      ASSERT_FALSE(second.has_value());
      ASSERT_FALSE(third.has_value());
      EXPECT_EQ(first.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(second.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(third.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(first.error_if()->byte_offset, input.offset);
      EXPECT_EQ(second.error_if()->byte_offset, input.offset);
      EXPECT_EQ(third.error_if()->byte_offset, input.offset);
      EXPECT_EQ(decimal.offset(), 0u);
      EXPECT_EQ(number.offset(), 0u);
      EXPECT_EQ(scalar.offset(), 0u);
    }
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  INSTANTIATE_TEST_SUITE_P(
    P032, J5_047_ExponentEntry,
    testing::Combine(testing::Values(exponent_case{"1", 0, true, 0}, exponent_case{"1e0", 0, false, 2},
                                     exponent_case{"1e12", 2, true, 0}, exponent_case{"1e123", 2, false, 4},
                                     exponent_case{"1e+12", 2, true, 0}, exponent_case{"1e-001", 2, false, 5},
                                     exponent_case{"1E001", 3, true, 0}),
                     testing::Values(syntax_mode::json5, syntax_mode::strict_json)));
  /* ================================= End Test_J5_047 ================================= */
  /* =================================== Test_J5_046 =================================== */
  /*
   * Result-only providers still require a paired release operation.
   * Arrange: Remove deallocation from a result-only provider.
   * Act: Request empty and nonempty blocks.
   * Expect: Explicit allocation failures without invoking the provider.
   */
  TEST(J5_046_ResultStorage, RequiresDeallocationForResultOnlyProvider)
  {
    /* Arrange */
    primitive_provider provider;
    auto callbacks = provider.checked_callbacks();
    callbacks.deallocate = nullptr;
    /* Act */
    const auto empty = allocate_block(callbacks, 0, 1);
    const auto nonempty = allocate_block(callbacks, 1, 1);
    /* Expect */
    ASSERT_FALSE(empty.has_value());
    ASSERT_FALSE(nonempty.has_value());
    EXPECT_EQ(empty.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(nonempty.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(provider.m_checked_attempts, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Accounted adapters preserve explicit upstream errors without updating counters.
   * Arrange: Expose both host callbacks and inject a located error.
   * Act: Allocate through an accounting adapter.
   * Expect: Original location/category, no fallback and unchanged current/peak bytes.
   */
  TEST(J5_046_ResultStorage, PreservesLocatedUpstreamFailureThroughAccounting)
  {
    /* Arrange */
    primitive_provider provider;
    provider.m_checked_error_on = 1;
    accounted_storage accounting{provider.checked_callbacks(true), 8};
    /* Act */
    const auto failed = accounting.allocate(8, 1);
    /* Expect */
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::resource_limit);
    EXPECT_TRUE(failed.error_if()->has_byte_offset);
    EXPECT_EQ(failed.error_if()->byte_offset, 73u);
    EXPECT_EQ(provider.m_checked_attempts, 1u);
    EXPECT_EQ(provider.m_attempts, 0u);
    EXPECT_EQ(accounting.current_bytes(), 0u);
    EXPECT_EQ(accounting.peak_bytes(), 0u);
  }
  /* ================================= End Test_J5_046 ================================= */
} // namespace
