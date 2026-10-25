#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/text_position.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* =================================== Test_J5_044 =================================== */
  static_assert(noexcept(locate(std::declval<input_view>(), std::size_t{})));
  static_assert(std::is_trivially_copyable_v<text_position>);
  static_assert((text_position{}.line == 1U) && (text_position{}.column == 1U));

  /**
   * @brief Describe one byte offset and the position it must map to.
   * @par Requirements
   * P039; J5-044.
   */
  struct position_case
  {
      const char* name;
      std::string_view input; // Input original bytes.
      std::size_t offset;     // Input zero-based byte offset.
      std::size_t line;       // Expected 1-based line.
      std::size_t column;     // Expected 1-based column in code points.
  };

  std::ostream& operator<<(std::ostream& stream, const position_case& value)
  {
    return stream << value.name;
  }

  class TextPositionUnits : public ::testing::TestWithParam<position_case>
  {
  };

  /* Verify byte offsets map to 1-based lines and code-point columns for every JSON5 line terminator.
   * Arrange: Select the case bytes and offset.
   * Act: Locate the offset.
   * Expect: The case line and column.
   */
  TEST_P(TextPositionUnits, MapsOffsetToLineAndColumn)
  {
    /* Arrange */
    const position_case& param = GetParam();
    /* Act */
    const auto position = locate(input_view{param.input.data(), param.input.size()}, param.offset);
    /* Expect */
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(position.value_if()->line, param.line);
    EXPECT_EQ(position.value_if()->column, param.column);
  }

  constexpr std::string_view c_mixed{"a\r\nb\xF0\x9F\x98\x80"
                                     "c"};

  INSTANTIATE_TEST_SUITE_P(J5_044_TextPosition, TextPositionUnits,
                           ::testing::Values(position_case{"Start", c_mixed, 0U, 1U, 1U},
                                             position_case{"AfterFirstByte", c_mixed, 1U, 1U, 2U},
                                             position_case{"InsideCrLfReportsCr", c_mixed, 2U, 1U, 2U},
                                             position_case{"AfterCrLf", c_mixed, 3U, 2U, 1U},
                                             position_case{"FourByteCharacter", c_mixed, 4U, 2U, 2U},
                                             position_case{"InsideFourByteCharacter", c_mixed, 6U, 2U, 2U},
                                             position_case{"AfterFourByteCharacter", c_mixed, 8U, 2U, 3U},
                                             position_case{"EndOfInput", c_mixed, 9U, 2U, 4U},
                                             position_case{"EmptyInputEnd", "", 0U, 1U, 1U},
                                             position_case{"LineFeed", "a\nb", 2U, 2U, 1U},
                                             position_case{"LoneCarriageReturn", "a\rb", 2U, 2U, 1U},
                                             position_case{"TrailingCarriageReturn", "a\r", 2U, 2U, 1U},
                                             position_case{"TwoCarriageReturns", "\r\rx", 2U, 3U, 1U},
                                             position_case{"LineFeedAfterCrLf", "\r\n\nx", 3U, 3U, 1U},
                                             position_case{"LineSeparator",
                                                           "a\xE2\x80\xA8"
                                                           "b",
                                                           4U, 2U, 1U},
                                             position_case{"ParagraphSeparator",
                                                           "a\xE2\x80\xA9"
                                                           "b",
                                                           4U, 2U, 1U},
                                             position_case{"TwoByteCharacter", "\xC3\xA9x", 2U, 1U, 2U},
                                             position_case{"ThreeByteCharacter",
                                                           "\xE2\x82\xAC"
                                                           "x",
                                                           3U, 1U, 2U},
                                             position_case{"InvalidByteIsOneColumn", "\xFFx", 1U, 1U, 2U},
                                             position_case{"TruncatedSequenceBytesAreColumns", "\xE2\x80x", 2U, 1U, 3U},
                                             position_case{"OverlongSequenceBytesAreColumns", "\xC0\x80x", 2U, 1U, 3U},
                                             position_case{"TabIsOneColumn", "\tx", 1U, 1U, 2U}),
                           [](const ::testing::TestParamInfo<position_case>& info)
                           {
                             return info.param.name;
                           });

  /* ----------------------------------------------------------------------------------- */

  /* Verify offsets beyond the end are rejected while the end itself is valid.
   * Arrange: Three bytes of input.
   * Act: Locate offset 3 (end) and offset 4 (past end).
   * Expect: 1:4 for the end; unlocated wrong_type_or_access past it.
   */
  TEST(J5_044_TextPosition, RejectsOffsetPastEnd)
  {
    /* Arrange */
    constexpr std::string_view c_input{"abc"};
    const input_view input{c_input.data(), c_input.size()};
    /* Act */
    const auto end = locate(input, 3U);
    const auto past = locate(input, 4U);
    /* Expect */
    ASSERT_TRUE(end.has_value());
    EXPECT_EQ(end.value_if()->line, 1U);
    EXPECT_EQ(end.value_if()->column, 4U);
    ASSERT_FALSE(past.has_value());
    EXPECT_EQ(past.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_FALSE(past.error_if()->has_byte_offset);
  }
  /* ================================= End Test_J5_044 ================================= */
} // namespace
