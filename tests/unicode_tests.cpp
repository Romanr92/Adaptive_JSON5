// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/unicode.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace
{
  using adaptive_json5::unicode_range;

  /* =================================== Test_J5_006 =================================== */

  constexpr unicode_range c_lookup_ranges[] = {{10, 12}, {20, 22}, {30, 32}, {40, 42}, {50, 52}};
  static_assert(noexcept(adaptive_json5::detail::contains_unicode(0, c_lookup_ranges)));
  static_assert(noexcept(adaptive_json5::is_unicode_space_separator(0)));
  static_assert(noexcept(adaptive_json5::is_identifier_start(0)));
  static_assert(noexcept(adaptive_json5::is_identifier_part(0)));

  /**
   * @brief Independent expected memberships for a small binary-search input.
   * @par Requirements
   * P006; J5-006.
   */
  struct lookup_case
  {
      const char* name; // Case description.
      char32_t scalar;  // Input scalar.
      bool expected;    // Expected interval membership.
  };

  using J5_006_UnicodeLookup = testing::TestWithParam<lookup_case>;

  /* Exercise both search directions, inclusive endpoints and missed ranges.
   * Arrange: Five separated intervals and an independent expected result.
   * Act: Call the production binary search.
   * Expect: Exact membership after any required sequence of search reductions.
   */
  TEST_P(J5_006_UnicodeLookup, SearchesFullInterval)
  {
    /* Arrange */
    const auto& sample = GetParam();
    /* Act */
    const bool actual = adaptive_json5::detail::contains_unicode(sample.scalar, c_lookup_ranges);
    /* Expect */
    EXPECT_EQ(actual, sample.expected);
  }

  INSTANTIATE_TEST_SUITE_P(SearchBoundaries, J5_006_UnicodeLookup,
                           testing::Values(lookup_case{"BelowAll", 0x9, false}, lookup_case{"FirstStart", 0xA, true},
                                           lookup_case{"FirstEnd", 0xC, true}, lookup_case{"FirstGap", 0xD, false},
                                           lookup_case{"SecondStart", 0x14, true}, lookup_case{"SecondEnd", 0x16, true},
                                           lookup_case{"SecondGap", 0x1D, false},
                                           lookup_case{"MiddleStart", 0x1E, true}, lookup_case{"MiddleEnd", 0x20, true},
                                           lookup_case{"MiddleGap", 0x21, false},
                                           lookup_case{"FourthStart", 0x28, true}, lookup_case{"FourthEnd", 0x2A, true},
                                           lookup_case{"FinalGap", 0x31, false}, lookup_case{"LastStart", 0x32, true},
                                           lookup_case{"LastEnd", 0x34, true}, lookup_case{"AboveAll", 0x35, false},
                                           lookup_case{"Surrogate", 0xD800, false},
                                           lookup_case{"BeyondUnicode", 0x110000, false}),
                           [](const testing::TestParamInfo<lookup_case>& info)
                           {
                             return info.param.name;
                           });

  /* ================================= End Test_J5_006 ================================= */

  /* =================================== Test_J5_019 =================================== */

  /**
   * @brief Independent classifier expectations from category and ES5 addition fixtures.
   * @par Requirements
   * P006; J5-006; J5-019; J5-052.
   */
  struct classifier_case
  {
      const char* name; // Case description.
      char32_t scalar;  // Input scalar or invalid code point.
      bool start;       // Expected identifier-start acceptance.
      bool part;        // Expected identifier-continuation acceptance.
      bool space;       // Expected modern Zs membership.
  };

  using J5_019_UnicodeClassifier = testing::TestWithParam<classifier_case>;

  /* Check actual classification, including additions absent from category tables.
   * Arrange: Category, historical, supplementary or invalid-scalar fixture.
   * Act: Call each public classifier.
   * Expect: Independent start/continuation/space results without grammar conflation.
   */
  TEST_P(J5_019_UnicodeClassifier, ClassifiesDecodedScalar)
  {
    /* Arrange */
    const auto& sample = GetParam();
    /* Act */
    const bool start = adaptive_json5::is_identifier_start(sample.scalar);
    const bool part = adaptive_json5::is_identifier_part(sample.scalar);
    const bool space = adaptive_json5::is_unicode_space_separator(sample.scalar);
    /* Expect */
    EXPECT_EQ(start, sample.start);
    EXPECT_EQ(part, sample.part);
    EXPECT_EQ(space, sample.space);
  }

  INSTANTIATE_TEST_SUITE_P(
    ScalarFixtures, J5_019_UnicodeClassifier,
    testing::Values(
      classifier_case{"Uppercase", 0x41, true, true, false}, classifier_case{"Lowercase", 0x7A, true, true, false},
      classifier_case{"Digit", 0x30, false, true, false}, classifier_case{"Dollar", 0x24, true, true, false},
      classifier_case{"Underscore", 0x5F, true, true, false}, classifier_case{"OtherLetter", 0xAA, true, true, false},
      classifier_case{"Titlecase", 0x1C5, true, true, false},
      classifier_case{"ModifierLetter", 0x2B0, true, true, false}, classifier_case{"Hebrew", 0x5D0, true, true, false},
      classifier_case{"LetterNumber", 0x2160, true, true, false},
      classifier_case{"NonspacingMark", 0x301, false, true, false},
      classifier_case{"SpacingMark", 0x903, false, true, false},
      classifier_case{"Connector", 0x203F, false, true, false},
      classifier_case{"HistoricalLetter", 0x1885, true, true, false},
      classifier_case{"HistoricalConnector", 0x30FB, false, true, false},
      classifier_case{"SupplementaryLetter", 0x10400, true, true, false},
      classifier_case{"Zwnj", 0x200C, false, true, false}, classifier_case{"Zwj", 0x200D, false, true, false},
      classifier_case{"HistoricalSpace", 0x200B, false, false, false},
      classifier_case{"NoBreakSpace", 0xA0, false, false, true}, classifier_case{"Space", 0x20, false, false, true},
      classifier_case{"LineSeparator", 0x2028, false, false, false},
      classifier_case{"Bom", 0xFEFF, false, false, false}, classifier_case{"Nul", 0x0, false, false, false},
      classifier_case{"Emoji", 0x1F600, false, false, false},
      classifier_case{"OtherIdStart", 0x2118, false, false, false},
      classifier_case{"Surrogate", 0xD800, false, false, false},
      classifier_case{"OutOfRange", 0x110000, false, false, false}),
    [](const testing::TestParamInfo<classifier_case>& info)
    {
      return info.param.name;
    });

  /* ================================= End Test_J5_019 ================================= */
} // namespace
