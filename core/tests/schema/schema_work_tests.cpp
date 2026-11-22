#include "adaptive_json5/schema/schema_budget.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/validation_options.hpp"
#include "schema/schema_work.hpp"

#include <cstddef>
#include <gtest/gtest.h>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;

  /**
   * @brief Count polls and stop when asked.
   * @par Requirements
   * P043; J5-049.
   */
  struct poll_counter
  {
      std::size_t polls{};
      bool stop{};

      static bool poll(void* context) noexcept
      {
        auto& self = *static_cast<poll_counter*>(context);
        ++self.polls;
        return self.stop;
      }
  };

  /* =================================== Test_P043_SchemaWork ==================================== */
  /*
   * Arrange: A budget of three work steps.
   * Act: Charge four steps.
   * Expect: Three succeed; the fourth returns resource_limit (max_work_steps) and is not counted.
   */
  TEST(P043_SchemaWork, WorkStepCapIsInclusive)
  {
    /* Arrange */
    validation_budget budget{};
    budget.max_work_steps = 3U;
    schema_work work{budget, cancellation_callback{}};
    /* Act */
    const bool first = work.step().has_value();
    const bool second = work.step().has_value();
    const bool third = work.step().has_value();
    const auto fourth = work.step();
    /* Expect */
    EXPECT_TRUE(first && second && third);
    ASSERT_FALSE(fourth.has_value());
    EXPECT_EQ(fourth.error_if()->code(), schema_error_code::resource_limit);
    EXPECT_EQ(fourth.error_if()->limit_reason(), schema_limit_reason::max_work_steps);
    EXPECT_EQ(work.work_steps(), 3U);
  }

  /*
   * Arrange: A callback that does not stop, then stops.
   * Act: Charge steps.
   * Expect: The callback is polled once per step; a stop returns cancelled without counting.
   */
  TEST(P043_SchemaWork, PollsCancellationOncePerStep)
  {
    /* Arrange */
    poll_counter counter{};
    schema_work work{validation_budget{}, cancellation_callback{&poll_counter::poll, &counter}};
    /* Act */
    const bool first = work.step().has_value();
    const bool second = work.step().has_value();
    counter.stop = true;
    const auto third = work.step();
    /* Expect */
    EXPECT_TRUE(first && second);
    ASSERT_FALSE(third.has_value());
    EXPECT_EQ(third.error_if()->code(), schema_error_code::cancelled);
    EXPECT_EQ(counter.polls, 3U);
    EXPECT_EQ(work.work_steps(), 2U);
  }

  /*
   * Arrange: An exhausted budget and a callback that asks to stop.
   * Act: Charge one step.
   * Expect: cancelled wins, since the callback is polled before the cap is checked.
   */
  TEST(P043_SchemaWork, CancellationIsCheckedBeforeTheCap)
  {
    /* Arrange */
    poll_counter counter{0U, true};
    validation_budget budget{};
    budget.max_work_steps = 0U;
    schema_work work{budget, cancellation_callback{&poll_counter::poll, &counter}};
    /* Act */
    const auto outcome = work.step();
    /* Expect */
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error_if()->code(), schema_error_code::cancelled);
  }

  /*
   * Arrange: A budget with a changed field.
   * Act: Construct the work context and change the caller's copy.
   * Expect: The context keeps its own copy.
   */
  TEST(P043_SchemaWork, KeepsItsOwnBudgetCopy)
  {
    /* Arrange */
    validation_budget budget{};
    budget.max_depth = 7U;
    /* Act */
    const schema_work work{budget, cancellation_callback{}};
    budget.max_depth = 9U;
    /* Expect */
    EXPECT_EQ(work.budget().max_depth, 7U);
    EXPECT_EQ(work.work_steps(), 0U);
  }
} // namespace
