#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/value_kind.hpp"
#include "adaptive_json5/value/value_view.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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
      // Element type keeps the caller buffer aligned for any allocation.
      std::vector<std::max_align_t> buffer;
      std::optional<json5_document> document{};
      std::optional<error> failure{};

      /**
       * @brief Parse text into the fixture's buffer.
       * @par Requirements
       * P037; J5-026.
       * @param text Complete input.
       * @param buffer_bytes Caller buffer size in bytes, rounded up to whole max_align_t elements.
       * @param options Syntax, policies and limits for the parse.
       */
      explicit parsed_fixture(const std::string& text, std::size_t buffer_bytes = 16384U,
                              const parse_options& options = {})
          : buffer((buffer_bytes + sizeof(std::max_align_t) - 1U) / sizeof(std::max_align_t))
      {
        auto parsed = json5_document::parse({text.data(), text.size()}, buffer.data(),
                                            buffer.size() * sizeof(std::max_align_t), options);

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

  /* Verify doubles are read through the document's own storage without using it up.
   * Arrange: A ratio, a long decimal that needs exact rounding and Infinity, in a 16 KiB
   *          caller buffer.
   * Act: Read each with as_double(), then read the long decimal once per buffer byte.
   * Expect: The correctly rounded values, numeric_conversion for Infinity under the default
   *         policy, and no lasting change in the document's bytes. Arena oracle: if a read
   *         kept even one byte of the buffer, the repeated reads would exhaust it.
   */
  TEST(J5_037_Json5Value, ReadsDoublesUsingDocumentScratch)
  {
    /* Arrange */
    parsed_fixture fixture{R"([0.25, 0.1000000000000000055511151231257827021181583404541015625, Infinity])"};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value values = fixture.document->root();
    const std::size_t bytes_before = fixture.document->current_bytes();
    const std::size_t buffer_bytes = fixture.buffer.size() * sizeof(std::max_align_t);
    /* Act */
    const auto quarter = values.at(0).as_double();
    const auto tenth = values.at(1).as_double();
    const auto infinity = values.at(2).as_double();
    std::size_t repeated_failures{};
    for (std::size_t read{}; read < buffer_bytes; ++read)
    {
      const auto again = values.at(1).as_double();
      repeated_failures += (again.has_value() && (*again.value_if() == 0.1)) ? 0U : 1U;
    }
    /* Expect */
    ASSERT_TRUE(quarter.has_value());
    EXPECT_EQ(*quarter.value_if(), 0.25);
    ASSERT_TRUE(tenth.has_value());
    EXPECT_EQ(*tenth.value_if(), 0.1);
    ASSERT_FALSE(infinity.has_value());
    EXPECT_EQ(infinity.error_if()->code, error_code::numeric_conversion);
    EXPECT_EQ(repeated_failures, 0U);
    EXPECT_EQ(fixture.document->current_bytes(), bytes_before);
  }
  /* ================================= End Test_J5_036 ================================= */
  /* =================================== Test_J5_037 =================================== */

  /**
   * @brief Build a document with short, 300-digit and subnormal numbers (issue #124).
   * @par Requirements
   * J5-037.
   */
  std::string long_number_document()
  {
    return "{ port: 8080, ratio: 1.5, long: 0." + std::string(300U, '1') + ", big: 1" + std::string(299U, '7') +
           ", tiny: 4.9406564584124654e-324 }";
  }

  /* Verify 10000 reads of every number succeed on a 4 KiB caller buffer (issue #124).
   * Arrange: Short, 300-digit and subnormal numbers parsed into a 4 KiB buffer; the first
   *          value of each read.
   * Act: Read every number 10000 times with as_double(), and the long one with as_float().
   * Expect: Every read succeeds with the first value; current_bytes() is unchanged and
   *         peak_bytes() does not grow after the first round. Arena oracle: 10000 reads each
   *         keeping one byte would exceed the whole buffer.
   */
  TEST(J5_037_Json5Value, RepeatsReadsOnASmallCallerBuffer)
  {
    /* Arrange */
    parsed_fixture fixture{long_number_document(), 4096U};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value root = fixture.document->root();
    const char* const keys[] = {"port", "ratio", "long", "big", "tiny"};
    double first[5]{};
    for (std::size_t key{}; key < 5U; ++key)
    {
      const auto value = root.member(keys[key]).as_double();
      ASSERT_TRUE(value.has_value()) << keys[key];
      first[key] = *value.value_if();
    }
    const auto first_float = root.member("long").as_float();
    ASSERT_TRUE(first_float.has_value());
    const std::size_t current_before = fixture.document->current_bytes();
    const std::size_t peak_before = fixture.document->peak_bytes();
    std::size_t failures{};
    /* Act */
    for (std::size_t read{}; read < 10000U; ++read)
    {
      for (std::size_t key{}; key < 5U; ++key)
      {
        const auto value = root.member(keys[key]).as_double();
        failures += (value.has_value() && (*value.value_if() == first[key])) ? 0U : 1U;
      }
      const auto single = root.member("long").as_float();
      failures += (single.has_value() && (*single.value_if() == *first_float.value_if())) ? 0U : 1U;
    }
    /* Expect */
    EXPECT_EQ(failures, 0U);
    EXPECT_EQ(first[0], 8080.0);
    EXPECT_EQ(first[1], 1.5);
    EXPECT_EQ(first[4], 0x1p-1074);
    EXPECT_EQ(fixture.document->current_bytes(), current_before);
    EXPECT_EQ(fixture.document->peak_bytes(), peak_before);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the scratch overloads use only the caller's provider.
   * Arrange: The long-number document; a guarded caller provider; an empty storage_callbacks{}.
   * Act: Read the 300-digit number and the ratio through as_double(scratch) and
   *      as_float(scratch), with each provider; read through a held error.
   * Expect: The same values as the plain reads; the caller provider is used and fully
   *         released while the document's bytes and peak stay unchanged; storage_callbacks{}
   *         reads the ratio and reports allocation_failure for the long number; a held error
   *         passes through without any scratch request.
   */
  TEST(J5_037_Json5Value, ReadsWithCallerScratch)
  {
    /* Arrange */
    parsed_fixture fixture{long_number_document()};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value root = fixture.document->root();
    const json5_value long_value = root.member("long");
    const json5_value ratio = root.member("ratio");
    const auto plain_double = long_value.as_double();
    const auto plain_float = long_value.as_float();
    ASSERT_TRUE(plain_double.has_value());
    ASSERT_TRUE(plain_float.has_value());
    const std::size_t current_before = fixture.document->current_bytes();
    const std::size_t peak_before = fixture.document->peak_bytes();
    adaptive_json5_test::guard_provider scratch;
    /* Act */
    const auto with_double = long_value.as_double(scratch.callbacks());
    const auto with_float = long_value.as_float(scratch.callbacks());
    const std::size_t used_requests = scratch.m_attempts;
    const auto ratio_without = ratio.as_double(storage_callbacks{});
    const auto ratio_float_without = ratio.as_float(storage_callbacks{});
    const auto long_without = long_value.as_double(storage_callbacks{});
    const auto long_float_without = long_value.as_float(storage_callbacks{});
    const auto from_error = root.member("absent").as_double(scratch.callbacks());
    /* Expect */
    ASSERT_TRUE(with_double.has_value());
    EXPECT_EQ(*with_double.value_if(), *plain_double.value_if());
    ASSERT_TRUE(with_float.has_value());
    EXPECT_EQ(*with_float.value_if(), *plain_float.value_if());
    EXPECT_GT(used_requests, 0U);
    EXPECT_EQ(scratch.m_live_bytes, 0U);
    EXPECT_EQ(fixture.document->current_bytes(), current_before);
    EXPECT_EQ(fixture.document->peak_bytes(), peak_before);
    ASSERT_TRUE(ratio_without.has_value());
    EXPECT_EQ(*ratio_without.value_if(), 1.5);
    ASSERT_TRUE(ratio_float_without.has_value());
    EXPECT_EQ(*ratio_float_without.value_if(), 1.5F);
    ASSERT_FALSE(long_without.has_value());
    EXPECT_EQ(long_without.error_if()->code, error_code::allocation_failure);
    ASSERT_FALSE(long_float_without.has_value());
    EXPECT_EQ(long_float_without.error_if()->code, error_code::allocation_failure);
    ASSERT_FALSE(from_error.has_value());
    EXPECT_EQ(from_error.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(scratch.m_attempts, used_requests);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify as_float() rounds once, directly to float, and reports float range errors.
   * Arrange: A tenth, a float tie, two spellings that double rounding would get wrong (one
   *          short, one long), a value above FLT_MAX and a string.
   * Act: Read each with as_float().
   * Expect: 0.1F, 16777216, 1 + 2^-23 twice, numeric_conversion and wrong_type_or_access.
   */
  TEST(J5_037_Json5Value, ReadsFloatsRoundedOnce)
  {
    /* Arrange */
    parsed_fixture fixture{R"([0.1, 16777217, 1.0000000596046448, 1.00000005960464477550, 3.4028236e38, "x"])"};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value values = fixture.document->root();
    /* Act */
    const auto tenth = values.at(0).as_float();
    const auto tie = values.at(1).as_float();
    const auto short_spelling = values.at(2).as_float();
    const auto long_spelling = values.at(3).as_float();
    const auto too_large = values.at(4).as_float();
    const auto text = values.at(5).as_float();
    /* Expect */
    ASSERT_TRUE(tenth.has_value());
    EXPECT_EQ(*tenth.value_if(), 0.1F);
    ASSERT_TRUE(tie.has_value());
    EXPECT_EQ(*tie.value_if(), 16777216.0F);
    ASSERT_TRUE(short_spelling.has_value());
    EXPECT_EQ(*short_spelling.value_if(), 0x1.000002p0F);
    ASSERT_TRUE(long_spelling.has_value());
    EXPECT_EQ(*long_spelling.value_if(), 0x1.000002p0F);
    ASSERT_FALSE(too_large.has_value());
    EXPECT_EQ(too_large.error_if()->code, error_code::numeric_conversion);
    ASSERT_FALSE(text.has_value());
    EXPECT_EQ(text.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_037 ================================= */
  /* =================================== Test_J5_031 =================================== */

  /**
   * @brief Build an object with members k0..k(count-1) whose values are their positions.
   * @par Requirements
   * J5-030; J5-031.
   * @param count Number of members.
   * @param duplicate_last When true, the last member reuses the name k0.
   */
  std::string numbered_object(std::size_t count, bool duplicate_last)
  {
    std::string text = "{";
    for (std::size_t index{}; index < count; ++index)
    {
      const bool reuse = duplicate_last && (index + 1U == count);
      text += "k" + std::to_string(reuse ? 0U : index) + ": " + std::to_string(index) + ",";
    }
    return text + "}";
  }

  // Each record takes buffer space; 1024 members need more than the default fixture buffer.
  constexpr std::size_t c_large_buffer_bytes = 1048576U;

  /**
   * @brief Parse options that admit an object with max_members members.
   * @par Requirements
   * J5-031; J5-048.
   * @note max_nodes also counts the root, so the default 1024 nodes allow only 1023 members;
   * input and owned-byte limits are raised for the long text.
   */
  parse_options large_object_options()
  {
    parse_options options{};
    options.limits.max_nodes = options.limits.max_members + 1U;
    options.limits.max_input_bytes = 65536U;
    options.limits.max_owned_bytes = c_large_buffer_bytes;
    return options;
  }

  class J5_031_MemberLookup : public ::testing::TestWithParam<std::size_t>
  {
  };

  /* Verify member() on objects of 0, 1, 2 and 1024 members (the default max_members).
   * Arrange: An object k0..k(n-1), and for n >= 2 the same object whose last name repeats k0.
   * Act: Look up the first, the last and a missing name in each.
   * Expect: First and last give their positions; a missing name gives wrong_type_or_access;
   *         in the duplicated object, k0 gives wrong_type_or_access and the others still resolve.
   */
  TEST_P(J5_031_MemberLookup, FindsFirstLastMissingAndDuplicate)
  {
    /* Arrange */
    const std::size_t count = GetParam();
    parsed_fixture fixture{numbered_object(count, false), c_large_buffer_bytes, large_object_options()};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value root = fixture.document->root();
    const std::string last = "k" + std::to_string((count == 0U) ? 0U : count - 1U);
    /* Act */
    const auto first_value = root.member("k0").as_uint64();
    const auto last_value = root.member(last).as_uint64();
    const json5_value missing = root.member("absent");
    const auto size = root.size();
    /* Expect */
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(*size.value_if(), count);
    expect_error(missing, error_code::wrong_type_or_access);
    if (count == 0U)
    {
      EXPECT_EQ(first_value.error_if()->code, error_code::wrong_type_or_access);
      return;
    }
    ASSERT_TRUE(first_value.has_value());
    EXPECT_EQ(*first_value.value_if(), 0U);
    ASSERT_TRUE(last_value.has_value());
    EXPECT_EQ(*last_value.value_if(), count - 1U);
    if (count < 2U)
    {
      return;
    }
    parsed_fixture duplicated{numbered_object(count, true), c_large_buffer_bytes, large_object_options()};
    ASSERT_TRUE(duplicated.document.has_value());
    const json5_value duplicated_root = duplicated.document->root();
    expect_error(duplicated_root.member("k0"), error_code::wrong_type_or_access);
    if (count > 2U)
    {
      const std::string before_last = "k" + std::to_string(count - 2U);
      EXPECT_EQ(*duplicated_root.member(before_last).as_uint64().value_if(), count - 2U);
    }
  }

  INSTANTIATE_TEST_SUITE_P(Sizes, J5_031_MemberLookup, ::testing::Values(0U, 1U, 2U, 1024U));

  /* ----------------------------------------------------------------------------------- */

  /* Verify value_view::find_member() directly, including views it must reject.
   * Arrange: An object with a duplicated name, an array and a default (invalid) view.
   * Act: Find present, duplicated, missing and invalid UTF-8 names, and look into the array
   *      and the invalid view.
   * Expect: The present member is the same value member_at() returns; duplicates, missing
   *         names, arrays and invalid views give wrong_type_or_access; invalid UTF-8 gives
   *         encoding.
   */
  TEST(J5_031_ValueView, FindMemberResolvesOnlyUniqueNames)
  {
    /* Arrange */
    parsed_fixture object{R"({a: 1, b: 2, a: 3, c: 4})"};
    parsed_fixture array{R"([1, 2])"};
    ASSERT_TRUE(object.document.has_value());
    ASSERT_TRUE(array.document.has_value());
    const auto object_root = object.document->values().root();
    const auto array_root = array.document->values().root();
    ASSERT_TRUE(object_root.has_value());
    ASSERT_TRUE(array_root.has_value());
    const value_view& members = *object_root.value_if();
    /* Act */
    const auto found = members.find_member("c");
    const auto by_position = members.member_at(3U);
    const auto duplicated = members.find_member("a");
    const auto missing = members.find_member("d");
    const auto invalid_name = members.find_member("\xC3");
    const auto in_array = array_root.value_if()->find_member("a");
    const auto in_invalid_view = value_view{}.find_member("a");
    /* Expect */
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(by_position.has_value());
    // The same record yields the same borrowed payload address.
    EXPECT_EQ(*found.value_if()->as_number().value_if(), *by_position.value_if()->as_number().value_if());
    EXPECT_EQ(duplicated.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(missing.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(invalid_name.error_if()->code, error_code::encoding);
    EXPECT_EQ(in_array.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(in_invalid_view.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_031 ================================= */
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
  /* ----------------------------------------------------------------------------------- */

  /* Verify a string and a member name with an embedded zero byte keep their full length.
   * Arrange: A member named "a\u0000b" whose value is "admin\u0000x".
   * Act: Look up the member by its exact three-byte name and by the truncated name "a", and
   *      export the value with as_string().
   * Expect: The exact name finds the member and the truncated name does not; the value has
   *         all 7 bytes with the zero byte at index 5, so a C string function such as
   *         strlen() would see only "admin" (integrator guidance, TARA threat T-06).
   */
  TEST(J5_029_Json5Value, KeepsEmbeddedZeroBytesAtFullLength)
  {
    /* Arrange */
    parsed_fixture fixture{R"({"a\u0000b": "admin\u0000x"})"};
    ASSERT_TRUE(fixture.document.has_value());
    const json5_value root = fixture.document->root();
    char text[16]{};
    /* Act */
    const json5_value exact = root.member(std::string_view{"a\0b", 3U});
    const json5_value truncated = root.member("a");
    const auto value = exact.as_string(text, sizeof(text));
    /* Expect */
    ASSERT_TRUE(exact.ok());
    EXPECT_FALSE(truncated.ok());
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value.value_if()->size(), 7U);
    EXPECT_EQ(*value.value_if(), (std::string_view{"admin\0x", 7U}));
    EXPECT_EQ(text[5], '\0');
    EXPECT_EQ(std::strlen(text), 5U);
  }
  /* ================================= End Test_J5_029 ================================= */
  /* =================================== Test_J5_032 =================================== */

  /* Verify duplicate member names are rejected when reject_duplicates is selected.
   * Arrange: An object with the member name "a" twice, parsed once with the default options,
   *          and options selecting duplicate_name_policy::reject_duplicates.
   * Act: Look up "a" in the default document, and parse the same text with reject_duplicates.
   * Expect: Under the default policy member("a") fails instead of picking one value; with
   *         reject_duplicates the parse fails with duplicate_policy (integrator guidance,
   *         TARA threat T-02).
   */
  TEST(J5_032_Json5Document, RejectsDuplicateNamesWhenConfigured)
  {
    /* Arrange */
    const std::string text{"{a: 1, a: 2}"};
    parsed_fixture preserved{text};
    ASSERT_TRUE(preserved.document.has_value());
    alignas(std::max_align_t) static unsigned char buffer[4096];
    parse_options strict_names;
    strict_names.duplicate_names = duplicate_name_policy::reject_duplicates;
    /* Act */
    const json5_value ambiguous = preserved.document->root().member("a");
    const auto rejected = json5_document::parse({text.data(), text.size()}, buffer, sizeof(buffer), strict_names);
    /* Expect */
    EXPECT_FALSE(ambiguous.ok());
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->code, error_code::duplicate_policy);
  }
  /* ================================= End Test_J5_032 ================================= */
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
