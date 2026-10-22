#include "../../../tests/support/document_provider.hpp"
#include "adaptive_json5/detail/parser/parser_engine.hpp"

#include <gtest/gtest.h>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_017 ==================================== */
  /*
   * Arrange: The first raw separator appears in a scalar; later ones occur in a name/value.
   * Act: Parse with no diagnostic slot and with exactly one retained-descriptor slot.
   * Expect: Zero slots rejects; one slot retains the first location and counts all warnings.
   */
  TEST(J5_017_ContainerWarnings, ScalarWarningOwnsFirstDiagnosticSlot)
  {
    /* Arrange */
    const std::string input = u8"[\"\u2028\",{\"\u2029\":\"\u2028\"}]";
    /* Act */
    for (const std::size_t diagnostics : {0u, 1u})
    {
      document_provider provider;
      parse_options options;
      options.limits.max_diagnostics = diagnostics;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      /* Expect */
      // Starting with a scalar verifies first-warning ownership independently of object-name decoding.
      ASSERT_EQ(parsed.has_value(), diagnostics != 0);
      if (diagnostics == 0)
      {
        EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
      }
      else
      {
        EXPECT_EQ(parsed.value_if()->warning_count, 3u);
        ASSERT_TRUE(parsed.value_if()->first_warning.has_value());
        EXPECT_EQ(parsed.value_if()->first_warning->byte_offset, 2u);
        EXPECT_EQ(parsed.value_if()->first_warning->byte_length, 3u);
      }
    }
  }
  /* ================================= End Test_J5_017 ================================== */

  /* =================================== Test_J5_009 ==================================== */
  /*
   * Arrange: Malformed UTF-8 in leading and nested trivia, including comments.
   * Act: Run a fresh container engine on each buffer.
   * Expect: Encoding errors preserve original offsets and publish no partial tree.
   */
  TEST(J5_009_ContainerTrivia, RejectsMalformedEncodingBeforeDispatch)
  {
    /* Arrange */
    for (const std::string prefix : {std::string{}, std::string{"[ "}, std::string{"[/*"}})
    {
      document_provider provider;
      parse_options options;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      const std::string input = prefix + static_cast<char>(0xFF);
      /* Act */
      // Trivia is discarded semantically, but its bytes must still be validated before another parser action.
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      /* Expect */
      ASSERT_FALSE(parsed.has_value());
      EXPECT_EQ(parsed.error_if()->code, error_code::encoding);
      EXPECT_TRUE(parsed.error_if()->has_byte_offset);
      EXPECT_EQ(parsed.error_if()->byte_offset, prefix.size());
    }
  }
  /* ================================= End Test_J5_009 ================================== */
} // namespace
