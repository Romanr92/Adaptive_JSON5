#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/options.hpp"
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
  constexpr std::size_t c_entry_bytes = sizeof(string_warning);

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

  /**
   * @brief Reserve and append count copies of one warning.
   * @par Requirements
   * P039; J5-017.
   * @return True when every reservation and append succeeded.
   */
  bool append_warnings(warning_list& list, std::size_t count)
  {
    for (std::size_t index{}; index < count; ++index)
    {
      if (!list.reserve(1U).has_value() || !list.append_reserved(c_line).has_value())
      {
        return false;
      }
    }

    return true;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_017 =================================== */
  static_assert(std::is_nothrow_default_constructible_v<warning_list>);
  static_assert(std::is_nothrow_move_constructible_v<warning_list>);
  static_assert(!std::is_copy_constructible_v<warning_list>);
  static_assert(!std::is_copy_assignable_v<warning_list>);
  static_assert(!std::is_move_assignable_v<warning_list>);
  static_assert(warning_list::c_adaptive_initial_entries == 64U);

  /* Verify reserved capacity accepts exactly that many appends, in order.
   * Arrange: A list with a maximum of three entries, all reserved.
   * Act: Append three warnings and then a fourth.
   * Expect: Three stored in order; the fourth is wrong_type_or_access and changes nothing.
   */
  TEST(J5_017_WarningList, AppendsOnlyIntoReservedCapacity)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks(), 3U};
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

  /* Verify adaptive growth copies existing entries in order.
   * Arrange: An adaptive list with a maximum of 200 holding 64 entries, the first two distinct.
   * Act: Reserve and append a 65th entry, which needs a larger table.
   * Expect: All entries preserved in order after the copy.
   */
  TEST(J5_017_WarningList, AdaptiveGrowthPreservesEntries)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks(), 200U, warning_storage_policy::adaptive};
    ASSERT_TRUE(list.reserve(2U).has_value());
    ASSERT_TRUE(list.append_reserved(c_paragraph).has_value());
    ASSERT_TRUE(list.append_reserved(c_line).has_value());
    ASSERT_TRUE(append_warnings(list, 62U));
    const std::size_t attempts = provider.m_attempts;
    /* Act */
    const auto grown = list.reserve(1U);
    const auto appended = list.append_reserved(c_paragraph);
    /* Expect */
    ASSERT_TRUE(grown.has_value());
    ASSERT_TRUE(appended.has_value());
    EXPECT_EQ(provider.m_attempts, attempts + 1U);
    ASSERT_EQ(list.size(), 65U);
    expect_warning(list, 0U, c_paragraph);
    expect_warning(list, 1U, c_line);
    expect_warning(list, 63U, c_line);
    expect_warning(list, 64U, c_paragraph);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a failed growth leaves every entry untouched and permits retry.
   * Arrange: An adaptive list with one stored warning and failure injected on the next request.
   * Act: Reserve 70 more entries, beyond the first 64-entry table, then retry without injection.
   * Expect: allocation_failure with the original entry intact, then successful growth.
   */
  TEST(J5_017_WarningList, AllocationFailurePreservesEntries)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks(), 200U, warning_storage_policy::adaptive};
    ASSERT_TRUE(list.reserve(1U).has_value());
    ASSERT_TRUE(list.append_reserved(c_line).has_value());
    provider.m_fail_on = provider.m_attempts + 1U;
    /* Act */
    const auto failed = list.reserve(70U);
    const auto retried = list.reserve(70U);
    /* Expect */
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
    ASSERT_TRUE(retried.has_value());
    ASSERT_EQ(list.size(), 1U);
    expect_warning(list, 0U, c_line);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify moving transfers the entries, maximum and policy, and leaves an empty source.
   * Arrange: Store two warnings in a list with a maximum of two.
   * Act: Move-construct a second list.
   * Expect: The destination holds both entries and still rejects a third; the source
   *         reports none and rejects access.
   */
  TEST(J5_017_WarningList, MoveTransfersEntries)
  {
    /* Arrange */
    guard_provider provider;
    warning_list source{provider.callbacks(), 2U};
    ASSERT_TRUE(source.reserve(2U).has_value());
    ASSERT_TRUE(source.append_reserved(c_line).has_value());
    ASSERT_TRUE(source.append_reserved(c_paragraph).has_value());
    /* Act */
    warning_list destination{std::move(source)};
    /* Expect */
    ASSERT_EQ(destination.size(), 2U);
    expect_warning(destination, 0U, c_line);
    expect_warning(destination, 1U, c_paragraph);
    const auto beyond = destination.reserve(1U);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(source.size(), 0U);
    EXPECT_FALSE(source.at(0U).has_value());
    EXPECT_FALSE(source.append_reserved(c_line).has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a list without a provider holds nothing and needs no storage for zero entries.
   * Arrange: Default-construct a list, whose maximum is zero.
   * Act: Reserve zero and one entries and read the first entry.
   * Expect: Zero succeeds, one is resource_limit, and reading is wrong_type_or_access.
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
    ASSERT_FALSE(one.has_value());
    EXPECT_EQ(one.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(list.size(), 0U);
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_017 ================================= */
  /* =================================== Test_J5_046 =================================== */
  /* Verify the fixed policy allocates the whole table once and never grows.
   * Arrange: A fixed list with a maximum of 16 entries.
   * Act: Reserve zero entries, then fill all 16 one at a time, then reserve one more.
   * Expect: No request for zero entries; exactly one allocation of 16 entries; the 17th
   *         is resource_limit with no request and the entries unchanged.
   */
  TEST(J5_046_WarningList, FixedPolicyAllocatesMaximumOnce)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks(), 16U};
    /* Act */
    const auto none = list.reserve(0U);
    const std::size_t attempts_after_none = provider.m_attempts;
    const bool filled = append_warnings(list, 16U);
    const std::size_t attempts_after_fill = provider.m_attempts;
    const auto beyond = list.reserve(1U);
    /* Expect */
    EXPECT_TRUE(none.has_value());
    EXPECT_EQ(attempts_after_none, 0U);
    ASSERT_TRUE(filled);
    EXPECT_EQ(attempts_after_fill, 1U);
    EXPECT_EQ(provider.m_live_bytes, 16U * c_entry_bytes);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(provider.m_attempts, attempts_after_fill);
    EXPECT_EQ(list.size(), 16U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a zero maximum rejects the first warning without allocating.
   * Arrange: Fixed and adaptive lists with a maximum of zero.
   * Act: Reserve one entry.
   * Expect: resource_limit and no provider request.
   */
  TEST(J5_046_WarningList, ZeroMaximumFailsWithoutAllocating)
  {
    for (const auto policy : {warning_storage_policy::fixed, warning_storage_policy::adaptive})
    {
      /* Arrange */
      guard_provider provider;
      warning_list list{provider.callbacks(), 0U, policy};
      /* Act */
      const auto reserved = list.reserve(1U);
      /* Expect */
      ASSERT_FALSE(reserved.has_value());
      EXPECT_EQ(reserved.error_if()->code, error_code::resource_limit);
      EXPECT_EQ(provider.m_attempts, 0U);
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a multi-entry reservation beyond the remaining slots writes nothing.
   * Arrange: A fixed list with a maximum of four holding two entries.
   * Act: Reserve three more, as for one string with three warnings.
   * Expect: resource_limit, no new request and the two entries unchanged.
   */
  TEST(J5_046_WarningList, ReservationBeyondRemainingSlotsWritesNothing)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks(), 4U};
    ASSERT_TRUE(list.reserve(2U).has_value());
    ASSERT_TRUE(list.append_reserved(c_line).has_value());
    ASSERT_TRUE(list.append_reserved(c_paragraph).has_value());
    const std::size_t attempts = provider.m_attempts;
    /* Act */
    const auto reserved = list.reserve(3U);
    /* Expect */
    ASSERT_FALSE(reserved.has_value());
    EXPECT_EQ(reserved.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(provider.m_attempts, attempts);
    ASSERT_EQ(list.size(), 2U);
    expect_warning(list, 0U, c_line);
    expect_warning(list, 1U, c_paragraph);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the adaptive policy starts at 64 entries, doubles, and stops at the maximum.
   * Arrange: An adaptive list with a maximum of 200 entries.
   * Act: Append entries one at a time up to 200, recording live bytes at the boundaries,
   *      then reserve one more.
   * Expect: 64 entries after the 1st, 63rd and 64th; 128 after the 65th; 200 (the cap)
   *         after the 129th; resource_limit for the 201st.
   */
  TEST(J5_046_WarningList, AdaptivePolicyGrowsByDoublingToTheMaximum)
  {
    /* Arrange */
    guard_provider provider;
    warning_list list{provider.callbacks(), 200U, warning_storage_policy::adaptive};
    /* Act and Expect, at each boundary */
    ASSERT_TRUE(append_warnings(list, 1U));
    EXPECT_EQ(provider.m_live_bytes, 64U * c_entry_bytes);
    ASSERT_TRUE(append_warnings(list, 62U)); // 63 entries
    EXPECT_EQ(provider.m_live_bytes, 64U * c_entry_bytes);
    ASSERT_TRUE(append_warnings(list, 1U)); // 64 entries
    EXPECT_EQ(provider.m_live_bytes, 64U * c_entry_bytes);
    EXPECT_EQ(provider.m_attempts, 1U);
    ASSERT_TRUE(append_warnings(list, 1U)); // 65 entries
    EXPECT_EQ(provider.m_live_bytes, 128U * c_entry_bytes);
    ASSERT_TRUE(append_warnings(list, 64U)); // 129 entries
    EXPECT_EQ(provider.m_live_bytes, 200U * c_entry_bytes);
    ASSERT_TRUE(append_warnings(list, 71U)); // 200 entries
    EXPECT_EQ(provider.m_attempts, 3U);
    const auto beyond = list.reserve(1U);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(list.size(), 200U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the adaptive policy equals the fixed one while the maximum is at most 64.
   * Arrange: Adaptive lists with maxima of 16 and 64.
   * Act: Reserve one entry.
   * Expect: One allocation holding the whole maximum.
   */
  TEST(J5_046_WarningList, AdaptiveMatchesFixedUpTo64Entries)
  {
    for (const std::size_t maximum : {std::size_t{16}, std::size_t{64}})
    {
      /* Arrange */
      guard_provider provider;
      warning_list list{provider.callbacks(), maximum, warning_storage_policy::adaptive};
      /* Act */
      const auto reserved = list.reserve(1U);
      /* Expect */
      ASSERT_TRUE(reserved.has_value()) << maximum;
      EXPECT_EQ(provider.m_attempts, 1U) << maximum;
      EXPECT_EQ(provider.m_live_bytes, maximum * c_entry_bytes) << maximum;
    }
  }
  /* ================================= End Test_J5_046 ================================= */
} // namespace
