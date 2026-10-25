#include "adaptive_json5/detail/unicode/unicode_tables.hpp"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <iterator>

namespace
{
  using adaptive_json5::unicode_range;
  namespace tables = adaptive_json5::detail;

  /* =================================== Test_J5_006 =================================== */

  /**
   * @brief Independently scan table membership without using the production lookup.
   * @par Requirements
   * P006; J5-006.
   */
  template <std::size_t Count>
  bool table_contains(const unicode_range (&ranges)[Count], char32_t scalar)
  {
    return std::any_of(std::begin(ranges), std::end(ranges),
                       [scalar](const unicode_range& range)
                       {
                         return range.first <= scalar && scalar <= range.last;
                       });
  }

  /**
   * @brief Check the sorted scalar intervals required by binary-search consumers.
   * @par Requirements
   * P006; J5-006.
   */
  template <std::size_t Count>
  void expect_valid_ranges(const unicode_range (&ranges)[Count])
  {
    for (std::size_t index = 0; index < Count; ++index)
    {
      SCOPED_TRACE(index);
      EXPECT_LE(ranges[index].first, ranges[index].last);
      EXPECT_LE(ranges[index].last, static_cast<char32_t>(0x10FFFF));
      EXPECT_TRUE(ranges[index].last < 0xD800 || ranges[index].first > 0xDFFF);
      if (index != 0)
      {
        EXPECT_GT(ranges[index].first, ranges[index - 1].last + 1);
      }
    }
  }

  /**
   * @brief Independent category fixtures, including historical category changes.
   * @par Requirements
   * P006; J5-006.
   */
  struct membership_case
  {
      const char* name;  // Fixture description.
      char32_t scalar;   // Input code point.
      bool start;        // Expected letter-category membership.
      bool extra;        // Expected additional continuation-category membership.
      bool space;        // Expected Unicode 17.0.0 Zs membership.
      bool legacy_space; // Expected Unicode 3.0.0 Zs membership.
  };

  using J5_006_UnicodeTableMembership = testing::TestWithParam<membership_case>;

  /* Preserve source-category membership with documented version distinctions.
   * Arrange: An independently selected category/boundary fixture.
   * Act: Scan each table for the scalar.
   * Expect: Exact property membership, including baseline retention.
   */
  TEST_P(J5_006_UnicodeTableMembership, MatchesDocumentedCategories)
  {
    /* Arrange */
    const auto& sample = GetParam();
    /* Act */
    const bool start = table_contains(tables::c_identifier_start_ranges, sample.scalar);
    const bool extra = table_contains(tables::c_identifier_extra_ranges, sample.scalar);
    const bool space = table_contains(tables::c_space_separator_ranges, sample.scalar);
    const bool legacy = table_contains(tables::c_unicode_3_space_separator_ranges, sample.scalar);
    /* Expect */
    EXPECT_EQ(start, sample.start);
    EXPECT_EQ(extra, sample.extra);
    EXPECT_EQ(space, sample.space);
    EXPECT_EQ(legacy, sample.legacy_space);
  }

  INSTANTIATE_TEST_SUITE_P(
    CategoryFixtures, J5_006_UnicodeTableMembership,
    testing::Values(membership_case{"AsciiLetter", U'A', true, false, false, false},
                    membership_case{"DecimalDigit", U'0', false, true, false, false},
                    membership_case{"CombiningMark", 0x0301, false, true, false, false},
                    membership_case{"LetterNumber", 0x2160, true, false, false, false},
                    // U+1885 changed from Lo to Mn; retain its Unicode 3.0 start membership.
                    membership_case{"HistoricalMongolianLetter", 0x1885, true, true, false, false},
                    // U+30FB changed from Pc to Po; retain its Unicode 3.0 continuation membership.
                    membership_case{"HistoricalConnector", 0x30FB, false, true, false, false},
                    membership_case{"SupplementaryDeseretLetter", 0x10400, true, false, false, false},
                    // U+2118 is Other_ID_Start, but does not belong to an ES5 letter category.
                    membership_case{"OtherIdStartIsNotLetterCategory", 0x2118, false, false, false, false},
                    membership_case{"NoBreakSpace", 0x00A0, false, false, true, true},
                    membership_case{"HistoricalSpace", 0x200B, false, false, false, true},
                    membership_case{"Emoji", 0x1F600, false, false, false, false},
                    membership_case{"Surrogate", 0xD800, false, false, false, false},
                    membership_case{"BeyondUnicode", 0x110000, false, false, false, false}),
    [](const testing::TestParamInfo<membership_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Ensure property ranges support bounded binary search without malformed intervals.
   * Arrange: All four generated property arrays.
   * Act: Inspect the version metadata and every interval.
   * Expect: Correct versions and sorted, disjoint, maximally merged scalar ranges.
   */
  TEST(J5_006_UnicodeTables, RangesAreSortedMergedScalars)
  {
    /* Arrange */
    /* Act */
    const auto* version = tables::c_unicode_data_version;
    const auto* baseline = tables::c_unicode_identifier_baseline_version;
    /* Expect */
    EXPECT_STREQ(version, "17.0.0");
    EXPECT_STREQ(baseline, "3.0.0");
    expect_valid_ranges(tables::c_identifier_start_ranges);
    expect_valid_ranges(tables::c_identifier_extra_ranges);
    expect_valid_ranges(tables::c_space_separator_ranges);
    expect_valid_ranges(tables::c_unicode_3_space_separator_ranges);
  }

  /* ================================= End Test_J5_006 ================================= */
} // namespace
