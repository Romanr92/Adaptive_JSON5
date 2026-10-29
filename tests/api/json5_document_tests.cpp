#include "../shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/diagnostic_format.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/value/string_warning.hpp"

#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <ostream>
#include <string>
#include <string_view>
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
  /* =================================== Test_J5_017 =================================== */

  /* Verify the document exposes every retained warning in source order.
   * Arrange: Two strings holding a raw U+2028 and a raw U+2029, and one without warnings.
   * Act: Parse both and read the warnings by index.
   * Expect: Two warnings with their codes and original spans, wrong_type_or_access past the
   *         last one, and no warnings for the plain document.
   */
  TEST(J5_017_Json5Document, ExposesWarningsInSourceOrder)
  {
    /* Arrange */
    const std::string text{"['\xE2\x80\xA8', '\xE2\x80\xA9']"};
    const std::string plain{"['a', 'b']"};
    caller_buffer memory{};
    caller_buffer plain_memory{};
    /* Act */
    const auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
    const auto parsed_plain =
      json5_document::parse({plain.data(), plain.size()}, plain_memory.bytes, sizeof(plain_memory.bytes));
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    const json5_document& document = *parsed.value_if();
    ASSERT_EQ(document.warning_count(), 2U);
    const auto first = document.warning(0U);
    const auto second = document.warning(1U);
    const auto past_end = document.warning(2U);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first.value_if()->code, string_warning_code::raw_line_separator);
    EXPECT_EQ(first.value_if()->byte_offset, 2U);
    EXPECT_EQ(first.value_if()->byte_length, 3U);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second.value_if()->code, string_warning_code::raw_paragraph_separator);
    EXPECT_EQ(second.value_if()->byte_offset, 9U);
    EXPECT_EQ(second.value_if()->byte_length, 3U);
    ASSERT_FALSE(past_end.has_value());
    EXPECT_EQ(past_end.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(parsed_plain.has_value());
    EXPECT_EQ(parsed_plain.value_if()->warning_count(), 0U);
    EXPECT_FALSE(parsed_plain.value_if()->warning(0U).has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify moving transfers the warnings and leaves the source without any.
   * Arrange: A parsed string holding a raw U+2028.
   * Act: Move the document into a new object.
   * Expect: The destination has the warning; the source reports none and rejects index 0.
   */
  TEST(J5_017_Json5Document, MoveTransfersWarnings)
  {
    /* Arrange */
    const std::string text{"'\xE2\x80\xA8'"};
    caller_buffer memory{};
    auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
    ASSERT_TRUE(parsed.has_value());
    json5_document& source = *parsed.value_if();
    /* Act */
    const json5_document destination{std::move(source)};
    /* Expect */
    EXPECT_EQ(destination.warning_count(), 1U);
    EXPECT_TRUE(destination.warning(0U).has_value());
    EXPECT_EQ(source.warning_count(), 0U);
    const auto moved_from = source.warning(0U);
    ASSERT_FALSE(moved_from.has_value());
    EXPECT_EQ(moved_from.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_017 ================================= */
  /* =================================== Test_J5_044 =================================== */

  /* Verify describe writes the same text as the formatters and views it in the buffer.
   * Arrange: A failed parse, an unlocated error and a document with one warning.
   * Act: Describe each into a character array and format each into a second buffer.
   * Expect: Each view starts at its array and equals the formatter's text.
   */
  TEST(J5_044_Json5Document, DescribesErrorsAndWarningsLikeTheFormatters)
  {
    /* Arrange */
    const std::string broken{"[1, "};
    const std::string warned{"['\xE2\x80\xA8']"};
    const input_view broken_input{broken.data(), broken.size()};
    const input_view warned_input{warned.data(), warned.size()};
    caller_buffer memory{};
    caller_buffer warned_memory{};
    const auto failed = json5_document::parse(broken_input, memory.bytes, sizeof(memory.bytes));
    ASSERT_FALSE(failed.has_value());
    const error located = *failed.error_if();
    const error unlocated = error::without_location(error_code::allocation_failure);
    const auto parsed = json5_document::parse(warned_input, warned_memory.bytes, sizeof(warned_memory.bytes));
    ASSERT_TRUE(parsed.has_value());
    const auto warning = parsed.value_if()->warning(0U);
    ASSERT_TRUE(warning.has_value());
    char located_text[128]{};
    char unlocated_text[128]{};
    char warning_text[128]{};
    char expected[128]{};
    /* Act */
    const auto located_view = describe(located, broken_input, located_text);
    const auto unlocated_view = describe(unlocated, broken_input, unlocated_text);
    const auto warning_view = describe(*warning.value_if(), warned_input, warning_text);
    /* Expect */
    ASSERT_TRUE(located_view.has_value());
    ASSERT_TRUE(format_error(located, broken_input, expected, sizeof(expected)).has_value());
    EXPECT_EQ(located_view.value_if()->data(), located_text);
    EXPECT_EQ(*located_view.value_if(), std::string_view{expected});
    ASSERT_TRUE(unlocated_view.has_value());
    ASSERT_TRUE(format_error(unlocated, broken_input, expected, sizeof(expected)).has_value());
    EXPECT_EQ(unlocated_view.value_if()->data(), unlocated_text);
    EXPECT_EQ(*unlocated_view.value_if(), std::string_view{expected});
    ASSERT_TRUE(warning_view.has_value());
    ASSERT_TRUE(format_warning(*warning.value_if(), warned_input, expected, sizeof(expected)).has_value());
    EXPECT_EQ(warning_view.value_if()->data(), warning_text);
    EXPECT_EQ(*warning_view.value_if(), std::string_view{expected});
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a buffer too small for the text yields the truncated, NUL-terminated part.
   * Arrange: A located error and a warning whose full texts exceed 8 bytes.
   * Act: Describe each into 8 bytes, and the error into zero bytes without a buffer.
   * Expect: Views of the first 7 bytes followed by a NUL; an empty view for zero bytes.
   */
  TEST(J5_044_Json5Document, TruncatesToTheBuffer)
  {
    /* Arrange */
    const std::string text{"'\xE2\x80\xA8'"};
    const input_view input{text.data(), text.size()};
    const error located = error::at_offset(error_code::syntax, 1U);
    const string_warning warning{string_warning_code::raw_line_separator, 1U, 3U};
    char full[128]{};
    char error_text[8];
    char warning_text[8];
    /* Act */
    const auto error_view = describe(located, input, error_text);
    const auto warning_view = describe(warning, input, warning_text);
    const auto empty_view = describe(located, input, nullptr, 0U);
    /* Expect */
    ASSERT_TRUE(error_view.has_value());
    ASSERT_TRUE(format_error(located, input, full, sizeof(full)).has_value());
    EXPECT_EQ(*error_view.value_if(), std::string_view{full}.substr(0, 7));
    EXPECT_EQ(error_text[7], '\0');
    ASSERT_TRUE(warning_view.has_value());
    ASSERT_TRUE(format_warning(warning, input, full, sizeof(full)).has_value());
    EXPECT_EQ(*warning_view.value_if(), std::string_view{full}.substr(0, 7));
    EXPECT_EQ(warning_text[7], '\0');
    ASSERT_TRUE(empty_view.has_value());
    EXPECT_TRUE(empty_view.value_if()->empty());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify describe forwards the formatters' argument errors.
   * Arrange: A null buffer with nonzero capacity, and an error and a warning beyond the input.
   * Act: Describe each.
   * Expect: wrong_type_or_access every time.
   */
  TEST(J5_044_Json5Document, RejectsInvalidBufferOrLocation)
  {
    /* Arrange */
    const std::string text{"[1]"};
    const input_view input{text.data(), text.size()};
    const error inside = error::at_offset(error_code::syntax, 1U);
    const error beyond = error::at_offset(error_code::syntax, 100U);
    const string_warning warning_beyond{string_warning_code::raw_line_separator, 100U, 3U};
    char buffer[64]{};
    /* Act */
    const auto null_buffer = describe(inside, input, nullptr, 8U);
    const auto error_beyond = describe(beyond, input, buffer);
    const auto warning_view = describe(warning_beyond, input, buffer);
    /* Expect */
    for (const auto* failed : {&null_buffer, &error_beyond, &warning_view})
    {
      ASSERT_FALSE(failed->has_value());
      EXPECT_EQ(failed->error_if()->code, error_code::wrong_type_or_access);
    }
  }
  /* ================================= End Test_J5_044 ================================= */
} // namespace
