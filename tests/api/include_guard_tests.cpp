// A consumer may define common guard names of its own. Define every unprefixed guard the public headers
// used before #126; the umbrella header must still declare the whole API.
#define HOSTED_PARSE_HPP_
#define INPUT_HPP_
#define JSON5_DOCUMENT_HPP_
#define JSON5_ITERATOR_HPP_
#define JSON5_VALUE_HPP_
#define ACCOUNTED_STORAGE_HPP_
#define BUFFER_ARENA_HPP_
#define DOCUMENT_STORAGE_OWNER_HPP_
#define OWNED_BLOCK_HPP_
#define RESOURCE_LIMITS_HPP_
#define STORAGE_HPP_
#define OPTIONS_HPP_
#define PARSER_HPP_
#define DECODED_NAME_EQUAL_HPP_
#define DECODED_STRING_HPP_
#define DOCUMENT_HPP_
#define MEMBER_LOOKUP_HPP_
#define OWNED_NUMBER_HPP_
#define UTF8_EXPORT_HPP_
#define VALUE_INDEX_HPP_
#define VALUE_ITERATOR_HPP_
#define VALUE_KIND_HPP_
#define VALUE_VIEW_HPP_
#include "adaptive_json5/adaptive_json5.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */

  // Compiling these checks is the main evidence: each type comes from a header whose old guard is defined.
  static_assert(std::is_class_v<json5_document>);
  static_assert(std::is_class_v<json5_value>);
  static_assert(std::is_class_v<input_view>);
  static_assert(std::is_class_v<parse_options>);
  static_assert(std::is_class_v<document>);
  static_assert(std::is_class_v<buffer_arena>);
  static_assert(std::is_class_v<accounted_storage>);
  static_assert(std::is_enum_v<value_kind>);

  constexpr std::size_t c_buffer_size = 4096;

  /**
   * @brief Caller storage suitably aligned for the control block.
   * @par Requirements
   * J5-092; J5-101.
   */
  struct alignas(std::max_align_t) caller_buffer
  {
      std::byte bytes[c_buffer_size];
  };
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_092 =================================== */

  /* Verify consumer macros named like the old unprefixed guards do not hide library declarations.
   * Arrange: The old public guard names defined before the umbrella header, a JSON5 array and a caller buffer.
   * Act: Parse the array and read its only element.
   * Expect: The parse succeeds and the element is 7.
   */
  TEST(J5_092_IncludeGuards, ConsumerGuardNamesDoNotHideHeaders)
  {
    /* Arrange */
    const std::string text = "[7,]";
    caller_buffer memory{};
    /* Act */
    const auto parsed = json5_document::parse({text.data(), text.size()}, memory.bytes, sizeof(memory.bytes));
    ASSERT_TRUE(parsed.has_value());
    const auto first = parsed.value_if()->root().at(0).as_int64();
    /* Expect */
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first.value_if(), 7);
  }
  /* ================================= End Test_J5_092 ================================= */
} // namespace
