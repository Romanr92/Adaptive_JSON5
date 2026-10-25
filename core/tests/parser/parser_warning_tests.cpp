#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "parser/parser_engine.hpp"

#include <gtest/gtest.h>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_017 ==================================== */
  /*
   * Arrange: The first raw separator appears in a scalar; later ones occur in a name and its value.
   * Act: Parse with one slot fewer than the warnings and with exactly one slot per warning.
   * Expect: Too few slots rejects; exact slots retain all three warnings in source order.
   */
  TEST(J5_017_ContainerWarnings, EachWarningConsumesDiagnosticSlot)
  {
    /* Arrange */
    const std::string input = u8"[\"\u2028\",{\"\u2029\":\"\u2028\"}]";
    /* Act */
    for (const std::size_t diagnostics : {2u, 3u})
    {
      document_provider provider;
      parse_options options;
      options.limits.max_diagnostics = diagnostics;
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run();
      /* Expect */
      // Mixing a scalar, a name and a value checks one shared diagnostic count across both lexers.
      ASSERT_EQ(parsed.has_value(), diagnostics == 3);
      if (diagnostics != 3)
      {
        EXPECT_EQ(parsed.error_if()->code, error_code::resource_limit);
      }
      else
      {
        const auto& warnings = parsed.value_if()->warnings;
        ASSERT_EQ(warnings.size(), 3u);
        constexpr std::size_t c_offsets[] = {2u, 9u, 15u};
        for (std::size_t index = 0; index < warnings.size(); ++index)
        {
          const auto warning = warnings.at(index);
          ASSERT_TRUE(warning.has_value());
          EXPECT_EQ(warning.value_if()->byte_offset, c_offsets[index]);
          EXPECT_EQ(warning.value_if()->byte_length, 3u);
        }
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
