#include "adaptive_json5/error.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/value_kind.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */

  static_assert(noexcept(std::declval<const json5_value&>().member(std::string_view{})));
  static_assert(noexcept(std::declval<const json5_value&>().at(0U)));
  static_assert(noexcept(std::declval<const json5_value&>().as_double()));
  static_assert(std::is_copy_constructible_v<json5_value>);
  static_assert(!std::is_copy_assignable_v<json5_value>);
  static_assert(std::is_same_v<decltype(std::declval<const json5_value&>().member(std::string_view{})), json5_value>);

  /**
   * @brief Own a caller buffer and the document parsed into it, for one unit.
   * @par Requirements
   * J5-040; J5-101.
   * @note The buffer must outlive the document, so both live in one object at a stable address.
   */
  struct parsed_fixture
  {
      alignas(std::max_align_t) unsigned char buffer[16384]{};
      std::optional<json5_document> document{};
      std::optional<error> failure{};

      /**
       * @brief Parse text into the fixture's buffer.
       * @par Requirements
       * P037; J5-026.
       */
      explicit parsed_fixture(const std::string& text)
      {
        auto parsed = json5_document::parse({text.data(), text.size()}, buffer, sizeof(buffer));

        if (parsed.has_value())
        {
          document.emplace(std::move(*parsed.value_if()));
        }
        else
        {
          failure = *parsed.error_if();
        }
      }

      parsed_fixture(const parsed_fixture&) = delete;
      parsed_fixture& operator=(const parsed_fixture&) = delete;
  };

  const std::string c_config = R"({
    server: {host: "example.org", port: 8080, secure: true, ratio: 0.25},
    tags: ["a", "b", "c"],
    name: "café",
    empty: null,
  })";

  /**
   * @brief Expect a value to hold an error with the given code.
   * @par Requirements
   * J5-039; J5-043.
   */
  void expect_error(const json5_value& value, error_code code)
  {
    ASSERT_FALSE(value.ok());
    ASSERT_NE(value.error_if(), nullptr);
    EXPECT_EQ(value.error_if()->code, code);
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_030 =================================== */

  /* Verify chained navigation reaches nested members and elements.
   * Arrange: A configuration object with a nested object, an array and scalars.
   * Act: Navigate with member() and at() chains and read each value.
   * Expect: The exact values and kinds. Each string gets its own buffer, because a returned
   *         view is valid only while its buffer is unchanged.
   */
  TEST(J5_030_Json5Value, NavigatesNestedValues)
  {
    /* Arrange */
    parsed_fixture fixture{c_config};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value root = fixture.document->root();
    char host_text[32];
    char tag_text[8];
    /* Act */
    const auto port = root.member("server").member("port").as_uint64();
    const auto secure = root.member("server").member("secure").as_bool();
    const auto host = root.member("server").member("host").as_string(host_text, sizeof(host_text));
    const auto tag = root.member("tags").at(1).as_string(tag_text, sizeof(tag_text));
    const auto tags = root.member("tags").size();
    const auto members = root.size();
    /* Expect */
    EXPECT_TRUE(root.is_object());
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*port.value_if(), 8080U);
    ASSERT_TRUE(secure.has_value());
    EXPECT_TRUE(*secure.value_if());
    ASSERT_TRUE(host.has_value());
    EXPECT_EQ(*host.value_if(), "example.org");
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(*tag.value_if(), "b");
    ASSERT_TRUE(tags.has_value());
    EXPECT_EQ(*tags.value_if(), 3U);
    ASSERT_TRUE(members.has_value());
    EXPECT_EQ(*members.value_if(), 4U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the kind checks and kind() for every kind.
   * Arrange: The configuration document.
   * Act: Query each kind check on one value of each kind.
   * Expect: Exactly one check is true per value; all are false for an error.
   */
  TEST(J5_030_Json5Value, ReportsEveryKind)
  {
    /* Arrange */
    parsed_fixture fixture{c_config};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value root = fixture.document->root();
    const json5_value missing = root.member("absent");
    /* Act and Expect */
    EXPECT_TRUE(root.member("empty").is_null());
    EXPECT_TRUE(root.member("server").member("secure").is_boolean());
    EXPECT_TRUE(root.member("name").is_string());
    EXPECT_TRUE(root.member("server").member("port").is_number());
    EXPECT_TRUE(root.member("tags").is_array());
    EXPECT_TRUE(root.member("server").is_object());
    EXPECT_FALSE(root.member("tags").is_object());
    const auto kind = root.member("tags").kind();
    ASSERT_TRUE(kind.has_value());
    EXPECT_EQ(*kind.value_if(), value_kind::array);
    EXPECT_FALSE(missing.is_null() || missing.is_boolean() || missing.is_string() || missing.is_number() ||
                 missing.is_array() || missing.is_object());
    EXPECT_FALSE(missing.kind().has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify member names match decoded names exactly, whatever their source spelling.
   * Arrange: Members spelled with escapes, single quotes, an identifier and non-ASCII text.
   * Act: Look them up by plain UTF-8 names.
   * Expect: Each is found; a different case or a prefix is not.
   */
  TEST(J5_033_Json5Value, MatchesDecodedNamesExactly)
  {
    /* Arrange */
    parsed_fixture fixture{R"({"ab": 1, 'café': 2, plain: 3, "😀": 4, "": 5})"};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value root = fixture.document->root();
    /* Act and Expect */
    EXPECT_EQ(*root.member("ab").as_uint64().value_if(), 1U);
    EXPECT_EQ(*root.member("caf\xC3\xA9").as_uint64().value_if(), 2U);
    EXPECT_EQ(*root.member("plain").as_uint64().value_if(), 3U);
    EXPECT_EQ(*root.member("\xF0\x9F\x98\x80").as_uint64().value_if(), 4U);
    EXPECT_EQ(*root.member("").as_uint64().value_if(), 5U);
    expect_error(root.member("AB"), error_code::wrong_type_or_access);
    expect_error(root.member("a"), error_code::wrong_type_or_access);
    expect_error(root.member("abc"), error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_033 ================================= */
  /* =================================== Test_J5_039 =================================== */

  /* Verify navigation failures report wrong_type_or_access or encoding.
   * Arrange: The configuration document and a document with a duplicate name.
   * Act: Ask for a missing member, a member of an array, an element of an object, an index
   *      out of range, a duplicate name and a name that is not valid UTF-8.
   * Expect: wrong_type_or_access for each, except encoding for the invalid name.
   */
  TEST(J5_039_Json5Value, ReportsNavigationFailures)
  {
    /* Arrange */
    parsed_fixture fixture{c_config};
    parsed_fixture duplicates{R"({"a": 1, "a": 2, "b": 3})"};
    ASSERT_TRUE(fixture.document.has_value());
    ASSERT_TRUE(duplicates.document.has_value());
    const json5_value root = fixture.document->root();
    /* Act and Expect */
    expect_error(root.member("absent"), error_code::wrong_type_or_access);
    expect_error(root.member("tags").member("a"), error_code::wrong_type_or_access);
    expect_error(root.at(0), error_code::wrong_type_or_access);
    expect_error(root.member("tags").at(3), error_code::wrong_type_or_access);
    expect_error(duplicates.document->root().member("a"), error_code::wrong_type_or_access);
    EXPECT_EQ(*duplicates.document->root().member("b").as_uint64().value_if(), 3U);
    expect_error(root.member("\xC3"), error_code::encoding);
    const auto scalar_size = root.member("name").size();
    ASSERT_FALSE(scalar_size.has_value());
    EXPECT_EQ(scalar_size.error_if()->code, error_code::wrong_type_or_access);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the first failure in a chain is the one reported at the end.
   * Arrange: The configuration document.
   * Act: Chain member(), at() and every read after a missing member.
   * Expect: Every step reports the original wrong_type_or_access.
   */
  TEST(J5_039_Json5Value, PassesTheFirstErrorThroughAChain)
  {
    /* Arrange */
    parsed_fixture fixture{c_config};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value lost = fixture.document->root().member("absent").member("deeper").at(2);
    char text[8];
    /* Act */
    const auto as_bool = lost.as_bool();
    const auto as_int = lost.as_int64();
    const auto as_double = lost.as_double();
    const auto as_string = lost.as_string(text, sizeof(text));
    /* Expect */
    expect_error(lost, error_code::wrong_type_or_access);
    EXPECT_EQ(as_bool.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(as_int.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(as_double.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(as_string.error_if()->code, error_code::wrong_type_or_access);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify reads reject values of another kind.
   * Arrange: The configuration document.
   * Act: Read a string as a number and a boolean, and a number as a string.
   * Expect: wrong_type_or_access each time.
   */
  TEST(J5_039_Json5Value, RejectsReadsOfAnotherKind)
  {
    /* Arrange */
    parsed_fixture fixture{c_config};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value name = fixture.document->root().member("name");
    const json5_value port = fixture.document->root().member("server").member("port");
    char text[16];
    /* Act */
    const auto number = name.as_double();
    const auto boolean = name.as_bool();
    const auto integer = name.as_uint64();
    const auto string = port.as_string(text, sizeof(text));
    /* Expect */
    EXPECT_EQ(number.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(boolean.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(integer.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(string.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_039 ================================= */
  /* =================================== Test_J5_036 =================================== */

  /* Verify exact integer reads and their limits.
   * Arrange: Integral spellings, a fraction, negatives and both 64-bit edges.
   * Act: Read each as int64 and uint64.
   * Expect: Exact values; a fraction, a negative uint64 and values out of range fail with a
   *         numeric conversion error.
   */
  TEST(J5_036_Json5Value, ReadsIntegersExactly)
  {
    /* Arrange */
    parsed_fixture fixture{
      R"([1e2, 1.0, 1.5, -7, 9223372036854775807, 9223372036854775808, 18446744073709551615, 0x1F])"};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value values = fixture.document->root();
    /* Act and Expect */
    EXPECT_EQ(*values.at(0).as_int64().value_if(), 100);
    EXPECT_EQ(*values.at(1).as_uint64().value_if(), 1U);
    EXPECT_EQ(values.at(2).as_int64().error_if()->code, error_code::numeric_conversion);
    EXPECT_EQ(*values.at(3).as_int64().value_if(), -7);
    EXPECT_EQ(values.at(3).as_uint64().error_if()->code, error_code::numeric_conversion);
    EXPECT_EQ(*values.at(4).as_int64().value_if(), std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(values.at(5).as_int64().error_if()->code, error_code::numeric_conversion);
    EXPECT_EQ(*values.at(5).as_uint64().value_if(), 9223372036854775808U);
    EXPECT_EQ(*values.at(6).as_uint64().value_if(), std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(*values.at(7).as_int64().value_if(), 31);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify doubles are read through the document's own storage.
   * Arrange: A ratio, a long decimal that needs exact rounding and Infinity.
   * Act: Read each with as_double() and compare current_bytes() before and after.
   * Expect: The correctly rounded values, numeric_conversion for Infinity under the default
   *         policy, and no lasting change in the document's bytes.
   */
  TEST(J5_037_Json5Value, ReadsDoublesUsingDocumentScratch)
  {
    /* Arrange */
    parsed_fixture fixture{R"([0.25, 0.1000000000000000055511151231257827021181583404541015625, Infinity])"};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value values = fixture.document->root();
    const std::size_t bytes_before = fixture.document->current_bytes();
    /* Act */
    const auto quarter = values.at(0).as_double();
    const auto tenth = values.at(1).as_double();
    const auto infinity = values.at(2).as_double();
    /* Expect */
    ASSERT_TRUE(quarter.has_value());
    EXPECT_EQ(*quarter.value_if(), 0.25);
    ASSERT_TRUE(tenth.has_value());
    EXPECT_EQ(*tenth.value_if(), 0.1);
    ASSERT_FALSE(infinity.has_value());
    EXPECT_EQ(infinity.error_if()->code, error_code::numeric_conversion);
    EXPECT_EQ(fixture.document->current_bytes(), bytes_before);
  }
  /* ================================= End Test_J5_036 ================================= */
  /* =================================== Test_J5_029 =================================== */

  /* Verify strings are exported as UTF-8 into the caller's buffer.
   * Arrange: An escaped non-ASCII name, an empty string and a buffer of exact and short sizes.
   * Act: Read the strings with as_string().
   * Expect: The UTF-8 bytes in the buffer; an exactly sized buffer works; a short one fails
   *         with resource_limit.
   */
  TEST(J5_029_Json5Value, ExportsStringsAsUtf8)
  {
    /* Arrange */
    parsed_fixture fixture{R"(["café", ""])"};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value values = fixture.document->root();
    char exact[5];
    char short_buffer[4];
    /* Act */
    const auto cafe = values.at(0).as_string(exact, sizeof(exact));
    const auto too_short = values.at(0).as_string(short_buffer, sizeof(short_buffer));
    const auto empty = values.at(1).as_string(short_buffer, 0U);
    /* Expect */
    ASSERT_TRUE(cafe.has_value());
    EXPECT_EQ(*cafe.value_if(), "caf\xC3\xA9");
    EXPECT_EQ(cafe.value_if()->data(), exact);
    ASSERT_FALSE(too_short.has_value());
    EXPECT_EQ(too_short.error_if()->code, error_code::resource_limit);
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty.value_if()->empty());
  }
  /* ================================= End Test_J5_029 ================================= */
  /* =================================== Test_J5_040 =================================== */

  /* Verify root() of a moved-from document holds an error instead of failing unsafely.
   * Arrange: A parsed document moved into another one.
   * Act: Call root() on both.
   * Expect: The destination navigates; the moved-from root holds an error.
   */
  TEST(J5_040_Json5Value, MovedFromDocumentRootHoldsError)
  {
    /* Arrange */
    parsed_fixture fixture{c_config};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_document moved{std::move(*fixture.document)};
    /* Act */
    const json5_value live = moved.root();
    const json5_value empty = fixture.document->root();
    /* Expect */
    EXPECT_EQ(*live.member("server").member("port").as_uint64().value_if(), 8080U);
    EXPECT_FALSE(empty.ok());
    EXPECT_NE(empty.error_if(), nullptr);
  }
  /* ================================= End Test_J5_040 ================================= */
} // namespace
