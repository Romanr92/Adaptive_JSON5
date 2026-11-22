#include "../../../tests/shared_infrastructure/document_provider.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/parser.hpp"
#include "adaptive_json5/schema/schema_error.hpp"
#include "adaptive_json5/schema/validation_options.hpp"
#include "schema/instance_domain.hpp"
#include "schema/schema_work.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  using adaptive_json5_test::document_provider;

  /**
   * @brief Parsed JSON5 instance with preserved duplicates and its storage.
   * @par Requirements
   * P043; J5-059; J5-061.
   * @note Member order keeps the provider alive until the document is released.
   */
  struct parsed_instance
  {
      document_provider provider;
      std::string text;
      accounted_storage accounting;
      std::optional<parsed_document> parsed;

      explicit parsed_instance(const char* input)
          : text(input), accounting(provider.callbacks(), parse_options{}.limits.max_owned_bytes)
      {
        auto outcome = parse_document({text.data(), text.size()}, parse_options{}, accounting);
        if (outcome.has_value())
        {
          parsed.emplace(std::move(*outcome.value_if()));
        }
      }

      const document& values() const
      {
        return parsed->values;
      }
  };

  /**
   * @brief Run the pre-pass with a fresh work context and its own work storage.
   * @par Requirements
   * P043; J5-065.
   */
  result<void, schema_error> check(const parsed_instance& instance, document_provider& work_provider,
                                   const validation_budget& budget = {}, const cancellation_callback& cancellation = {})
  {
    schema_work work{budget, cancellation};
    return check_instance_domain(instance.values(), work, work_provider.callbacks());
  }

  /**
   * @brief Count the steps a successful pre-pass charges.
   * @par Requirements
   * P043; J5-065.
   */
  std::size_t steps_of(const parsed_instance& instance)
  {
    document_provider work_provider;
    schema_work work{validation_budget{}, cancellation_callback{}};
    const auto outcome = check_instance_domain(instance.values(), work, work_provider.callbacks());
    EXPECT_TRUE(outcome.has_value());
    return work.work_steps();
  }

  /* =================================== Test_P043_InstanceDomain ==================================== */
  struct domain_case
  {
      const char* name;                                  // Case identifier.
      const char* input;                                 // JSON5 instance parsed with preserve_all.
      std::optional<unsupported_instance_reason> reason; // nullopt: JSON-compatible.
      source_span range;                                 // Expected range of the offending item.
  };

  class P043_InstanceDomain : public testing::TestWithParam<domain_case>
  {
  };

  /*
   * Arrange: Instances that are JSON-compatible or contain exactly one first unsupported item.
   * Act: Run the instance-domain pre-pass.
   * Expect: Success, or unsupported_instance with the reason and range of the first offending item;
   *         all work storage is released.
   */
  TEST_P(P043_InstanceDomain, RejectsOnlyNonJsonValues)
  {
    /* Arrange */
    const auto& fixture = GetParam();
    const parsed_instance instance{fixture.input};
    ASSERT_TRUE(instance.parsed.has_value());
    document_provider work_provider;
    /* Act */
    const auto outcome = check(instance, work_provider);
    /* Expect */
    if (!fixture.reason.has_value())
    {
      EXPECT_TRUE(outcome.has_value());
    }
    else
    {
      ASSERT_FALSE(outcome.has_value());
      const auto& failure = *outcome.error_if();
      EXPECT_EQ(failure.code(), schema_error_code::unsupported_instance);
      EXPECT_EQ(failure.instance_reason(), fixture.reason);
      ASSERT_TRUE(failure.source_range().has_value());
      EXPECT_EQ(failure.source_range()->byte_offset, fixture.range.byte_offset);
      EXPECT_EQ(failure.source_range()->byte_length, fixture.range.byte_length);
      EXPECT_FALSE(failure.parser_error().has_value());
    }
    EXPECT_EQ(work_provider.m_live_bytes, 0U);
    EXPECT_EQ(work_provider.m_allocations, work_provider.m_releases);
  }

  INSTANTIATE_TEST_SUITE_P(
    Cases, P043_InstanceDomain,
    testing::Values(
      domain_case{"scalar_null", "null", std::nullopt, {}}, domain_case{"scalar_number", "-0x1F", std::nullopt, {}},
      domain_case{"nested", R"({"a": [1, {"b": "x"}, []], "c": {}})", std::nullopt, {}},
      domain_case{"same_name_other_objects", R"({"a": {"a": 1}, "b": [{"a": 1}, {"a": 2}]})", std::nullopt, {}},
      domain_case{"valid_pair", R"(["😀", {"😀": 1}])", std::nullopt, {}},
      domain_case{"root_nan", "NaN", unsupported_instance_reason::non_finite_number, {0U, 3U}},
      domain_case{"nested_infinity", "[1, {a: -Infinity}]", unsupported_instance_reason::non_finite_number, {8U, 9U}},
      domain_case{"positive_infinity", "[+Infinity]", unsupported_instance_reason::non_finite_number, {1U, 9U}},
      domain_case{"lone_high", R"(["\uD800"])", unsupported_instance_reason::unpaired_surrogate, {1U, 8U}},
      domain_case{"lone_low", R"("\uDC00")", unsupported_instance_reason::unpaired_surrogate, {0U, 8U}},
      domain_case{"high_then_text", R"("\uD800x")", unsupported_instance_reason::unpaired_surrogate, {0U, 9U}},
      domain_case{"reversed_pair", R"("\uDE00\uD83D")", unsupported_instance_reason::unpaired_surrogate, {0U, 14U}},
      domain_case{"name_surrogate", R"({"\uD800": 1})", unsupported_instance_reason::unpaired_surrogate, {1U, 8U}},
      domain_case{"duplicate", R"({"a": 1, "a": 2})", unsupported_instance_reason::duplicate_name, {9U, 3U}},
      domain_case{
        "duplicate_quoted_and_identifier", R"({"a": 1, a: 2})", unsupported_instance_reason::duplicate_name, {9U, 1U}},
      domain_case{"duplicate_identifier", "{a: 1, b: 2, a: 3}", unsupported_instance_reason::duplicate_name, {13U, 1U}},
      domain_case{
        "duplicate_nested", R"([{"x": {"k": 1, "k": 2}}])", unsupported_instance_reason::duplicate_name, {16U, 3U}},
      domain_case{"first_in_document_order",
                  R"([NaN, {"a": 1, "a": 2}])",
                  unsupported_instance_reason::non_finite_number,
                  {1U, 3U}},
      domain_case{
        "duplicate_before_children", R"({"a": NaN, "a": 1})", unsupported_instance_reason::duplicate_name, {11U, 3U}}),
    [](const testing::TestParamInfo<domain_case>& param_info)
    {
      return std::string{param_info.param.name};
    });

  /*
   * Arrange: Two names that differ in spelling but decode to the same text.
   * Act: Run the pre-pass.
   * Expect: duplicate_name at the repeated name's offset; equality uses decoded names.
   */
  TEST(P043_InstanceDomainNames, ComparesDecodedNames)
  {
    /* Arrange */
    const parsed_instance instance{R"({"a": 1, "a": 2})"};
    ASSERT_TRUE(instance.parsed.has_value());
    document_provider work_provider;
    /* Act */
    const auto outcome = check(instance, work_provider);
    /* Expect */
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error_if()->instance_reason(), unsupported_instance_reason::duplicate_name);
    ASSERT_TRUE(outcome.error_if()->source_range().has_value());
    // Only the offset is checked: the stored member-name span length of an escaped name is
    // currently not its source length (parser finding reported with P043).
    EXPECT_EQ(outcome.error_if()->source_range()->byte_offset, 9U);
  }

  /* =================================== Test_P043_InstanceDomainStorage ==================================== */
  /*
   * Arrange: A scalar root.
   * Act: Run the pre-pass.
   * Expect: No work storage is requested.
   */
  TEST(P043_InstanceDomainStorage, ScalarRootNeedsNoWorkStorage)
  {
    /* Arrange */
    const parsed_instance instance{R"("text")"};
    ASSERT_TRUE(instance.parsed.has_value());
    document_provider work_provider;
    /* Act */
    const auto outcome = check(instance, work_provider);
    /* Expect */
    EXPECT_TRUE(outcome.has_value());
    EXPECT_EQ(work_provider.m_attempts, 0U);
  }

  /*
   * Arrange: A container root and a work provider that fails its first request.
   * Act: Run the pre-pass.
   * Expect: allocation_failure carrying the provider error unchanged.
   */
  TEST(P043_InstanceDomainStorage, ReportsWorkAllocationFailure)
  {
    /* Arrange */
    const parsed_instance instance{"[1]"};
    ASSERT_TRUE(instance.parsed.has_value());
    document_provider work_provider;
    work_provider.m_fail_on = 1U;
    /* Act */
    const auto outcome = check(instance, work_provider);
    /* Expect */
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error_if()->code(), schema_error_code::allocation_failure);
    ASSERT_TRUE(outcome.error_if()->parser_error().has_value());
    EXPECT_EQ(outcome.error_if()->parser_error()->code(), error_code::allocation_failure);
    EXPECT_EQ(work_provider.m_allocations, 0U);
  }

  /*
   * Arrange: A work storage byte ceiling smaller than one stack frame.
   * Act: Run the pre-pass.
   * Expect: resource_limit without a schema reason; the storage error names owned_bytes.
   */
  TEST(P043_InstanceDomainStorage, ReportsWorkByteCeiling)
  {
    /* Arrange */
    const parsed_instance instance{"[1]"};
    ASSERT_TRUE(instance.parsed.has_value());
    document_provider work_provider;
    accounted_storage work_accounting{work_provider.callbacks(), 1U};
    schema_work work{validation_budget{}, cancellation_callback{}};
    /* Act */
    const auto outcome = check_instance_domain(instance.values(), work, work_accounting.callbacks());
    /* Expect */
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error_if()->code(), schema_error_code::resource_limit);
    EXPECT_FALSE(outcome.error_if()->limit_reason().has_value());
    ASSERT_TRUE(outcome.error_if()->parser_error().has_value());
    EXPECT_EQ(outcome.error_if()->parser_error()->limit_reason(), resource_limit_reason::owned_bytes);
  }

  /* =================================== Test_P043_InstanceDomainBudget ==================================== */
  /*
   * Arrange: Three nested containers and depth limits around three.
   * Act: Run the pre-pass for each limit.
   * Expect: The limit is inclusive; one less returns resource_limit (max_depth).
   */
  TEST(P043_InstanceDomainBudget, DepthLimitIsInclusive)
  {
    /* Arrange */
    const parsed_instance instance{R"([{"a": []}])"};
    ASSERT_TRUE(instance.parsed.has_value());
    for (const std::size_t depth : {0U, 1U, 2U, 3U, 4U})
    {
      document_provider work_provider;
      validation_budget budget{};
      budget.max_depth = depth;
      /* Act */
      const auto outcome = check(instance, work_provider, budget);
      /* Expect */
      if (depth >= 3U)
      {
        EXPECT_TRUE(outcome.has_value()) << depth;
      }
      else
      {
        ASSERT_FALSE(outcome.has_value()) << depth;
        EXPECT_EQ(outcome.error_if()->limit_reason(), schema_limit_reason::max_depth) << depth;
      }
      EXPECT_EQ(work_provider.m_live_bytes, 0U);
    }
  }

  /*
   * Arrange: A scalar root with max_depth zero.
   * Act: Run the pre-pass.
   * Expect: Success, since a scalar opens no container.
   */
  TEST(P043_InstanceDomainBudget, ScalarRootPassesZeroDepth)
  {
    /* Arrange */
    const parsed_instance instance{"true"};
    ASSERT_TRUE(instance.parsed.has_value());
    document_provider work_provider;
    validation_budget budget{};
    budget.max_depth = 0U;
    /* Act */
    const auto outcome = check(instance, work_provider, budget);
    /* Expect */
    EXPECT_TRUE(outcome.has_value());
  }

  /*
   * Arrange: An instance whose pre-pass charges N steps (values plus name comparisons).
   * Act: Run it with max_work_steps N and N - 1.
   * Expect: N passes; N - 1 returns resource_limit (max_work_steps) and releases work storage.
   */
  TEST(P043_InstanceDomainBudget, WorkStepLimitIsInclusive)
  {
    /* Arrange */
    const parsed_instance instance{R"({"a": 1, "b": [2, 3], "c": null})"};
    ASSERT_TRUE(instance.parsed.has_value());
    const std::size_t steps = steps_of(instance);
    // 6 values plus 3 name comparisons in the root object.
    EXPECT_EQ(steps, 9U);
    document_provider exact_provider;
    document_provider short_provider;
    validation_budget exact{};
    exact.max_work_steps = steps;
    validation_budget short_budget{};
    short_budget.max_work_steps = steps - 1U;
    /* Act */
    const auto passed = check(instance, exact_provider, exact);
    const auto failed = check(instance, short_provider, short_budget);
    /* Expect */
    EXPECT_TRUE(passed.has_value());
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->limit_reason(), schema_limit_reason::max_work_steps);
    EXPECT_EQ(short_provider.m_live_bytes, 0U);
  }

  /**
   * @brief Cancel once the poll has been called a given number of times.
   * @par Requirements
   * P043; J5-049.
   */
  struct cancel_after
  {
      std::size_t polls{};
      std::size_t limit{};

      static bool poll(void* context) noexcept
      {
        auto& self = *static_cast<cancel_after*>(context);
        ++self.polls;
        return self.polls > self.limit;
      }
  };

  /*
   * Arrange: A nested instance and a callback that cancels on its third poll.
   * Act: Run the pre-pass.
   * Expect: cancelled after exactly three polls; work storage is released.
   */
  TEST(P043_InstanceDomainBudget, CancellationStopsAndReleasesWorkStorage)
  {
    /* Arrange */
    const parsed_instance instance{"[[1, 2], [3]]"};
    ASSERT_TRUE(instance.parsed.has_value());
    document_provider work_provider;
    cancel_after cancel{0U, 2U};
    /* Act */
    const auto outcome =
      check(instance, work_provider, validation_budget{}, cancellation_callback{&cancel_after::poll, &cancel});
    /* Expect */
    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error_if()->code(), schema_error_code::cancelled);
    EXPECT_EQ(cancel.polls, 3U);
    EXPECT_GT(work_provider.m_allocations, 0U);
    EXPECT_EQ(work_provider.m_live_bytes, 0U);
  }
} // namespace
