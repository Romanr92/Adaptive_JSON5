// Only the umbrella header is included from the library: this file proves it is enough.
#include "adaptive_json5/adaptive_json5.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */

  // The simple API and representative advanced components are all declared.
  static_assert(std::is_class_v<json5_document>);
  static_assert(std::is_class_v<json5_value>);
  static_assert(std::is_class_v<buffer_arena>);
  static_assert(std::is_class_v<accounted_storage>);
  static_assert(std::is_class_v<document>);
  static_assert(std::is_class_v<warning_list>);
  static_assert(std::is_class_v<parse_options>);
  static_assert(std::is_enum_v<value_kind>);
  static_assert(std::is_enum_v<syntax_error_reason>);

  constexpr std::size_t c_buffer_size = 8192;

  /**
   * @brief Caller storage suitably aligned for the control block.
   * @par Requirements
   * J5-101.
   */
  struct alignas(std::max_align_t) caller_buffer
  {
      unsigned char bytes[c_buffer_size];
  };
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_092 =================================== */

  /* Verify the common case works with the umbrella header alone.
   * Arrange: A JSON5 object with a number and a string, and a caller buffer.
   * Act: Parse it and read both members through root().
   * Expect: The number and the string, and no warnings.
   */
  TEST(J5_092_UmbrellaHeader, ParsesAndReadsWithOneInclude)
  {
    /* Arrange */
    const std::string text = "{port: 8080, host: 'localhost'}";
    caller_buffer memory{};
    char host[16]{};
    /* Act */
    const auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
    ASSERT_TRUE(parsed.has_value());
    const json5_value root = parsed.value_if()->root();
    const auto port = root.member("port").as_int64();
    const auto name = root.member("host").as_string(host, sizeof(host));
    /* Expect */
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*port.value_if(), 8080);
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name.value_if(), "localhost");
    EXPECT_EQ(parsed.value_if()->warning_count(), 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a failed parse can be described with the umbrella header alone.
   * Arrange: An unterminated array and a caller buffer.
   * Act: Parse it and describe the error.
   * Expect: A syntax error whose text starts with its line and column.
   */
  TEST(J5_092_UmbrellaHeader, DescribesFailureWithOneInclude)
  {
    /* Arrange */
    const std::string text = "[1, ";
    const input_view input{text.data(), text.size()};
    caller_buffer memory{};
    char message[128]{};
    /* Act */
    const auto parsed = json5_document::parse(input, memory.bytes, sizeof(memory.bytes));
    ASSERT_FALSE(parsed.has_value());
    const auto described = describe(*parsed.error_if(), input, message);
    /* Expect */
    EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
    ASSERT_TRUE(described.has_value());
    EXPECT_EQ(described.value_if()->substr(0, 6), "line: ");
  }
  /* ================================= End Test_J5_092 ================================= */
} // namespace
