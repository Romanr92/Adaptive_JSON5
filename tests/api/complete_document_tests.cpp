#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/parser.hpp"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_026 ==================================== */
  /**
   * @brief Specify public-entry examples independently of internal parser state.
   * @par Requirements
   * P037; J5-026; J5-028; J5-041.
   */
  struct root_case
  {
      const char* name;     // Input case identifier.
      const char* input;    // Input complete source bytes.
      syntax_mode mode;     // Input language selection.
      value_kind kind;      // Expected root type.
      source_span span;     // Expected root range in original bytes, excluding trivia.
      std::size_t records;  // Expected total document records.
      std::size_t children; // Expected immediate children for a container root.
  };
  class J5_026_CompleteRoot : public testing::TestWithParam<root_case>
  {
  };
  /*
   * Arrange: Every root type with independently specified spans, counts and syntax modes.
   * Act: Call only the supported complete-document API.
   * Expect: An established root of the right type, exact payload and no retained trivia.
   */
  TEST_P(J5_026_CompleteRoot, AcceptsEveryTypeAndSurroundingTrivia)
  {
    /* Arrange */
    const auto fixture = GetParam();
    document_provider provider;
    parse_options options{fixture.mode};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const std::string_view source{fixture.input};
    /* Act */
    const auto parsed = parse_document({source.data(), source.size()}, options, accounting);
    /* Expect */
    // The public entry must validate the suffix without adding trivia to a value's source range.
    ASSERT_TRUE(parsed.has_value()) << fixture.name;
    EXPECT_EQ(parsed.error_if(), nullptr);
    EXPECT_EQ(parsed.value_if()->values.size(), fixture.records);
    EXPECT_EQ(parsed.value_if()->warnings.size(), 0u);
    EXPECT_FALSE(parsed.value_if()->source.is_retained());
    const auto root = parsed.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto kind = root.value_if()->kind();
    ASSERT_TRUE(kind.has_value());
    EXPECT_EQ(*kind.value_if(), fixture.kind);
    const auto span = root.value_if()->source_span();
    ASSERT_TRUE(span.has_value());
    ASSERT_TRUE(span.value_if()->has_value());
    EXPECT_EQ((**span.value_if()).byte_offset, fixture.span.byte_offset);
    EXPECT_EQ((**span.value_if()).byte_length, fixture.span.byte_length);
    if (fixture.kind == value_kind::array || fixture.kind == value_kind::object)
    {
      const auto count = root.value_if()->container_size();
      ASSERT_TRUE(count.has_value());
      EXPECT_EQ(*count.value_if(), fixture.children);
    }
    else if (fixture.kind == value_kind::boolean)
    {
      const auto boolean = root.value_if()->as_boolean();
      ASSERT_TRUE(boolean.has_value());
      EXPECT_EQ(*boolean.value_if(), source.substr(fixture.span.byte_offset, fixture.span.byte_length) == "true");
    }
    else if (fixture.kind == value_kind::number)
    {
      const auto number = root.value_if()->as_number();
      ASSERT_TRUE(number.has_value());
      const auto spelling = (*number.value_if())->spelling();
      EXPECT_EQ(std::string_view(spelling.data, spelling.size),
                source.substr(fixture.span.byte_offset, fixture.span.byte_length));
    }
    else if (fixture.kind == value_kind::string)
    {
      const auto text = root.value_if()->as_string();
      ASSERT_TRUE(text.has_value());
      ASSERT_EQ((*text.value_if())->size(), 1u);
      const auto unit = (*text.value_if())->unit_at(0);
      ASSERT_TRUE(unit.has_value());
      EXPECT_EQ(*unit.value_if(), u'x');
    }
  }
  INSTANTIATE_TEST_SUITE_P(
    P037, J5_026_CompleteRoot,
    testing::Values(
      root_case{"NullJson5", "null", syntax_mode::json5, value_kind::null, {0, 4}, 1, 0},
      root_case{"TrueJson5", " true ", syntax_mode::json5, value_kind::boolean, {1, 4}, 1, 0},
      root_case{"FalseJson5", "false", syntax_mode::json5, value_kind::boolean, {0, 5}, 1, 0},
      root_case{"StringJson5", " 'x' ", syntax_mode::json5, value_kind::string, {1, 3}, 1, 0},
      root_case{"DecimalJson5", " -1.25e+2 ", syntax_mode::json5, value_kind::number, {1, 8}, 1, 0},
      root_case{"HexJson5", "+0x2a", syntax_mode::json5, value_kind::number, {0, 5}, 1, 0},
      root_case{"InfinityJson5", "Infinity", syntax_mode::json5, value_kind::number, {0, 8}, 1, 0},
      root_case{"NaNJson5", "-NaN", syntax_mode::json5, value_kind::number, {0, 4}, 1, 0},
      root_case{"ArrayJson5", " [1,true,] ", syntax_mode::json5, value_kind::array, {1, 9}, 3, 2},
      root_case{"ObjectJson5", " {a:[{}]} ", syntax_mode::json5, value_kind::object, {1, 8}, 3, 1},
      root_case{"EofLineComment", "{}// done", syntax_mode::json5, value_kind::object, {0, 2}, 1, 0},
      root_case{"BlockComments", "/*a*/null/*b*/", syntax_mode::json5, value_kind::null, {5, 4}, 1, 0},
      root_case{"EofCommentAfterLine", "[]\r\n// done", syntax_mode::json5, value_kind::array, {0, 2}, 1, 0},
      root_case{"UnicodeTrivia", "\xc2\xa0{}\xe2\x80\xa8", syntax_mode::json5, value_kind::object, {2, 2}, 1, 0},
      root_case{"NullStrict", "\t null\r\n", syntax_mode::strict_json, value_kind::null, {2, 4}, 1, 0},
      root_case{"TrueStrict", "true", syntax_mode::strict_json, value_kind::boolean, {0, 4}, 1, 0},
      root_case{"FalseStrict", " false ", syntax_mode::strict_json, value_kind::boolean, {1, 5}, 1, 0},
      root_case{"StringStrict", " \"x\" ", syntax_mode::strict_json, value_kind::string, {1, 3}, 1, 0},
      root_case{"NumberStrict", " -1.25e+2\t", syntax_mode::strict_json, value_kind::number, {1, 8}, 1, 0},
      root_case{"ArrayStrict", " [1,true] ", syntax_mode::strict_json, value_kind::array, {1, 8}, 3, 2},
      root_case{"ObjectStrict", " {\"a\":[{}]} ", syntax_mode::strict_json, value_kind::object, {1, 10}, 3, 1}),
    [](const testing::TestParamInfo<root_case>& info)
    {
      return info.param.name;
    });
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief Describe rejected complete inputs and original error coordinates.
   * @par Requirements
   * P037; J5-026; J5-043; J5-044; J5-051.
   */
  struct rejection_case
  {
      const char* name;   // Input case identifier.
      std::string input;  // Input bytes, including malformed UTF-8 and embedded NUL.
      syntax_mode mode;   // Input syntax selection.
      error_code code;    // Expected explicit failure category.
      std::size_t offset; // Expected original byte coordinate.
  };
  class J5_026_CompleteRejection : public testing::TestWithParam<rejection_case>
  {
  };
  /*
   * Arrange: Missing roots, second roots, junk, malformed comments and invalid suffix encoding.
   * Act: Parse a complete input through the public entry.
   * Expect: A located error without a partial document or live temporary allocations.
   */
  TEST_P(J5_026_CompleteRejection, RejectsMissingRootsAndInvalidSuffixesAtomically)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    document_provider provider;
    parse_options options{fixture.mode};
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_document({fixture.input.data(), fixture.input.size()}, options, accounting);
    /* Expect */
    // Even a fully constructed root must be destroyed when its suffix invalidates the document.
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.value_if(), nullptr);
    ASSERT_NE(parsed.error_if(), nullptr);
    EXPECT_EQ(parsed.error_if()->code, fixture.code);
    EXPECT_TRUE(parsed.error_if()->has_byte_offset);
    EXPECT_EQ(parsed.error_if()->byte_offset, fixture.offset);
    EXPECT_EQ(accounting.current_bytes(), 0u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }
  INSTANTIATE_TEST_SUITE_P(
    P037, J5_026_CompleteRejection,
    testing::Values(
      rejection_case{"EmptyJson5", "", syntax_mode::json5, error_code::syntax, 0},
      rejection_case{"EmptyStrict", "", syntax_mode::strict_json, error_code::syntax, 0},
      rejection_case{"WhitespaceJson5", " \t\r\n", syntax_mode::json5, error_code::syntax, 4},
      rejection_case{"WhitespaceStrict", " \t\r\n", syntax_mode::strict_json, error_code::syntax, 4},
      rejection_case{"LineCommentOnly", "// only", syntax_mode::json5, error_code::syntax, 7},
      rejection_case{"BlockCommentOnly", "/*only*/", syntax_mode::json5, error_code::syntax, 8},
      rejection_case{"SecondObjectJson5", "{} {}", syntax_mode::json5, error_code::syntax, 3},
      rejection_case{"SecondObjectStrict", "{} {}", syntax_mode::strict_json, error_code::syntax, 3},
      rejection_case{"AdjacentRoots", "{}[]", syntax_mode::json5, error_code::syntax, 2},
      rejection_case{"SecondScalar", "true false", syntax_mode::json5, error_code::syntax, 5},
      rejection_case{"JunkJson5", "{} junk", syntax_mode::json5, error_code::syntax, 3},
      rejection_case{"JunkStrict", "{} junk", syntax_mode::strict_json, error_code::syntax, 3},
      rejection_case{"TrailingComma", "[],", syntax_mode::json5, error_code::syntax, 2},
      rejection_case{"TrailingCloser", "{}]", syntax_mode::json5, error_code::syntax, 2},
      rejection_case{"Semicolon", "{};", syntax_mode::json5, error_code::syntax, 2},
      rejection_case{"TrailingSlash", "{}/", syntax_mode::json5, error_code::syntax, 2},
      rejection_case{"UnterminatedComment", "{} /*", syntax_mode::json5, error_code::syntax, 5},
      rejection_case{"CommentBeforeSecondRoot", "{}/*ok*/[]", syntax_mode::json5, error_code::syntax, 8},
      rejection_case{"StrictLineComment", "{} //x", syntax_mode::strict_json, error_code::syntax, 3},
      rejection_case{"StrictBlockComment", "{} /*x*/", syntax_mode::strict_json, error_code::syntax, 3},
      rejection_case{"StrictLeadingComment", "/*x*/{}", syntax_mode::strict_json, error_code::syntax, 0},
      rejection_case{"StrictUnicodeSpace", "{}\xc2\xa0", syntax_mode::strict_json, error_code::syntax, 2},
      rejection_case{"InvalidSuffixEncoding", std::string("{} ") + char(0xff), syntax_mode::json5, error_code::encoding,
                     3},
      rejection_case{"TruncatedSuffixEncoding", "{}\xe2\x80", syntax_mode::json5, error_code::encoding, 2},
      rejection_case{"InvalidStrictEncoding", std::string("{} ") + char(0xff), syntax_mode::strict_json,
                     error_code::encoding, 3},
      rejection_case{"EmbeddedNul", std::string("{}\0", 3), syntax_mode::json5, error_code::syntax, 2},
      rejection_case{"NulHidesSecondRoot", std::string("{}\0[]", 5), syntax_mode::strict_json, error_code::syntax, 2}),
    [](const testing::TestParamInfo<rejection_case>& info)
    {
      return info.param.name;
    });
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Every proper prefix of a nested object whose root closes only at its final byte.
   * Act: Parse each explicit-length prefix in both syntax modes using one accounting object.
   * Expect: Every truncation fails without exposing a partial tree; the complete source succeeds.
   */
  TEST(J5_026_CompleteTruncation, RejectsEveryUnfinishedNestedPrefix)
  {
    /* Arrange */
    constexpr std::string_view input{"{\"a\":[true,\"x\",{}]}"};
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      options.syntax = mode;
      for (std::size_t size = 0; size < input.size(); ++size)
      {
        SCOPED_TRACE(size);
        const auto parsed = parse_document({input.data(), size}, options, accounting);
        /* Expect */
        // Explicit lengths must never let a scanner consume the unread closing bytes.
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
        EXPECT_EQ(parsed.value_if(), nullptr);
        EXPECT_EQ(accounting.current_bytes(), 0u);
      }
      const auto complete = parse_document({input.data(), input.size()}, options, accounting);
      ASSERT_TRUE(complete.has_value());
      EXPECT_EQ(complete.value_if()->values.size(), 5u);
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: A valid root followed by junk outside the caller's declared input length.
   * Act: Parse only the two root bytes in each supported syntax mode.
   * Expect: Success with the root's exact range; bytes beyond the view are irrelevant.
   */
  TEST(J5_026_CompleteLength, HonorsExplicitLengthWithoutTerminatingNul)
  {
    /* Arrange */
    constexpr std::array<char, 6> bytes{'{', '}', 'j', 'u', 'n', 'k'};
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      options.syntax = mode;
      const auto parsed = parse_document({bytes.data(), 2}, options, accounting);
      /* Expect */
      // Complete input means the supplied byte view, not the size of its backing array.
      ASSERT_TRUE(parsed.has_value());
      const auto root = parsed.value_if()->values.root();
      ASSERT_TRUE(root.has_value());
      const auto span = root.value_if()->source_span();
      ASSERT_TRUE(span.has_value());
      ASSERT_TRUE(span.value_if()->has_value());
      EXPECT_EQ((**span.value_if()).byte_length, 2u);
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  /* ================================= End Test_J5_026 ================================== */

  /* =================================== Test_J5_051 ==================================== */
  /*
   * Arrange: One shared accounting object and a successful document retained across later calls.
   * Act: Alternate syntax, work-limit and successful parses; inspect the earlier document again.
   * Expect: Failures restore prior outstanding bytes and never damage other live results.
   */
  TEST(J5_051_CompleteReuse, PreservesLiveDocumentsAcrossFailedAndSuccessfulCalls)
  {
    /* Arrange */
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const auto first = parse_document({"[true]", 6}, options, accounting);
    ASSERT_TRUE(first.has_value());
    const auto baseline = accounting.current_bytes();
    /* Act */
    for (const std::string_view invalid : {"{\"a\":[", "[1] junk", "{} {}"})
    {
      const auto failed = parse_document({invalid.data(), invalid.size()}, options, accounting);
      /* Expect */
      // Cleanup belongs to this call and must not release allocations owned by an earlier result.
      EXPECT_FALSE(failed.has_value());
      EXPECT_EQ(accounting.current_bytes(), baseline);
    }
    options.limits.max_work_units = 0;
    const auto limited = parse_document({"null", 4}, options, accounting);
    ASSERT_FALSE(limited.has_value());
    EXPECT_EQ(limited.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(accounting.current_bytes(), baseline);
    options.limits.max_work_units = 32768;
    const auto later = parse_document({"false", 5}, options, accounting);
    ASSERT_TRUE(later.has_value());
    const auto root = first.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto child = root.value_if()->element_at(0);
    ASSERT_TRUE(child.has_value());
    const auto boolean = child.value_if()->as_boolean();
    ASSERT_TRUE(boolean.has_value());
    EXPECT_TRUE(*boolean.value_if());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: A complete nested tree requiring names, strings, numbers, records and frame storage.
   * Act: Fail every observed allocation independently, then retry through the same accounting object.
   * Expect: Typed allocation failure, balanced cleanup and a complete successful retry.
   */
  TEST(J5_046_CompleteAllocation, FailsEveryAllocationAndAllowsRetry)
  {
    /* Arrange */
    constexpr std::string_view input{"{long_name:[\"owned text\",123,{nested:true}]}"};
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    {
      const auto measured = parse_document({input.data(), input.size()}, options, accounting);
      ASSERT_TRUE(measured.has_value());
    }
    const auto attempts = provider.m_attempts;
    ASSERT_GT(attempts, 0u);
    /* Act */
    for (std::size_t point = 1; point <= attempts; ++point)
    {
      SCOPED_TRACE(point);
      provider.m_fail_on = provider.m_attempts + point;
      const auto failed = parse_document({input.data(), input.size()}, options, accounting);
      /* Expect */
      // Each failure point must release partially owned names and descendants without publishing them.
      ASSERT_FALSE(failed.has_value());
      EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
      EXPECT_EQ(failed.value_if(), nullptr);
      EXPECT_EQ(accounting.current_bytes(), 0u);
      EXPECT_EQ(provider.m_live_bytes, 0u);
      provider.m_fail_on = 0;
      {
        const auto retry = parse_document({input.data(), input.size()}, options, accounting);
        ASSERT_TRUE(retry.has_value());
        EXPECT_EQ(retry.value_if()->values.size(), 6u);
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
  }
  /* ================================= End Test_J5_051 ================================== */

  /* =================================== Test_J5_047 ==================================== */
  /**
   * @brief Pair independent input examples with inclusive public resource boundaries.
   * @par Requirements
   * P037; J5-047; J5-049.
   */
  struct limit_case
  {
      const char* name;                    // Input case identifier.
      const char* input;                   // Input complete source.
      std::size_t resource_limits::*field; // Input limit selected for the operation.
      std::size_t exact;                   // Expected minimum successful allowance in that limit's units.
  };
  class J5_047_CompleteLimit : public testing::TestWithParam<limit_case>
  {
  };
  /*
   * Arrange: One resource limit at its exact required value and one unit below.
   * Act: Parse both configurations through the complete-document entry.
   * Expect: Inclusive success and recoverable resource failure, with no failed-call allocations retained.
   */
  TEST_P(J5_047_CompleteLimit, EnforcesInclusiveConfiguredBoundaries)
  {
    /* Arrange */
    const auto fixture = GetParam();
    const std::string_view input{fixture.input};
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    for (const std::size_t allowance : {fixture.exact - 1, fixture.exact})
    {
      options.limits.*(fixture.field) = allowance;
      const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
      /* Expect */
      // Public option forwarding must retain each resource's own units, including trailing input bytes.
      if (allowance == fixture.exact)
      {
        ASSERT_TRUE(parsed.has_value()) << fixture.name;
      }
      else
      {
        ASSERT_FALSE(parsed.has_value());
        EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
        EXPECT_EQ(parsed.value_if(), nullptr);
        EXPECT_EQ(accounting.current_bytes(), 0u);
      }
    }
    EXPECT_EQ(accounting.current_bytes(), 0u);
  }
  INSTANTIATE_TEST_SUITE_P(
    P037, J5_047_CompleteLimit,
    testing::Values(limit_case{"InputIncludesTrivia", "null  ", &resource_limits::max_input_bytes, 6},
                    limit_case{"Depth", "[[]]", &resource_limits::max_depth, 2},
                    limit_case{"Nodes", "[null,true]", &resource_limits::max_nodes, 3},
                    limit_case{"Members", "{a:1,b:2}", &resource_limits::max_members, 2},
                    limit_case{"String", "{a:\"xy\"}", &resource_limits::max_string_units, 2},
                    limit_case{"AggregateString", "{a:\"xy\"}", &resource_limits::max_total_string_units, 3},
                    limit_case{"AggregateAcrossNamesAndValues", "{ab:\"c\",d:[\"ef\",{g:\"h\"}]}",
                               &resource_limits::max_total_string_units, 8},
                    limit_case{"NumberBytes", "1.2e34", &resource_limits::max_number_bytes, 6},
                    limit_case{"ExponentDigits", "1e002", &resource_limits::max_exponent_digits, 3},
                    limit_case{"Diagnostics", "\"\xe2\x80\xa8\"", &resource_limits::max_diagnostics, 1},
                    limit_case{"WorkIncludesTrailingScan", "null  ", &resource_limits::max_work_units, 19}),
    [](const testing::TestParamInfo<limit_case>& info)
    {
      return info.param.name;
    });
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Null/length mismatches, a mismatched byte ceiling and an unavailable provider.
   * Act: Call the public parser without supplying a valid operation configuration.
   * Expect: Explicit access/allocation errors without an accidental provider call or partial result.
   */
  TEST(J5_047_CompleteConfiguration, RejectsInvalidViewsAccountingAndProviders)
  {
    /* Arrange */
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    accounted_storage mismatch{provider.callbacks(), options.limits.max_owned_bytes - 1};
    accounted_storage unavailable{{}, options.limits.max_owned_bytes};
    /* Act */
    const auto invalid = parse_document({nullptr, 1}, options, accounting);
    const auto wrong_limit = parse_document({"null", 4}, options, mismatch);
    const auto missing = parse_document({"null", 4}, options, unavailable);
    const auto empty = parse_document({}, options, accounting);
    /* Expect */
    // A null pointer with zero length is empty input; with nonzero length it is invalid access.
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(wrong_limit.has_value());
    EXPECT_EQ(wrong_limit.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error_if()->code, error_code::allocation_failure);
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error_if()->code, error_code::syntax);
    EXPECT_EQ(empty.error_if()->byte_offset, 0u);
    EXPECT_EQ(provider.m_attempts, 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: Measure platform-dependent requested storage for one complete nested document.
   * Act: Parse at the measured peak, one byte below, and with one extra caller-owned byte live.
   * Expect: An inclusive shared ceiling and successful reuse after releasing the caller's block.
   */
  TEST(J5_047_CompleteBytes, EnforcesPeakAndSharesOutstandingCallerAllocations)
  {
    /* Arrange */
    constexpr std::string_view input{"{a:[\"text\",123]}"};
    document_provider provider;
    parse_options options;
    std::size_t peak = 0;
    {
      accounted_storage measured{provider.callbacks(), options.limits.max_owned_bytes};
      const auto parsed = parse_document({input.data(), input.size()}, options, measured);
      ASSERT_TRUE(parsed.has_value());
      peak = measured.peak_bytes();
    }
    ASSERT_GT(peak, 0u);
    /* Act */
    for (const std::size_t limit : {peak - 1, peak})
    {
      options.limits.max_owned_bytes = limit;
      accounted_storage accounting{provider.callbacks(), limit};
      {
        const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
        /* Expect */
        // Requested byte sizes depend on the target layout, so measure them instead of assuming an ABI.
        EXPECT_EQ(parsed.has_value(), limit == peak);
        if (limit < peak)
        {
          ASSERT_NE(parsed.error_if(), nullptr);
          EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
          EXPECT_EQ(accounting.current_bytes(), 0u);
        }
      }
      EXPECT_EQ(accounting.current_bytes(), 0u);
    }
    accounted_storage shared{provider.callbacks(), peak};
    {
      const auto caller_block = shared.allocate(1, 1);
      ASSERT_TRUE(caller_block.has_value());
      const auto failed = parse_document({input.data(), input.size()}, options, shared);
      ASSERT_FALSE(failed.has_value());
      EXPECT_EQ(failed.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(shared.current_bytes(), 1u);
    }
    EXPECT_EQ(shared.current_bytes(), 0u);
    {
      const auto retry = parse_document({input.data(), input.size()}, options, shared);
      EXPECT_TRUE(retry.has_value());
    }
    EXPECT_EQ(shared.current_bytes(), 0u);
  }
  /* ================================= End Test_J5_047 ================================== */

  /* =================================== Test_J5_040 ==================================== */
  /*
   * Arrange: Mutable input with escaped name/value, embedded NUL and exact numeric spelling.
   * Act: Parse, overwrite the source, and move the public result while storage accounting stays alive.
   * Expect: Names, UTF-16 units, number spelling and original spans remain readable.
   */
  TEST(J5_040_CompleteOwnership, RetainsDataAfterInputOverwriteAndResultMove)
  {
    /* Arrange */
    std::string input = "{\"n\\u0061me\":\"x\\u0000y\",number:+0x2A}";
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    auto parsed = parse_document({input.data(), input.size()}, options, accounting);
    ASSERT_TRUE(parsed.has_value());
    std::fill(input.begin(), input.end(), '?');
    auto moved = std::move(parsed);
    /* Expect */
    // Neither decoding nor numeric spelling may borrow the caller's overwritten source buffer.
    const auto root = moved.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto first = root.value_if()->member_at(0);
    const auto second = root.value_if()->member_at(1);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    const auto name = first.value_if()->member_name();
    ASSERT_TRUE(name.has_value());
    ASSERT_EQ((*name.value_if())->size(), 4u);
    for (std::size_t index = 0; index < 4; ++index)
    {
      const auto unit = (*name.value_if())->unit_at(index);
      ASSERT_TRUE(unit.has_value());
      EXPECT_EQ(*unit.value_if(), u"name"[index]);
    }
    const auto text = first.value_if()->as_string();
    ASSERT_TRUE(text.has_value());
    ASSERT_EQ((*text.value_if())->size(), 3u);
    const std::array<char16_t, 3> expected{u'x', 0, u'y'};
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      const auto unit = (*text.value_if())->unit_at(index);
      ASSERT_TRUE(unit.has_value());
      EXPECT_EQ(*unit.value_if(), expected[index]);
    }
    const auto number = second.value_if()->as_number();
    ASSERT_TRUE(number.has_value());
    const auto spelling = (*number.value_if())->spelling();
    EXPECT_EQ(std::string_view(spelling.data, spelling.size), "+0x2A");
    const auto name_span = first.value_if()->member_name_source_span();
    ASSERT_TRUE(name_span.has_value());
    ASSERT_TRUE(name_span.value_if()->has_value());
    EXPECT_EQ((**name_span.value_if()).byte_offset, 1u);
    EXPECT_EQ((**name_span.value_if()).byte_length, 11u);
  }
  /* ================================= End Test_J5_040 ================================== */

  /* =================================== Test_J5_032 ==================================== */
  /*
   * Arrange: Two spellings of the same decoded name and explicit duplicate rejection.
   * Act: Parse with default preservation and rejection through the public wrapper.
   * Expect: Ordered distinct values by default; rejection preserves both original token spans.
   */
  TEST(J5_032_CompletePolicy, PreservesDefaultAndForwardsDuplicateDiagnostics)
  {
    /* Arrange */
    constexpr std::string_view input{"{a:true,\"\\u0061\":false}"};
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto preserved = parse_document({input.data(), input.size()}, options, accounting);
    options.duplicate_names = duplicate_name_policy::reject_duplicates;
    const auto rejected = parse_document({input.data(), input.size()}, options, accounting);
    /* Expect */
    // Returning the engine error intact is necessary to retain its second diagnostic location.
    ASSERT_TRUE(preserved.has_value());
    const auto root = preserved.value_if()->values.root();
    ASSERT_TRUE(root.has_value());
    const auto count = root.value_if()->container_size();
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(*count.value_if(), 2u);
    for (std::size_t index = 0; index < 2; ++index)
    {
      const auto member = root.value_if()->member_at(index);
      ASSERT_TRUE(member.has_value());
      const auto boolean = member.value_if()->as_boolean();
      ASSERT_TRUE(boolean.has_value());
      EXPECT_EQ(*boolean.value_if(), index == 0);
    }
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.value_if(), nullptr);
    EXPECT_EQ(rejected.error_if()->code, error_code::duplicate_policy);
    EXPECT_EQ(rejected.error_if()->byte_offset, 8u);
    ASSERT_TRUE(rejected.error_if()->duplicate_names.has_value());
    EXPECT_EQ(rejected.error_if()->duplicate_names->first.byte_offset, 1u);
    EXPECT_EQ(rejected.error_if()->duplicate_names->first.byte_length, 1u);
    EXPECT_EQ(rejected.error_if()->duplicate_names->repeated.byte_offset, 8u);
    EXPECT_EQ(rejected.error_if()->duplicate_names->repeated.byte_length, 8u);
  }
  /* ================================= End Test_J5_032 ================================== */

  /* =================================== Test_J5_017 ==================================== */
  /*
   * Arrange: A member name and string containing both advisory raw line-separator forms.
   * Act: Parse a complete document with trailing trivia through the public API.
   * Expect: Both warnings are retained in source order with their original byte locations.
   */
  TEST(J5_017_CompleteWarnings, RetainsEveryWarningInSourceOrder)
  {
    /* Arrange */
    constexpr std::string_view input{"{\"\xe2\x80\xa8\":\"\xe2\x80\xa9\"} //end"};
    document_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    const auto parsed = parse_document({input.data(), input.size()}, options, accounting);
    /* Expect */
    // The public result must carry metadata from names as well as values across engine destruction.
    ASSERT_TRUE(parsed.has_value());
    const auto& warnings = parsed.value_if()->warnings;
    ASSERT_EQ(warnings.size(), 2u);
    const auto first = warnings.at(0);
    const auto second = warnings.at(1);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first.value_if()->code, string_warning_code::raw_line_separator);
    EXPECT_EQ(first.value_if()->byte_offset, 2u);
    EXPECT_EQ(first.value_if()->byte_length, 3u);
    EXPECT_EQ(second.value_if()->code, string_warning_code::raw_paragraph_separator);
    EXPECT_EQ(second.value_if()->byte_offset, 8u);
    EXPECT_EQ(second.value_if()->byte_length, 3u);
  }
  /* ================================= End Test_J5_017 ================================== */
} // namespace
