#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "lexer/scalar_lexer.hpp"
#include "lexer/string/string_lexer.hpp"
#include "unicode/unicode.hpp"

#include <gtest/gtest.h>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_004 ==================================== */
  /*
   * Arrange: A null pointer with a nonzero advertised input length.
   * Act: Inspect and consume through every cursor read operation.
   * Expect: Reads report absence and do not advance despite remaining byte metadata.
   */
  TEST(J5_004_CursorBoundary, NullBufferNeverProducesReadableBytes)
  {
    /* Arrange */
    cursor source{{nullptr, 3}};
    /* Act */
    // A cursor is a bounded view; malformed pointer/length pairs must not trigger dereferences.
    const auto current = source.peek();
    const auto ahead = source.peek(1);
    const auto consumed = source.next();
    /* Expect */
    EXPECT_FALSE(current.has_value());
    EXPECT_FALSE(ahead.has_value());
    EXPECT_FALSE(consumed.has_value());
    EXPECT_EQ(source.span_from(0).data, nullptr);
    EXPECT_EQ(source.offset(), 0u);
    EXPECT_EQ(source.remaining(), 3u);
    const cursor nonempty{{"x", 1}};
    EXPECT_EQ(nonempty.span_from(0).size, 0u);
    EXPECT_EQ(nonempty.span_from(2).data, nullptr);
  }
  /* ================================= End Test_J5_004 ================================== */

  /* =================================== Test_J5_006 ==================================== */
  /*
   * Arrange: Surrogates and out-of-Unicode values passed directly to property queries.
   * Act: Query each independent property table at runtime.
   * Expect: Non-scalars never acquire whitespace or identifier membership.
   */
  TEST(J5_006_UnicodeProperties, RejectsNonScalarsInEveryPropertyTable)
  {
    /* Arrange */
    const char32_t values[] = {0xD800, 0xDFFF, 0x110000, 0xFFFFFFFF};
    /* Act */
    for (const auto value : values)
    {
      // Calling table queries directly verifies their guard independently of UTF-8 decoding.
      const bool space = is_unicode_space_separator(value);
      const bool start = is_identifier_start(value);
      const bool part = is_identifier_part(value);
      const bool legacy_space = detail::contains_unicode(value, detail::c_unicode_3_space_separator_ranges);
      /* Expect */
      EXPECT_FALSE(space);
      EXPECT_FALSE(start);
      EXPECT_FALSE(part);
      EXPECT_FALSE(legacy_space);
    }
  }
  /* ================================= End Test_J5_006 ================================== */

  /* =================================== Test_J5_017 ==================================== */
  /*
   * Arrange: A previously validated body containing a raw separator and its closing quote.
   * Act: Replay warnings with an absent observer through the internal helper.
   * Expect: Replay succeeds and preserves the caller's cursor because traversal uses a copy.
   */
  TEST(J5_017_WarningReplay, AbsentObserverPreservesValidatedInputCursor)
  {
    /* Arrange */
    document_provider provider;
    const std::string input = u8"\"\u2028\"";
    cursor validation{{input.data(), input.size()}};
    const auto token = consume_string(validation, syntax_mode::json5, provider.callbacks(), 8);
    ASSERT_TRUE(token.has_value());
    ASSERT_TRUE(token.value_if()->has_value());
    ASSERT_EQ((**token.value_if()).warning_count, 1u);
    cursor body{{input.data(), input.size()}};
    body.advance(1);
    const auto attempts = provider.m_attempts;
    /* Act */
    // Reuse a genuinely validated body; injecting malformed bytes here would violate the replay precondition.
    const auto replayed = detail::notify_string_warnings(body, string_quote::double_quote, syntax_mode::json5, {});
    /* Expect */
    ASSERT_TRUE(replayed.has_value());
    EXPECT_EQ(body.offset(), 1u);
    EXPECT_EQ(provider.m_attempts, attempts);
  }
  /* ================================= End Test_J5_017 ================================== */

  /* =================================== Test_J5_046 ==================================== */
  /*
   * Arrange: Escaped BMP and supplementary characters needing decoded storage.
   * Act: Fail each allocation in turn, then retry with the same input cursor.
   * Expect: No partial token is published; retry decodes the exact UTF-16 length.
   */
  TEST(J5_046_StringEscapeStorage, FailsEachEscapedUnitAllocationAtomically)
  {
    /* Arrange */
    for (const std::string input : {std::string{"\"\\u0041\\u0042\""}, std::string{u8"\"\\\U0001F600\""}})
    {
      SCOPED_TRACE(input);
      document_provider baseline;
      cursor complete{{input.data(), input.size()}};
      const auto token = consume_string(complete, syntax_mode::json5, baseline.callbacks(), 8);
      ASSERT_TRUE(token.has_value());
      ASSERT_TRUE(token.value_if()->has_value());
      ASSERT_EQ((**token.value_if()).value.size(), 2u);
      for (std::size_t failure = 1; failure <= baseline.m_attempts; ++failure)
      {
        document_provider provider;
        provider.m_fail_on = failure;
        cursor source{{input.data(), input.size()}};
        /* Act */
        // A supplementary non-escape contributes two units; failure on the second must roll back both.
        const auto failed = consume_string(source, syntax_mode::json5, provider.callbacks(), 8);
        /* Expect */
        ASSERT_FALSE(failed.has_value());
        EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
        EXPECT_EQ(source.offset(), 0u);
        EXPECT_EQ(provider.m_live_bytes, 0u);
        provider.m_fail_on = 0;
        const auto retry = consume_string(source, syntax_mode::json5, provider.callbacks(), 8);
        ASSERT_TRUE(retry.has_value());
        ASSERT_TRUE(retry.value_if()->has_value());
        EXPECT_EQ((**retry.value_if()).value.size(), 2u);
        EXPECT_EQ(source.offset(), input.size());
      }
    }
  }
  /* ================================= End Test_J5_046 ================================== */
} // namespace
