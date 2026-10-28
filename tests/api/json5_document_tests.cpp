#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"

#include <cstddef>
#include <cstring>
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

  static_assert(noexcept(json5_document::parse(std::declval<input_view>(), std::declval<void*>(),
                                               std::declval<std::size_t>())));
  static_assert(std::is_nothrow_move_constructible_v<json5_document>);
  static_assert(!std::is_copy_constructible_v<json5_document>);
  static_assert(!std::is_copy_assignable_v<json5_document>);
  static_assert(!std::is_move_assignable_v<json5_document>);

  constexpr std::size_t c_buffer_size = 16384;

  /**
   * @brief Caller storage suitably aligned for the control block.
   * @par Requirements
   * J5-046; J5-101.
   */
  struct alignas(std::max_align_t) caller_buffer
  {
      unsigned char bytes[c_buffer_size];
  };

  /**
   * @brief Parse text through the explicit-storage API with the same options.
   * @par Requirements
   * P037; J5-026.
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
   * @brief Describe one input that the buffer and explicit APIs must reject identically.
   * @par Requirements
   * P037; J5-043; J5-044.
   */
  struct failure_case
  {
      const char* name;
      const char* input; // Complete input.
      syntax_mode mode;  // Selected syntax.
  };

  std::ostream& operator<<(std::ostream& stream, const failure_case& value)
  {
    return stream << value.name;
  }

  class Json5DocumentFailureUnits : public ::testing::TestWithParam<failure_case>
  {
  };
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_101 =================================== */

  /* Verify a document parses into a caller buffer and owns its storage.
   * Arrange: A JSON5 object with an array and a caller buffer.
   * Act: Parse with default options.
   * Expect: Four values and owned bytes equal to the explicit API's, with a peak at least
   *         as high as the current bytes.
   */
  TEST(J5_101_Json5Document, ParsesIntoCallerBuffer)
  {
    /* Arrange */
    const std::string text = "{a: [1, 2]}";
    const auto explicit_bytes = parse_explicitly(text, parse_options{});
    ASSERT_TRUE(explicit_bytes.has_value());
    caller_buffer memory{};
    /* Act */
    const auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed.value_if()->values().size(), 4U);
    EXPECT_EQ(parsed.value_if()->current_bytes(), *explicit_bytes.value_if());
    EXPECT_GE(parsed.value_if()->peak_bytes(), parsed.value_if()->current_bytes());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify missing or too small buffers fail with allocation_failure.
   * Arrange: A null buffer, a buffer too small for the control block, and a buffer that
   *          fits the control block but not a 2000-character string.
   * Act: Parse into each.
   * Expect: allocation_failure each time. Failures before parsing starts have no location;
   *         the string that does not fit is reported at its opening quote, as by parse_document.
   */
  TEST(J5_101_Json5Document, ReportsBuffersTooSmall)
  {
    /* Arrange */
    const std::string small = "[1]";
    const std::string large = "\"" + std::string(2000, 'a') + "\"";
    caller_buffer memory{};
    /* Act */
    const auto null_buffer = json5_document::parse({small.data(), small.size()}, nullptr, c_buffer_size);
    const auto no_control_block = json5_document::parse({small.data(), small.size()}, memory.bytes, 8U);
    const auto no_room_for_values = json5_document::parse({large.data(), large.size()}, memory.bytes, 1024U);
    /* Expect */
    for (const auto* failed : {&null_buffer, &no_control_block})
    {
      ASSERT_FALSE(failed->has_value());
      EXPECT_EQ(failed->error_if()->code, error_code::allocation_failure);
      EXPECT_FALSE(failed->error_if()->has_byte_offset);
    }
    ASSERT_FALSE(no_room_for_values.has_value());
    EXPECT_EQ(no_room_for_values.error_if()->code, error_code::allocation_failure);
    EXPECT_TRUE(no_room_for_values.error_if()->has_byte_offset);
    EXPECT_EQ(no_room_for_values.error_if()->byte_offset, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the configured byte ceiling still applies inside a large buffer.
   * Arrange: max_owned_bytes of 16 and a buffer far larger than that.
   * Act: Parse a document that needs more than 16 bytes.
   * Expect: resource_limit, as parse_document reports it.
   */
  TEST(J5_101_Json5Document, EnforcesMaxOwnedBytes)
  {
    /* Arrange */
    const std::string text = R"({"name": "a string longer than sixteen bytes"})";
    parse_options options{};
    options.limits.max_owned_bytes = 16;
    const auto expected = parse_explicitly(text, options);
    ASSERT_FALSE(expected.has_value());
    caller_buffer memory{};
    /* Act */
    const auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes), options);
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(parsed.error_if()->code, expected.error_if()->code);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify parse errors are reported exactly as by the explicit-storage API.
   * Arrange: The case input and syntax, and a caller buffer.
   * Act: Parse through json5_document::parse and parse_document.
   * Expect: The same code, location presence, offset and syntax reason.
   */
  TEST_P(Json5DocumentFailureUnits, MatchesParseDocumentErrors)
  {
    /* Arrange */
    const failure_case& param = GetParam();
    const std::string text = param.input;
    parse_options options{};
    options.syntax = param.mode;
    const auto expected = parse_explicitly(text, options);
    ASSERT_FALSE(expected.has_value());
    caller_buffer memory{};
    /* Act */
    const auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes), options);
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    const error& failure = *parsed.error_if();
    EXPECT_EQ(failure.code, expected.error_if()->code);
    EXPECT_EQ(failure.has_byte_offset, expected.error_if()->has_byte_offset);
    EXPECT_EQ(failure.byte_offset, expected.error_if()->byte_offset);
    EXPECT_EQ(failure.syntax_reason, expected.error_if()->syntax_reason);
  }

  INSTANTIATE_TEST_SUITE_P(J5_101_Json5Document, Json5DocumentFailureUnits,
                           ::testing::Values(failure_case{"EmptyInput", "", syntax_mode::json5},
                                             failure_case{"Unterminated", "[1, ", syntax_mode::json5},
                                             failure_case{"TrailingContent", "1 x", syntax_mode::json5},
                                             failure_case{"StrictIdentifierName", "{a: 1}", syntax_mode::strict_json}),
                           [](const ::testing::TestParamInfo<failure_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify input and storage buffer must not share bytes.
   * Arrange: Input text copied into a caller buffer, so the buffer starts at the input,
   *          a buffer starting inside the input, and a buffer directly after the input.
   * Act: Parse with each buffer.
   * Expect: wrong_type_or_access for both overlaps; the adjacent buffer parses.
   */
  TEST(J5_101_Json5Document, RejectsOverlappingInputAndBuffer)
  {
    /* Arrange */
    const char text[] = "[1, 2, 3]";
    const std::size_t length = sizeof(text) - 1U;
    caller_buffer memory{};
    std::memcpy(memory.bytes, text, length);
    const input_view input{reinterpret_cast<const char*>(memory.bytes), length};
    /* Act */
    const auto at_start = json5_document::parse(input, memory.bytes, sizeof(memory.bytes));
    const auto inside = json5_document::parse(input, memory.bytes + 4, sizeof(memory.bytes) - 4U);
    // A buffer starting right after the input's last byte shares no byte with it.
    const std::size_t after = 64;
    std::memcpy(memory.bytes + after - length, text, length);
    const auto adjacent = json5_document::parse({reinterpret_cast<const char*>(memory.bytes + after - length), length},
                                                memory.bytes + after, sizeof(memory.bytes) - after);
    /* Expect */
    ASSERT_FALSE(at_start.has_value());
    EXPECT_EQ(at_start.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(inside.has_value());
    EXPECT_EQ(inside.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(adjacent.has_value());
    EXPECT_EQ(adjacent.value_if()->values().size(), 4U);
  }
  /* ================================= End Test_J5_101 ================================= */
  /* =================================== Test_J5_040 =================================== */

  /* Verify moving keeps the document usable and empties the source.
   * Arrange: A parsed array of three numbers in a caller buffer.
   * Act: Move the document into a new object.
   * Expect: The destination keeps its values and byte counts; the source reports zero.
   */
  TEST(J5_040_Json5Document, MoveKeepsDocumentAndStorage)
  {
    /* Arrange */
    const std::string text = "[1, 2, 3]";
    caller_buffer memory{};
    auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
    ASSERT_TRUE(parsed.has_value());
    json5_document& source = *parsed.value_if();
    const std::size_t bytes = source.current_bytes();
    const std::size_t peak = source.peak_bytes();
    /* Act */
    const json5_document destination{std::move(source)};
    /* Expect */
    EXPECT_EQ(destination.values().size(), 4U);
    EXPECT_EQ(destination.current_bytes(), bytes);
    EXPECT_EQ(destination.peak_bytes(), peak);
    EXPECT_EQ(source.current_bytes(), 0U);
    EXPECT_EQ(source.peak_bytes(), 0U);
    EXPECT_EQ(source.values().size(), 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the buffer can be reused after a document is destroyed or a parse fails.
   * Arrange: One caller buffer.
   * Act: Parse and destroy a document, fail a parse, then parse again in the same buffer.
   * Expect: Every successful parse reports the same bytes as the first one.
   */
  TEST(J5_040_Json5Document, BufferIsReusableAfterDestructionAndFailure)
  {
    /* Arrange */
    const std::string text = R"({"items": [1, 2, 3], "name": "reuse"})";
    const std::string broken = "[1, ";
    caller_buffer memory{};
    std::size_t first_bytes{};
    /* Act */
    {
      const auto first = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
      ASSERT_TRUE(first.has_value());
      first_bytes = first.value_if()->current_bytes();
    }
    const auto failed = json5_document::parse({broken.data(), broken.size()}, memory.bytes, sizeof(memory.bytes));
    const auto again = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
    /* Expect */
    EXPECT_FALSE(failed.has_value());
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(again.value_if()->current_bytes(), first_bytes);
    EXPECT_EQ(again.value_if()->values().size(), 6U);
  }
  /* ================================= End Test_J5_040 ================================= */
} // namespace
