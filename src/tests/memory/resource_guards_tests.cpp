// SPDX-License-Identifier: Apache-2.0
#include "../../../tests/support/guard_provider.hpp"
#include "adaptive_json5/detail/memory/resource_budget.hpp"
#include "adaptive_json5/detail/memory/work_budget.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ===================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;
  /* =================================== End Fixtures =================================== */
  /* =================================== Test_J5_047 =================================== */
  static_assert(std::is_nothrow_default_constructible_v<resource_limits>);
  static_assert(std::is_nothrow_constructible_v<resource_budget, const resource_limits&>);
  static_assert(std::is_nothrow_constructible_v<work_budget, std::size_t>);
  static_assert(!std::is_copy_constructible_v<accounted_storage>);
  static_assert(!std::is_move_constructible_v<accounted_storage>);
  static_assert(!std::is_move_assignable_v<accounted_storage>);
  static_assert(noexcept(std::declval<resource_budget&>().reserve_value(false)));
  static_assert(noexcept(std::declval<resource_budget&>().reserve_string_units(0, 0)));
  static_assert(noexcept(std::declval<accounted_storage&>().allocate(0, 1)));
  static_assert(noexcept(std::declval<work_budget&>().charge(0)));
  /*
   * RetainsDefaultsAndIndependentOverrides.
   * Arrange: Use default and explicit strict options.
   * Act: Override limits in only the strict configuration.
   * Expect: Literal defaults remain unchanged in the independent configuration.
   */
  TEST(J5_047_Options, RetainsDefaultsAndIndependentOverrides)
  {
    /* Arrange */
    const parse_options defaults{};
    parse_options selected{syntax_mode::strict_json};
    /* Act */
    selected.limits.max_depth = 0;
    selected.limits.max_owned_bytes = 7;
    /* Expect */
    EXPECT_EQ(defaults.syntax, syntax_mode::json5);
    EXPECT_EQ(selected.syntax, syntax_mode::strict_json);
    EXPECT_EQ(defaults.limits.max_input_bytes, 16384u);
    EXPECT_EQ(defaults.limits.max_depth, 64u);
    EXPECT_EQ(defaults.limits.max_nodes, 1024u);
    EXPECT_EQ(defaults.limits.max_members, 1024u);
    EXPECT_EQ(defaults.limits.max_string_units, 4096u);
    EXPECT_EQ(defaults.limits.max_total_string_units, 16384u);
    EXPECT_EQ(defaults.limits.max_number_bytes, 1024u);
    EXPECT_EQ(defaults.limits.max_exponent_digits, 128u);
    EXPECT_EQ(defaults.limits.max_owned_bytes, 32768u);
    EXPECT_EQ(defaults.limits.max_diagnostics, 16u);
    EXPECT_EQ(defaults.limits.max_work_units, 32768u);
    EXPECT_EQ(selected.limits.max_depth, 0u);
    EXPECT_EQ(selected.limits.max_owned_bytes, 7u);
  }
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief Literal input and numeric-length boundary expectations.
   * @par Requirements
   * P031; J5-047.
   */
  struct length_case
  {
      std::size_t limit;  // Input inclusive length limit.
      std::size_t length; // Input requested byte/digit count.
      bool accepted;      // Expected guard success.
  };
  class J5_047_LengthGuard : public testing::TestWithParam<length_case>
  {
  };
  /*
   * Check three independent length ceilings.
   * Arrange: Configure the parameter limit for input, number and exponent.
   * Act: Check each requested length twice.
   * Expect: Literal boundary outcomes without cumulative charging.
   */
  TEST_P(J5_047_LengthGuard, ChecksInputNumberAndExponent)
  {
    /* Arrange */
    const auto input = GetParam();
    resource_limits limits;
    limits.max_input_bytes = input.limit;
    limits.max_number_bytes = input.limit;
    limits.max_exponent_digits = input.limit;
    const resource_budget budget{limits};
    /* Act */
    for (int repeat{}; repeat < 2; ++repeat)
    {
      const auto bytes = budget.check_input(input.length);
      const auto spelling = budget.check_number(input.length, 0);
      const auto exponent = budget.check_number(0, input.length);
      /* Expect */
      EXPECT_EQ(bytes.has_value(), input.accepted);
      EXPECT_EQ(spelling.has_value(), input.accepted);
      EXPECT_EQ(exponent.has_value(), input.accepted);
      if (!input.accepted)
      {
        EXPECT_EQ(bytes.error_if()->code, error_code::resource_limit);
        EXPECT_EQ(spelling.error_if()->code, error_code::resource_limit);
        EXPECT_EQ(exponent.error_if()->code, error_code::resource_limit);
        EXPECT_FALSE(bytes.error_if()->has_byte_offset);
      }
    }
  }
  INSTANTIATE_TEST_SUITE_P(P031, J5_047_LengthGuard,
                           testing::Values(length_case{0, 0, true}, length_case{0, 1, false}, length_case{3, 2, true},
                                           length_case{3, 3, true}, length_case{3, 4, false},
                                           length_case{std::numeric_limits<std::size_t>::max(),
                                                       std::numeric_limits<std::size_t>::max(), true},
                                           length_case{3, std::numeric_limits<std::size_t>::max(), false}));
  /*
   * BoundsAndReleasesNodes.
   * Arrange: Configure an inclusive limit of two and an independent zero limit.
   * Act: Reserve at and above the boundary, release, then reuse.
   * Expect: Failure changes no reservation and the original limits were copied.
   */
  TEST(J5_047_Reservations, BoundsAndReleasesNodes)
  {
    /* Arrange */
    resource_limits limits;
    limits.max_nodes = 2;
    resource_budget budget{limits};
    limits.max_nodes = 0;
    resource_budget zero{limits};
    /* Act */
    const auto underflow = budget.release_value(false);
    ASSERT_FALSE(underflow.has_value());
    EXPECT_EQ(underflow.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(budget.reserve_value(false).has_value());
    ASSERT_TRUE(budget.reserve_value(false).has_value());
    const auto excess = budget.reserve_value(false);
    ASSERT_TRUE(budget.release_value(false).has_value());
    ASSERT_TRUE(budget.reserve_value(false).has_value());
    ASSERT_TRUE(budget.release_value(false).has_value());
    ASSERT_TRUE(budget.release_value(false).has_value());
    const auto empty = budget.release_value(false);
    const auto denied = zero.reserve_value(false);
    /* Expect */
    ASSERT_FALSE(excess.has_value());
    EXPECT_EQ(excess.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * BoundsAndReleasesMembers.
   * Arrange: Configure an inclusive limit of two and an independent zero limit.
   * Act: Reserve at and above the boundary, release, then reuse.
   * Expect: Failure changes no reservation and the original limits were copied.
   */
  TEST(J5_047_Reservations, BoundsAndReleasesMembers)
  {
    /* Arrange */
    resource_limits limits;
    limits.max_members = 2;
    resource_budget budget{limits};
    limits.max_members = 0;
    resource_budget zero{limits};
    /* Act */
    const auto underflow = budget.release_value(true);
    ASSERT_FALSE(underflow.has_value());
    EXPECT_EQ(underflow.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(budget.reserve_value(true).has_value());
    ASSERT_TRUE(budget.reserve_value(true).has_value());
    const auto excess = budget.reserve_value(true);
    ASSERT_TRUE(budget.release_value(true).has_value());
    ASSERT_TRUE(budget.reserve_value(true).has_value());
    ASSERT_TRUE(budget.release_value(true).has_value());
    ASSERT_TRUE(budget.release_value(true).has_value());
    const auto empty = budget.release_value(true);
    const auto denied = zero.reserve_value(true);
    /* Expect */
    ASSERT_FALSE(excess.has_value());
    EXPECT_EQ(excess.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * BoundsAndReleasesDepth.
   * Arrange: Configure an inclusive limit of two and an independent zero limit.
   * Act: Reserve at and above the boundary, release, then reuse.
   * Expect: Failure changes no reservation and the original limits were copied.
   */
  TEST(J5_047_Reservations, BoundsAndReleasesDepth)
  {
    /* Arrange */
    resource_limits limits;
    limits.max_depth = 2;
    resource_budget budget{limits};
    limits.max_depth = 0;
    resource_budget zero{limits};
    /* Act */
    const auto underflow = budget.leave_container();
    ASSERT_FALSE(underflow.has_value());
    EXPECT_EQ(underflow.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(budget.enter_container().has_value());
    ASSERT_TRUE(budget.enter_container().has_value());
    const auto excess = budget.enter_container();
    ASSERT_TRUE(budget.leave_container().has_value());
    ASSERT_TRUE(budget.enter_container().has_value());
    ASSERT_TRUE(budget.leave_container().has_value());
    ASSERT_TRUE(budget.leave_container().has_value());
    const auto empty = budget.leave_container();
    const auto denied = zero.enter_container();
    /* Expect */
    ASSERT_FALSE(excess.has_value());
    EXPECT_EQ(excess.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * BoundsAndReleasesDiagnostics.
   * Arrange: Configure an inclusive limit of two and an independent zero limit.
   * Act: Reserve at and above the boundary, release, then reuse.
   * Expect: Failure changes no reservation and the original limits were copied.
   */
  TEST(J5_047_Reservations, BoundsAndReleasesDiagnostics)
  {
    /* Arrange */
    resource_limits limits;
    limits.max_diagnostics = 2;
    resource_budget budget{limits};
    limits.max_diagnostics = 0;
    resource_budget zero{limits};
    /* Act */
    const auto underflow = budget.release_diagnostic();
    ASSERT_FALSE(underflow.has_value());
    EXPECT_EQ(underflow.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_TRUE(budget.reserve_diagnostic().has_value());
    ASSERT_TRUE(budget.reserve_diagnostic().has_value());
    const auto excess = budget.reserve_diagnostic();
    ASSERT_TRUE(budget.release_diagnostic().has_value());
    ASSERT_TRUE(budget.reserve_diagnostic().has_value());
    ASSERT_TRUE(budget.release_diagnostic().has_value());
    ASSERT_TRUE(budget.release_diagnostic().has_value());
    const auto empty = budget.release_diagnostic();
    const auto denied = zero.reserve_diagnostic();
    /* Expect */
    ASSERT_FALSE(excess.has_value());
    EXPECT_EQ(excess.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_FALSE(denied.has_value());
    EXPECT_EQ(denied.error_if()->code, error_code::resource_limit);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * CouplesNodesAndMembersAtomically.
   * Arrange: Configure two nodes but only one member.
   * Act: Fail each coupled boundary then reserve a different value category.
   * Expect: Failed reservations consume neither counter.
   */
  TEST(J5_047_Reservations, CouplesNodesAndMembersAtomically)
  {
    /* Arrange */
    resource_limits limits;
    limits.max_nodes = 2;
    limits.max_members = 1;
    resource_budget budget{limits};
    /* Act */
    ASSERT_TRUE(budget.reserve_value(true).has_value());
    const auto denied_member = budget.reserve_value(true);
    ASSERT_TRUE(budget.reserve_value(false).has_value());
    const auto denied_node = budget.reserve_value(true);
    ASSERT_TRUE(budget.release_value(true).has_value());
    ASSERT_TRUE(budget.reserve_value(true).has_value());
    ASSERT_TRUE(budget.release_value(false).has_value());
    ASSERT_TRUE(budget.release_value(true).has_value());
    /* Expect */
    ASSERT_FALSE(denied_member.has_value());
    ASSERT_FALSE(denied_node.has_value());
    EXPECT_EQ(denied_member.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(denied_node.error_if()->code, error_code::resource_limit);
    EXPECT_FALSE(budget.release_value(false).has_value());
    EXPECT_FALSE(budget.release_value(true).has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * RejectsMismatchedReleaseWithoutMutation.
   * Arrange: Reserve only a member and separately only a nonmember.
   * Act: Release the wrong categories before releasing the original ones.
   * Expect: Both original reservations remain releasable exactly once.
   */
  TEST(J5_047_Reservations, RejectsMismatchedReleaseWithoutMutation)
  {
    /* Arrange */
    resource_limits limits;
    limits.max_nodes = 1;
    limits.max_members = 1;
    resource_budget member{limits};
    resource_budget nonmember{limits};
    ASSERT_TRUE(member.reserve_value(true).has_value());
    ASSERT_TRUE(nonmember.reserve_value(false).has_value());
    /* Act */
    const auto wrong_member = member.release_value(false);
    const auto wrong_nonmember = nonmember.release_value(true);
    /* Expect */
    ASSERT_FALSE(wrong_member.has_value());
    ASSERT_FALSE(wrong_nonmember.has_value());
    EXPECT_EQ(wrong_member.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(wrong_nonmember.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_TRUE(member.release_value(true).has_value());
    EXPECT_TRUE(nonmember.release_value(false).has_value());
    EXPECT_FALSE(member.release_value(true).has_value());
    EXPECT_FALSE(nonmember.release_value(false).has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * ChecksIndividualAndAggregateBeforeCharging.
   * Arrange: Set per-string limit three and aggregate limit five.
   * Act: Attempt both boundaries, rollback units and reuse.
   * Expect: Both failures preserve aggregate usage and releases prevent underflow.
   */
  TEST(J5_047_Strings, ChecksIndividualAndAggregateBeforeCharging)
  {
    /* Arrange */
    resource_limits limits;
    limits.max_string_units = 3;
    limits.max_total_string_units = 5;
    resource_budget budget{limits};
    /* Act */
    ASSERT_TRUE(budget.reserve_string_units(0, 2).has_value());
    ASSERT_TRUE(budget.reserve_string_units(2, 1).has_value());
    const auto individual = budget.reserve_string_units(3, 1);
    ASSERT_TRUE(budget.reserve_string_units(0, 2).has_value());
    const auto total = budget.reserve_string_units(2, 1);
    ASSERT_TRUE(budget.reserve_string_units(2, 0).has_value());
    const auto underflow = budget.release_string_units(6);
    ASSERT_TRUE(budget.release_string_units(2).has_value());
    ASSERT_TRUE(budget.reserve_string_units(0, 2).has_value());
    /* Expect */
    ASSERT_FALSE(individual.has_value());
    ASSERT_FALSE(total.has_value());
    EXPECT_EQ(individual.error_if()->code, error_code::resource_limit);
    EXPECT_EQ(total.error_if()->code, error_code::resource_limit);
    ASSERT_FALSE(underflow.has_value());
    EXPECT_EQ(underflow.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_TRUE(budget.release_string_units(5).has_value());
    EXPECT_TRUE(budget.release_string_units(0).has_value());
    EXPECT_FALSE(budget.release_string_units(1).has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * HandlesZeroAndMaximumWithoutOverflow.
   * Arrange: Allow the full size_t domain without allocating.
   * Act: Reach maximum and attempt both per-string and aggregate overflow.
   * Expect: Overflow rejects without wrapping, and the full reservation can be released.
   */
  TEST(J5_047_Strings, HandlesZeroAndMaximumWithoutOverflow)
  {
    /* Arrange */
    const auto maximum = std::numeric_limits<std::size_t>::max();
    resource_limits limits;
    limits.max_string_units = maximum;
    limits.max_total_string_units = maximum;
    resource_budget budget{limits};
    limits.max_string_units = 0;
    limits.max_total_string_units = 0;
    resource_budget zero{limits};
    /* Act */
    ASSERT_TRUE(budget.reserve_string_units(0, maximum).has_value());
    const auto individual = budget.reserve_string_units(maximum, 1);
    const auto aggregate = budget.reserve_string_units(0, 1);
    const auto empty = zero.reserve_string_units(0, 0);
    const auto denied = zero.reserve_string_units(0, 1);
    /* Expect */
    EXPECT_FALSE(individual.has_value());
    EXPECT_FALSE(aggregate.has_value());
    EXPECT_TRUE(empty.has_value());
    EXPECT_FALSE(denied.has_value());
    EXPECT_TRUE(budget.release_string_units(maximum).has_value());
    EXPECT_TRUE(budget.reserve_string_units(0, 1).has_value());
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_047 ================================= */
  /* =================================== Test_J5_049 =================================== */
  /**
   * @brief Work charging cases spanning zero, exact and overflowing requests.
   * @par Requirements
   * P031; J5-049.
   */
  struct work_case
  {
      std::size_t limit;     // Input total available work units.
      std::size_t initial;   // Input first successful charge.
      std::size_t next;      // Input candidate charge.
      bool accepted;         // Expected candidate success.
      std::size_t consumed;  // Expected total consumed units.
      std::size_t remaining; // Expected remaining units.
  };
  class J5_049_Work : public testing::TestWithParam<work_case>
  {
  };
  /*
   * Work accounting never wraps or refunds failed charges.
   * Arrange: Charge the initial amount.
   * Act: Attempt the candidate charge and a zero-cost charge.
   * Expect: Literal totals and explicit resource exhaustion.
   */
  TEST_P(J5_049_Work, ChargesBeforeWorkAndPreservesFailureState)
  {
    /* Arrange */
    const auto input = GetParam();
    work_budget work{input.limit};
    EXPECT_EQ(work.consumed(), 0u);
    EXPECT_EQ(work.remaining(), input.limit);
    ASSERT_TRUE(work.charge(input.initial).has_value());
    /* Act */
    const auto next = work.charge(input.next);
    const auto zero = work.charge(0);
    /* Expect */
    EXPECT_EQ(next.has_value(), input.accepted);
    if (!input.accepted)
    {
      EXPECT_EQ(next.error_if()->code, error_code::resource_limit);
      EXPECT_FALSE(next.error_if()->has_byte_offset);
    }
    EXPECT_TRUE(zero.has_value());
    EXPECT_EQ(work.consumed(), input.consumed);
    EXPECT_EQ(work.remaining(), input.remaining);
  }
  constexpr auto maximum_work = std::numeric_limits<std::size_t>::max();
  INSTANTIATE_TEST_SUITE_P(P031, J5_049_Work,
                           testing::Values(work_case{0, 0, 0, true, 0, 0}, work_case{0, 0, 1, false, 0, 0},
                                           work_case{5, 0, 4, true, 4, 1}, work_case{5, 0, 5, true, 5, 0},
                                           work_case{5, 0, 6, false, 0, 5}, work_case{5, 3, 2, true, 5, 0},
                                           work_case{5, 3, 3, false, 3, 2}, work_case{5, 5, 1, false, 5, 0},
                                           work_case{maximum_work, maximum_work, 1, false, maximum_work, 0},
                                           work_case{maximum_work, maximum_work - 1, 1, true, maximum_work, 0},
                                           work_case{maximum_work, maximum_work - 1, 2, false, maximum_work - 1, 1}));
  /*
   * ExhaustionDoesNotPreventOwnedCleanup.
   * Arrange: Allocate through accounting and exhaust an independent work guard.
   * Act: Destroy an owned block after work exhaustion.
   * Expect: Cleanup releases memory without refunding consumed work.
   */
  TEST(J5_049_Work, ExhaustionDoesNotPreventOwnedCleanup)
  {
    /* Arrange */
    guard_provider provider;
    accounted_storage storage{provider.callbacks(), 4};
    work_budget work{1};
    /* Act */
    {
      const auto block = storage.allocate(4, 1);
      ASSERT_TRUE(block.has_value());
      ASSERT_TRUE(work.charge(1).has_value());
      const auto exhausted = work.charge(1);
      ASSERT_FALSE(exhausted.has_value());
      EXPECT_EQ(exhausted.error_if()->code, error_code::resource_limit);
    }
    /* Expect */
    EXPECT_EQ(storage.current_bytes(), 0u);
    EXPECT_EQ(storage.peak_bytes(), 4u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(work.consumed(), 1u);
    EXPECT_EQ(work.remaining(), 0u);
  }
  /* ----------------------------------------------------------------------------------- */
  /* ================================= End Test_J5_049 ================================= */
} // namespace
