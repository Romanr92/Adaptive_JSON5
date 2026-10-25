#include "../support/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/value/string_warning.hpp"
#include "adaptive_json5/value/warning_list.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  constexpr string_warning c_line{string_warning_code::raw_line_separator, 1U, 3U};
  constexpr string_warning c_paragraph{string_warning_code::raw_paragraph_separator, 7U, 3U};

  /**
   * @brief Compare a stored warning with an expected value field by field.
   * @par Requirements
   * P039; J5-017.
   */
  void expect_warning(const warning_list& list, std::size_t index, const string_warning& expected)
  {
    const auto stored = list.at(index);
    ASSERT_TRUE(stored.has_value()) << "index " << index;
    EXPECT_EQ(stored.value_if()->code, expected.code) << "index " << index;
    EXPECT_EQ(stored.value_if()->byte_offset, expected.byte_offset) << "index " << index;
    EXPECT_EQ(stored.value_if()->byte_length, expected.byte_length) << "index " << index;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_017 =================================== */
  static_assert(std::is_nothrow_default_constructible_v<warning_list>);
  static_assert(std::is_nothrow_move_constructible_v<warning_list>);
  static_assert(!std::is_copy_constructible_v<warning_list>);
  static_assert(!std::is_copy_assignable_v<warning_list>);
  static_assert(!std::is_move_assignable_v<warning_list>);

  /* Verify reserved capacity accepts exactly that many appends, in order.
   * Arrange: Reserve three entries.
   * Act: Append three warnings and then a fourth.
   * Expect: Three stored in order; the fourth is wrong_type_or_access and changes nothing.
   */
  TEST(J5_017_WarningList, AppendsOnlyIntoReservedCapacity)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks()};
    ASSERT_TRUE(list.reserve(3U).has_value());
    /* Act */
    const auto first = list.append_reserved(c_line);
    const auto second = list.append_reserved(c_paragraph);
    const auto third = list.append_reserved(c_line);
    const auto overflow = list.append_reserved(c_paragraph);
    /* Expect */
    EXPECT_TRUE(first.has_value());
    EXPECT_TRUE(second.has_value());
    EXPECT_TRUE(third.has_value());
    ASSERT_FALSE(overflow.has_value());
    EXPECT_EQ(overflow.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_EQ(list.size(), 3U);
    expect_warning(list, 0U, c_line);
    expect_warning(list, 1U, c_paragraph);
    expect_warning(list, 2U, c_line);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify growth copies existing entries and that capacity already available allocates nothing.
   * Arrange: Reserve and fill two entries.
   * Act: Reserve one more, append it, then reserve within the doubled capacity.
   * Expect: Entries preserved in order and no provider request for the last reservation.
   */
  TEST(J5_017_WarningList, GrowthPreservesEntries)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks()};
    ASSERT_TRUE(list.reserve(2U).has_value());
    ASSERT_TRUE(list.append_reserved(c_line).has_value());
    ASSERT_TRUE(list.append_reserved(c_paragraph).has_value());
    /* Act */
    const auto grown = list.reserve(1U);
    const auto appended = list.append_reserved(c_line);
    const std::size_t attempts = provider.m_attempts;
    // Doubling from two gives four, so one further entry needs no allocation.
    const auto within = list.reserve(1U);
    /* Expect */
    ASSERT_TRUE(grown.has_value());
    ASSERT_TRUE(appended.has_value());
    ASSERT_TRUE(within.has_value());
    EXPECT_EQ(provider.m_attempts, attempts);
    ASSERT_EQ(list.size(), 3U);
    expect_warning(list, 0U, c_line);
    expect_warning(list, 1U, c_paragraph);
    expect_warning(list, 2U, c_line);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a failed growth leaves every entry untouched and permits retry.
   * Arrange: Store one warning and inject failure on the next provider request.
   * Act: Reserve ten more, then retry without injection.
   * Expect: allocation_failure with the original entry intact, then successful growth.
   */
  TEST(J5_017_WarningList, AllocationFailurePreservesEntries)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks()};
    ASSERT_TRUE(list.reserve(1U).has_value());
    ASSERT_TRUE(list.append_reserved(c_line).has_value());
    provider.m_fail_on = provider.m_attempts + 1U;
    /* Act */
    const auto failed = list.reserve(10U);
    const auto retried = list.reserve(10U);
    /* Expect */
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
    ASSERT_TRUE(retried.has_value());
    ASSERT_EQ(list.size(), 1U);
    expect_warning(list, 0U, c_line);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify moving transfers the entries and leaves an empty source.
   * Arrange: Store two warnings.
   * Act: Move-construct a second list.
   * Expect: The destination holds both entries; the source reports none and rejects access.
   */
  TEST(J5_017_WarningList, MoveTransfersEntries)
  {
    /* Arrange */
    guard_provider provider;
    warning_list source{provider.callbacks()};
    ASSERT_TRUE(source.reserve(2U).has_value());
    ASSERT_TRUE(source.append_reserved(c_line).has_value());
    ASSERT_TRUE(source.append_reserved(c_paragraph).has_value());
    /* Act */
    const warning_list destination{std::move(source)};
    /* Expect */
    ASSERT_EQ(destination.size(), 2U);
    expect_warning(destination, 0U, c_line);
    expect_warning(destination, 1U, c_paragraph);
    EXPECT_EQ(source.size(), 0U);
    EXPECT_FALSE(source.at(0U).has_value());
    EXPECT_FALSE(source.append_reserved(c_line).has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a list without a provider holds nothing and needs no storage for zero entries.
   * Arrange: Default-construct a list.
   * Act: Reserve zero and one entries and read the first entry.
   * Expect: Zero succeeds, one fails, and reading is wrong_type_or_access.
   */
  TEST(J5_017_WarningList, DefaultListOwnsNothing)
  {
    /* Arrange */
    warning_list list{};
    /* Act */
    const auto none = list.reserve(0U);
    const auto one = list.reserve(1U);
    const auto read = list.at(0U);
    /* Expect */
    EXPECT_TRUE(none.has_value());
    EXPECT_FALSE(one.has_value());
    EXPECT_EQ(list.size(), 0U);
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_017 ================================= */
} // namespace
