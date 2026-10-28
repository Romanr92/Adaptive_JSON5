#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/hosted/parse.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;
  /* ==================================== Fixtures ===================================== */

  static_assert(noexcept(parse(std::declval<input_view>())));
  static_assert(noexcept(parse(std::declval<input_view>(), std::declval<const parse_options&>())));
  static_assert(std::is_nothrow_move_constructible_v<hosted_document>);
  static_assert(!std::is_copy_constructible_v<hosted_document>);
  static_assert(!std::is_copy_assignable_v<hosted_document>);
  static_assert(!std::is_move_assignable_v<hosted_document>);

  /**
   * @brief Parse text through the explicit-storage API with the same options.
   * @par Requirements
   * P037; J5-026.
   * @param text Complete input.
   * @param options Parse options shared with the hosted call.
   * @return accounting.current_bytes() while the document was alive, or the explicit error.
   */
  result<std::size_t> parse_explicitly(const std::string& text, const parse_options& options)
  {
    document_provider provider;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    const auto parsed = parse_document({text.data(), text.size()}, options, accounting);

    if (!parsed.has_value())
    {
      return result<std::size_t>::failure(*parsed.error_if());
    }

    return result<std::size_t>::success(accounting.current_bytes());
  }

  /**
   * @brief Describe one input that the hosted and explicit APIs must reject identically.
   * @par Requirements
   * P037; J5-043; J5-044.
   */
  struct failure_case
  {
      const char* name;
      const char* input;     // Complete input.
      syntax_mode mode;      // Selected syntax.
      std::size_t max_depth; // Configured nesting limit.
  };

  std::ostream& operator<<(std::ostream& stream, const failure_case& value)
  {
    return stream << value.name;
  }

  class HostedParseFailureUnits : public ::testing::TestWithParam<failure_case>
  {
  };
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_026 =================================== */

  /* Verify the one-call parse succeeds with default options and owns its storage.
   * Arrange: A JSON5 document with an identifier name, a single-quoted string and one raw U+2028.
   * Act: Parse it with parse(input) only.
   * Expect: Four values, one retained warning, owned bytes equal to the explicit API's, and a
   *         peak at least as high as the current bytes.
   */
  TEST(J5_026_HostedParse, ParsesWithDefaultOptionsAndOwnsStorage)
  {
    /* Arrange */
    const std::string text = "{a: [1, 'x\xE2\x80\xA8']}";
    const auto explicit_bytes = parse_explicitly(text, parse_options{});
    ASSERT_TRUE(explicit_bytes.has_value());
    /* Act */
    const auto parsed = parse({text.data(), text.size()});
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const hosted_document& document = *parsed.value_if();
    EXPECT_EQ(document.values().size(), 4U);
    EXPECT_EQ(document.warnings().size(), 1U);
    EXPECT_EQ(document.current_bytes(), *explicit_bytes.value_if());
    EXPECT_GE(document.peak_bytes(), document.current_bytes());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify explicit options are applied exactly as by parse_document.
   * Arrange: An identifier member name, valid in JSON5 only.
   * Act: Parse it with default options and with strict_json.
   * Expect: Success by default; in strict mode the same syntax error as parse_document.
   */
  TEST(J5_026_HostedParse, AppliesExplicitOptions)
  {
    /* Arrange */
    const std::string text = "{a: 1}";
    parse_options strict{};
    strict.syntax = syntax_mode::strict_json;
    const auto explicit_strict = parse_explicitly(text, strict);
    ASSERT_FALSE(explicit_strict.has_value());
    /* Act */
    const auto lenient = parse({text.data(), text.size()});
    const auto rejected = parse({text.data(), text.size()}, strict);
    /* Expect */
    EXPECT_TRUE(lenient.has_value());
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->code, explicit_strict.error_if()->code);
    EXPECT_EQ(rejected.error_if()->byte_offset, explicit_strict.error_if()->byte_offset);
    EXPECT_EQ(rejected.error_if()->syntax_reason, explicit_strict.error_if()->syntax_reason);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify failures are reported exactly as by the explicit-storage API.
   * Arrange: The case input, syntax and nesting limit.
   * Act: Parse through parse() and through parse_document().
   * Expect: The same error code, location presence, offset and syntax reason.
   */
  TEST_P(HostedParseFailureUnits, MatchesParseDocumentErrors)
  {
    /* Arrange */
    const failure_case& param = GetParam();
    const std::string text = param.input;
    parse_options options{};
    options.syntax = param.mode;
    options.limits.max_depth = param.max_depth;
    const auto expected = parse_explicitly(text, options);
    ASSERT_FALSE(expected.has_value());
    /* Act */
    const auto parsed = parse({text.data(), text.size()}, options);
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    const error& failure = *parsed.error_if();
    EXPECT_EQ(failure.code, expected.error_if()->code);
    EXPECT_EQ(failure.has_byte_offset, expected.error_if()->has_byte_offset);
    EXPECT_EQ(failure.byte_offset, expected.error_if()->byte_offset);
    EXPECT_EQ(failure.syntax_reason, expected.error_if()->syntax_reason);
  }

  INSTANTIATE_TEST_SUITE_P(J5_026_HostedParse, HostedParseFailureUnits,
                           ::testing::Values(failure_case{"EmptyInput", "", syntax_mode::json5, 512},
                                             failure_case{"Unterminated", "[1, ", syntax_mode::json5, 512},
                                             failure_case{"TrailingContent", "1 x", syntax_mode::json5, 512},
                                             failure_case{"DepthLimit", "[[1]]", syntax_mode::json5, 1},
                                             failure_case{"StrictTrailingComma", "[1,]", syntax_mode::strict_json,
                                                          512}),
                           [](const ::testing::TestParamInfo<failure_case>& info)
                           {
                             return info.param.name;
                           });
  /* ================================= End Test_J5_026 ================================= */
  /* =================================== Test_J5_040 =================================== */

  /* Verify moving keeps the document usable and empties the source.
   * Arrange: A parsed array of three numbers.
   * Act: Move the hosted document into a new object.
   * Expect: The destination keeps its values and byte counts; the source reports zero bytes.
   */
  TEST(J5_040_HostedParse, MoveKeepsDocumentAndStorage)
  {
    /* Arrange */
    const std::string text = "[1, 2, 3]";
    auto parsed = parse({text.data(), text.size()});
    ASSERT_TRUE(parsed.has_value());
    hosted_document& source = *parsed.value_if();
    const std::size_t bytes = source.current_bytes();
    const std::size_t peak = source.peak_bytes();
    /* Act */
    const hosted_document destination{std::move(source)};
    /* Expect */
    EXPECT_EQ(destination.values().size(), 4U);
    EXPECT_EQ(destination.current_bytes(), bytes);
    EXPECT_EQ(destination.peak_bytes(), peak);
    EXPECT_EQ(source.current_bytes(), 0U);
    EXPECT_EQ(source.peak_bytes(), 0U);
    EXPECT_EQ(source.values().size(), 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify each call owns independent storage.
   * Arrange: Two different documents.
   * Act: Parse both and keep both alive.
   * Expect: Each reports the bytes the explicit API reports for its own input.
   */
  TEST(J5_040_HostedParse, EachCallOwnsIndependentStorage)
  {
    /* Arrange */
    const std::string small = "[1]";
    const std::string large = R"({"name": "a longer string value", "items": [1, 2, 3, 4]})";
    const auto small_bytes = parse_explicitly(small, parse_options{});
    const auto large_bytes = parse_explicitly(large, parse_options{});
    ASSERT_TRUE(small_bytes.has_value());
    ASSERT_TRUE(large_bytes.has_value());
    /* Act */
    const auto first = parse({small.data(), small.size()});
    const auto second = parse({large.data(), large.size()});
    /* Expect */
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first.value_if()->current_bytes(), *small_bytes.value_if());
    EXPECT_EQ(second.value_if()->current_bytes(), *large_bytes.value_if());
  }
  /* ================================= End Test_J5_040 ================================= */
} // namespace
