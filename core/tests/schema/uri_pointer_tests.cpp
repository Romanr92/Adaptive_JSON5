#include "../../../tests/schema/schema_test_storage.hpp"
#include "schema/json_pointer.hpp"
#include "schema/pod_vector.hpp"
#include "schema/uri.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::schema_test_storage;

  /**
   * @brief Resolve reference against base and return the text, or "<error>".
   * @par Requirements
   * P044; J5-055.
   */
  std::string resolve(std::string_view base, std::string_view reference)
  {
    schema_test_storage storage;
    pod_vector<char> out{storage.callbacks()};
    const auto resolved = resolve_uri(base, reference, out, storage.callbacks());
    std::string text = resolved.has_value() ? std::string{text_of(out, 0U, out.size())} : std::string{"<error>"};
    static_cast<void>(out.release());
    return text;
  }

  /* =================================== Test_P044_UriResolution ==================================== */
  struct resolution_case
  {
      const char* reference;
      const char* expected;
  };

  class P044_UriResolution : public testing::TestWithParam<resolution_case>
  {
  };

  /*
   * Arrange: The normal and abnormal examples of RFC 3986 section 5.4 with base http://a/b/c/d;p?q.
   * Act: Resolve each reference.
   * Expect: The RFC's target URI.
   */
  TEST_P(P044_UriResolution, MatchesRfc3986Examples)
  {
    EXPECT_EQ(resolve("http://a/b/c/d;p?q", GetParam().reference), GetParam().expected) << GetParam().reference;
  }

  INSTANTIATE_TEST_SUITE_P(
    Rfc3986, P044_UriResolution,
    testing::Values(resolution_case{"g:h", "g:h"}, resolution_case{"g", "http://a/b/c/g"},
                    resolution_case{"./g", "http://a/b/c/g"}, resolution_case{"g/", "http://a/b/c/g/"},
                    resolution_case{"/g", "http://a/g"}, resolution_case{"//g", "http://g"},
                    resolution_case{"?y", "http://a/b/c/d;p?y"}, resolution_case{"g?y", "http://a/b/c/g?y"},
                    resolution_case{"#s", "http://a/b/c/d;p?q#s"}, resolution_case{"g#s", "http://a/b/c/g#s"},
                    resolution_case{"g?y#s", "http://a/b/c/g?y#s"}, resolution_case{";x", "http://a/b/c/;x"},
                    resolution_case{"g;x", "http://a/b/c/g;x"}, resolution_case{"", "http://a/b/c/d;p?q"},
                    resolution_case{".", "http://a/b/c/"}, resolution_case{"./", "http://a/b/c/"},
                    resolution_case{"..", "http://a/b/"}, resolution_case{"../", "http://a/b/"},
                    resolution_case{"../g", "http://a/b/g"}, resolution_case{"../..", "http://a/"},
                    resolution_case{"../../", "http://a/"}, resolution_case{"../../g", "http://a/g"},
                    resolution_case{"../../../g", "http://a/g"}, resolution_case{"../../../../g", "http://a/g"},
                    resolution_case{"/./g", "http://a/g"}, resolution_case{"/../g", "http://a/g"},
                    resolution_case{"g.", "http://a/b/c/g."}, resolution_case{".g", "http://a/b/c/.g"},
                    resolution_case{"g..", "http://a/b/c/g.."}, resolution_case{"..g", "http://a/b/c/..g"},
                    resolution_case{"./../g", "http://a/b/g"}, resolution_case{"./g/.", "http://a/b/c/g/"},
                    resolution_case{"g/./h", "http://a/b/c/g/h"}, resolution_case{"g/../h", "http://a/b/c/h"},
                    resolution_case{"g;x=1/./y", "http://a/b/c/g;x=1/y"},
                    resolution_case{"g;x=1/../y", "http://a/b/c/y"}));

  /*
   * Arrange: URIs differing in scheme/host case, dot segments and an empty fragment.
   * Act: Normalize them by resolving each against itself.
   * Expect: Scheme and host lowercased, dots removed, empty fragment dropped; path case
   *         and percent-encoding unchanged (ADR 0006 decision 8).
   */
  TEST(P044_UriNormalization, LowercasesSchemeAndHostOnly)
  {
    EXPECT_EQ(resolve("HTTP://Example.COM/A/./b/../C#", "HTTP://Example.COM/A/./b/../C#"), "http://example.com/A/C");
    EXPECT_EQ(resolve("http://User@HOST:8080/p", ""), "http://User@host:8080/p");
    EXPECT_EQ(resolve("http://x/%7Ea", ""), "http://x/%7Ea");
    EXPECT_EQ(resolve("urn:example:a", "#frag"), "urn:example:a#frag");
    EXPECT_EQ(resolve("relative/base", "g"), "<error>");
  }

  /*
   * Arrange: URI texts with and without components.
   * Act: Split them.
   * Expect: The five components and which ones are present.
   */
  TEST(P044_UriNormalization, SplitsComponents)
  {
    const auto parts = split_uri("http://h/p?q#f");
    EXPECT_TRUE(parts.has_scheme && parts.has_authority && parts.has_query && parts.has_fragment);
    EXPECT_EQ(parts.scheme, "http");
    EXPECT_EQ(parts.authority, "h");
    EXPECT_EQ(parts.path, "/p");
    EXPECT_EQ(parts.query, "q");
    EXPECT_EQ(parts.fragment, "f");
    const auto fragment_only = split_uri("#foo");
    EXPECT_FALSE(fragment_only.has_scheme);
    EXPECT_TRUE(fragment_only.path.empty());
    EXPECT_EQ(fragment_only.fragment, "foo");
    EXPECT_FALSE(split_uri("1abc:x").has_scheme);
    EXPECT_EQ(without_fragment("a#b#c"), "a");
  }

  /* =================================== Test_P044_JsonPointer ==================================== */
  /**
   * @brief Decode every token of pointer into "|"-joined text, or "<error>".
   * @par Requirements
   * P044; J5-055.
   */
  std::string tokens(std::string_view pointer)
  {
    schema_test_storage storage;
    pod_vector<char> token{storage.callbacks()};
    std::string joined;
    std::string_view rest = pointer;
    while (!rest.empty())
    {
      if (!next_pointer_token(rest, token).has_value())
      {
        joined = "<error>";
        break;
      }
      joined += std::string{text_of(token, 0U, token.size())} + "|";
    }
    static_cast<void>(token.release());
    return joined;
  }

  /*
   * Arrange: Pointer fragments with ~0, ~1, percent escapes, empty tokens and malformed escapes.
   * Act: Decode their tokens.
   * Expect: Percent escapes first, then ~1 and ~0 (RFC 6901 sections 4 and 6); malformed
   *         escapes are unresolved references.
   */
  TEST(P044_JsonPointer, DecodesTokens)
  {
    EXPECT_EQ(tokens("/definitions/a~1b"), "definitions|a/b|");
    EXPECT_EQ(tokens("/c~0d"), "c~d|");
    EXPECT_EQ(tokens("/~01"), "~1|");
    EXPECT_EQ(tokens("/e%25f"), "e%f|");
    EXPECT_EQ(tokens("/%7e1"), "/|");
    EXPECT_EQ(tokens("//"), "||");
    EXPECT_EQ(tokens("/a~2"), "<error>");
    EXPECT_EQ(tokens("/a~"), "<error>");
    EXPECT_EQ(tokens("/a%2"), "<error>");
    EXPECT_EQ(tokens("/a%zz"), "<error>");
    EXPECT_EQ(tokens("a"), "<error>");
  }

  /*
   * Arrange: Index tokens.
   * Act: Parse them.
   * Expect: Decimal indexes without leading zeros only.
   */
  TEST(P044_JsonPointer, ParsesArrayIndexes)
  {
    EXPECT_EQ(pointer_index("0"), std::optional<std::size_t>{0U});
    EXPECT_EQ(pointer_index("12"), std::optional<std::size_t>{12U});
    EXPECT_FALSE(pointer_index("01").has_value());
    EXPECT_FALSE(pointer_index("").has_value());
    EXPECT_FALSE(pointer_index("-").has_value());
    EXPECT_FALSE(pointer_index("99999999999999999999999").has_value());
  }

  /*
   * Arrange: Tokens containing '~' and '/'.
   * Act: Escape them into a large and a too-small buffer.
   * Expect: RFC 6901 escaping; the needed size is reported and nothing past capacity is written.
   */
  TEST(P044_JsonPointer, EscapesTokens)
  {
    char buffer[16] = {};
    EXPECT_EQ(escape_pointer_token("a/b~c", buffer, sizeof(buffer)), 7U);
    EXPECT_EQ(std::string(buffer, 7U), "a~1b~0c");
    char small[3] = {'x', 'x', 'x'};
    EXPECT_EQ(escape_pointer_token("~~", small, 2U), 4U);
    EXPECT_EQ(std::string(small, 3U), "~0x");
  }
} // namespace
