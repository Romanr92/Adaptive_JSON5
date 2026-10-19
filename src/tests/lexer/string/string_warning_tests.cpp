// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/detail/lexer/string/line_continuation.hpp"
#include "adaptive_json5/detail/lexer/string/string_warning.hpp"
#include "adaptive_json5/detail/lexer/string/unicode_escape.hpp"
#include "adaptive_json5/detail/memory/decoded_string_builder.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Supply host-only storage for checking retained separator content.
   * @par Requirements
   * P020; J5-017.
   */
  void* allocate_test_storage(void*, std::size_t bytes, std::size_t alignment) noexcept
  {
    return alignment <= alignof(std::max_align_t) ? std::malloc(bytes) : nullptr;
  }

  /**
   * @brief Release storage obtained from the test provider.
   * @par Requirements
   * P020; J5-017.
   */
  void release_test_storage(void*, void* address, std::size_t, std::size_t) noexcept
  {
    std::free(address);
  }

  constexpr storage_callbacks c_test_storage{nullptr, &allocate_test_storage, &release_test_storage};

  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_017 =================================== */

  static_assert(std::is_trivially_copyable_v<string_warning>);
  static_assert(noexcept(raw_separator_warning_for(std::declval<const string_piece&>(), syntax_mode::json5)));

  /**
   * @brief Independent raw and escaped separator spellings and expected advisories.
   * @par Requirements
   * P020; J5-017.
   */
  struct separator_case
  {
      const char* name;         // Diagnostic parameter name.
      std::string raw;          // Input original UTF-8 separator bytes.
      std::string escaped;      // Input Unicode escape suffix after backslash.
      char32_t scalar;          // Expected decoded scalar.
      string_warning_code code; // Expected JSON5 warning category.
      unsigned char final_byte; // Expected final UTF-8 byte; first two are E2 80.
  };
  class J5_017_StringWarning : public ::testing::TestWithParam<separator_case>
  {
  };

  /* Verify raw separators stay valid content and produce mode-specific advisories.
   * Arrange: Place each raw separator after a multibyte prefix inside a string.
   * Act: Read it, inspect its warning twice, then store and export the scalar.
   * Expect: JSON5 warning at original byte offset five; strict mode no advisory.
   * The piece, cursor and exact separator content remain unchanged by inspection.
   */
  TEST_P(J5_017_StringWarning, ReportsOriginalSpanWithoutChangingContent)
  {
    /* Arrange */
    const auto& param = GetParam();
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (const char quote : {'"', '\''})
      {
        if (mode == syntax_mode::strict_json && quote == '\'')
        {
          continue;
        }
        const std::string input = std::string("X") + quote + "\xE2\x82\xAC" + param.raw + quote;
        const auto before = input;
        cursor source({input.data(), input.size()});
        source.advance(1);
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        ASSERT_TRUE(opening.value_if()->has_value());
        ASSERT_TRUE(consume_string_piece(source, **opening.value_if(), mode).has_value());
        const auto piece = consume_string_piece(source, **opening.value_if(), mode);
        ASSERT_TRUE(piece.has_value());
        const auto offset = source.offset();
        decoded_string_builder builder(c_test_storage, 1);
        /* Act */
        const auto warning = raw_separator_warning_for(*piece.value_if(), mode);
        const auto repeated = raw_separator_warning_for(*piece.value_if(), mode);
        ASSERT_TRUE(builder.append_scalar(piece.value_if()->scalar).has_value());
        auto text = builder.finish();
        std::array<unsigned char, 4> output{0xA5, 0xA5, 0xA5, 0xA5};
        const auto exported = export_utf8(text, output.data(), output.size());
        /* Expect */
        ASSERT_EQ(warning.has_value(), mode == syntax_mode::json5);
        ASSERT_EQ(repeated.has_value(), warning.has_value());
        if (warning.has_value())
        {
          EXPECT_EQ(warning->code, param.code);
          EXPECT_EQ(warning->byte_offset, 5u);
          EXPECT_EQ(warning->byte_length, 3u);
          EXPECT_EQ(repeated->code, warning->code);
          EXPECT_EQ(repeated->byte_offset, warning->byte_offset);
          EXPECT_EQ(repeated->byte_length, warning->byte_length);
        }
        EXPECT_EQ(piece.value_if()->kind, string_piece_kind::character);
        EXPECT_EQ(piece.value_if()->scalar, param.scalar);
        EXPECT_EQ(piece.value_if()->byte_offset, 5u);
        EXPECT_EQ(piece.value_if()->byte_length, 3u);
        EXPECT_EQ(source.offset(), offset);
        EXPECT_EQ(input, before);
        ASSERT_TRUE(exported.has_value());
        EXPECT_EQ(*exported.value_if(), 3u);
        EXPECT_EQ(output, (std::array<unsigned char, 4>{0xE2, 0x80, param.final_byte, 0xA5}));
        const auto closing = consume_string_piece(source, **opening.value_if(), mode);
        ASSERT_TRUE(closing.has_value());
        EXPECT_EQ(closing.value_if()->kind, string_piece_kind::closing_quote);
        EXPECT_TRUE(source.is_at_end());
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify escaped equivalents and continuations do not become raw-character pieces.
   * Arrange: Open strings containing a Unicode escape or a continued separator.
   * Act: Inspect the actual backslash handoff piece and run its owning decoder.
   * Expect: No raw warning for the handoff, correct escaped unit/continuation span,
   * and a successfully recognized following closing quote.
   */
  TEST_P(J5_017_StringWarning, DoesNotWarnOnEscapedOrContinuedSeparators)
  {
    /* Arrange */
    const auto& param = GetParam();
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      const std::string input = std::string("\"\\") + param.escaped + "\"";
      cursor source({input.data(), input.size()});
      const auto opening = consume_string_opening(source, mode);
      ASSERT_TRUE(opening.has_value());
      ASSERT_TRUE(opening.value_if()->has_value());
      const auto handoff = consume_string_piece(source, **opening.value_if(), mode);
      ASSERT_TRUE(handoff.has_value());
      /* Act */
      const auto warning = raw_separator_warning_for(*handoff.value_if(), mode);
      const auto decoded = consume_unicode_escape(source);
      const auto closing = consume_string_piece(source, **opening.value_if(), mode);
      /* Expect */
      EXPECT_EQ(handoff.value_if()->kind, string_piece_kind::escape_start);
      EXPECT_FALSE(warning.has_value());
      ASSERT_TRUE(decoded.has_value());
      ASSERT_TRUE(decoded.value_if()->has_value());
      EXPECT_EQ(**decoded.value_if(), static_cast<char16_t>(param.scalar));
      ASSERT_TRUE(closing.has_value());
      EXPECT_EQ(closing.value_if()->kind, string_piece_kind::closing_quote);
      EXPECT_TRUE(source.is_at_end());
    }
    const std::string continued = std::string("\"\\") + param.raw + "\"";
    cursor source({continued.data(), continued.size()});
    const auto opening = consume_string_opening(source, syntax_mode::json5);
    ASSERT_TRUE(opening.has_value());
    ASSERT_TRUE(opening.value_if()->has_value());
    const auto handoff = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    ASSERT_TRUE(handoff.has_value());
    EXPECT_FALSE(raw_separator_warning_for(*handoff.value_if(), syntax_mode::json5).has_value());
    const auto continuation = consume_line_continuation(source, syntax_mode::json5);
    ASSERT_TRUE(continuation.has_value());
    ASSERT_TRUE(continuation.value_if()->has_value());
    EXPECT_EQ((**continuation.value_if()).byte_offset, 2u);
    EXPECT_EQ((**continuation.value_if()).byte_length, 3u);
    const auto closing = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    ASSERT_TRUE(closing.has_value());
    EXPECT_EQ(closing.value_if()->kind, string_piece_kind::closing_quote);
  }
  INSTANTIATE_TEST_SUITE_P(Separators, J5_017_StringWarning,
                           ::testing::Values(separator_case{"Line", "\xE2\x80\xA8", "u2028", 0x2028,
                                                            string_warning_code::raw_line_separator, 0xA8},
                                             separator_case{"Paragraph", "\xE2\x80\xA9", "u2029", 0x2029,
                                                            string_warning_code::raw_paragraph_separator, 0xA9}),
                           [](const ::testing::TestParamInfo<separator_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify ordinary Unicode source and syntactic quote/backslash pieces do not warn.
   * Arrange: Read ASCII bytes and neighboring/non-BMP scalars through the real reader.
   * Act: Inspect every successfully read source piece in both modes.
   * Expect: No separator warning, regardless of source piece kind or scalar width.
   */
  TEST(J5_017_StringWarningBoundary, IgnoresOtherCharactersAndSyntaxPieces)
  {
    /* Arrange */
    for (const auto mode : {syntax_mode::json5, syntax_mode::strict_json})
    {
      for (unsigned int byte = 0; byte < 128; ++byte)
      {
        if (byte == 0x0A || byte == 0x0D || (mode == syntax_mode::strict_json && byte < 0x20))
        {
          continue;
        }
        const std::array<char, 3> input{'"', static_cast<char>(byte), '"'};
        cursor source({input.data(), input.size()});
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        ASSERT_TRUE(opening.value_if()->has_value());
        const auto piece = consume_string_piece(source, **opening.value_if(), mode);
        ASSERT_TRUE(piece.has_value());
        /* Act */
        const auto warning = raw_separator_warning_for(*piece.value_if(), mode);
        /* Expect */
        EXPECT_FALSE(warning.has_value()) << byte;
      }
      for (const std::string bytes :
           {std::string("\xE2\x80\xA7"), std::string("\xE2\x80\xAA"), std::string("\xF0\x9F\x8E\xBC")})
      {
        const std::string input = std::string("\"") + bytes + "\"";
        cursor source({input.data(), input.size()});
        const auto opening = consume_string_opening(source, mode);
        ASSERT_TRUE(opening.has_value());
        ASSERT_TRUE(opening.value_if()->has_value());
        const auto piece = consume_string_piece(source, **opening.value_if(), mode);
        ASSERT_TRUE(piece.has_value());
        EXPECT_FALSE(raw_separator_warning_for(*piece.value_if(), mode).has_value());
      }
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify adjacent separators produce independent descriptors at distinct byte offsets.
   * Arrange: Place both raw separators consecutively inside a JSON5 string.
   * Act: Read each piece and inspect its warning.
   * Expect: Two different codes and exact one/three-byte-spaced original offsets.
   */
  TEST(J5_017_StringWarningBoundary, DescribesEachAdjacentSeparator)
  {
    /* Arrange */
    const std::string input = "\"\xE2\x80\xA8\xE2\x80\xA9\"";
    cursor source({input.data(), input.size()});
    const auto opening = consume_string_opening(source, syntax_mode::json5);
    ASSERT_TRUE(opening.has_value());
    ASSERT_TRUE(opening.value_if()->has_value());
    /* Act */
    const auto first = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    const auto second = consume_string_piece(source, **opening.value_if(), syntax_mode::json5);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    const auto first_warning = raw_separator_warning_for(*first.value_if(), syntax_mode::json5);
    const auto second_warning = raw_separator_warning_for(*second.value_if(), syntax_mode::json5);
    /* Expect */
    ASSERT_TRUE(first_warning.has_value());
    ASSERT_TRUE(second_warning.has_value());
    EXPECT_EQ(first_warning->code, string_warning_code::raw_line_separator);
    EXPECT_EQ(first_warning->byte_offset, 1u);
    EXPECT_EQ(first_warning->byte_length, 3u);
    EXPECT_EQ(second_warning->code, string_warning_code::raw_paragraph_separator);
    EXPECT_EQ(second_warning->byte_offset, 4u);
    EXPECT_EQ(second_warning->byte_length, 3u);
    EXPECT_EQ(source.offset(), 7u);
  }

  /* ================================= End Test_J5_017 ================================= */
} // namespace
