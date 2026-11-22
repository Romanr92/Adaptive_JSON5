#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/memory/resource_limits.hpp"
#include "adaptive_json5/result.hpp"
#include "adaptive_json5/schema.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"

#include <array>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
  using namespace adaptive_json5;

  // Schema outcomes travel in the library's non-throwing result type (ADR 0006 decision 3).
  static_assert(std::is_nothrow_copy_constructible_v<schema_error>);
  static_assert(std::is_nothrow_move_constructible_v<schema_error>);
  static_assert(std::is_trivially_destructible_v<schema_error>);
  static_assert(std::is_trivially_copyable_v<validation_budget>);
  static_assert(std::is_trivially_copyable_v<schema_compile_limits>);
  static_assert(std::is_trivially_copyable_v<validation_options>);
  static_assert(schema_error::limit(schema_limit_reason::max_depth).code() == schema_error_code::resource_limit);
  static_assert(matches_dialect_uri(schema_dialect::draft_07, "http://json-schema.org/draft-07/schema#"));

  /* =================================== Test_P043_SchemaError ==================================== */
  /*
   * Arrange: Every schema error code.
   * Act: Ask in which phase each can occur.
   * Expect: The reason/phase table of ADR 0006 decision 3.
   */
  TEST(P043_SchemaError, PhaseTableMatchesTheOutcomeModel)
  {
    struct row
    {
        schema_error_code code;
        bool registration;
        bool compile;
        bool validate;
    };
    constexpr std::array<row, 11> c_rows{{
      {schema_error_code::invalid_registration, true, false, false},
      {schema_error_code::invalid_schema, true, true, false},
      {schema_error_code::unresolved_reference, false, true, false},
      {schema_error_code::unsupported_dialect, false, true, false},
      {schema_error_code::unsupported_option, false, true, false},
      {schema_error_code::unsupported_instance, false, false, true},
      {schema_error_code::nonprogressing_reference, false, true, true},
      {schema_error_code::cancelled, false, false, true},
      {schema_error_code::resource_limit, true, true, true},
      {schema_error_code::allocation_failure, true, true, true},
      {schema_error_code::internal_error, true, true, true},
    }};
    for (const auto& entry : c_rows)
    {
      EXPECT_EQ(occurs_in(entry.code, schema_phase::registration), entry.registration) << static_cast<int>(entry.code);
      EXPECT_EQ(occurs_in(entry.code, schema_phase::compile), entry.compile) << static_cast<int>(entry.code);
      EXPECT_EQ(occurs_in(entry.code, schema_phase::validate), entry.validate) << static_cast<int>(entry.code);
    }
    EXPECT_FALSE(occurs_in(static_cast<schema_error_code>(0), schema_phase::compile));
  }

  /*
   * Arrange: One error per factory.
   * Act: Read each sub-reason accessor.
   * Expect: Only the accessor matching the code returns a value.
   */
  TEST(P043_SchemaError, SubReasonsBelongToTheirCode)
  {
    const auto invalid = schema_error::invalid(invalid_schema_reason::keyword_shape, source_span{4U, 2U});
    const auto instance = schema_error::unsupported(unsupported_instance_reason::duplicate_name, std::nullopt);
    const auto limit = schema_error::limit(schema_limit_reason::max_branch_evaluations);
    const auto plain = schema_error::of(schema_error_code::unresolved_reference);
    const auto registration = schema_error::registration(registration_reason::relative_uri);
    const auto domain = schema_error::invalid(invalid_schema_reason::domain);

    EXPECT_EQ(registration.code(), schema_error_code::invalid_registration);
    EXPECT_EQ(registration.registration_reason(), registration_reason::relative_uri);
    EXPECT_FALSE(registration.invalid_schema_reason().has_value());
    EXPECT_FALSE(invalid.registration_reason().has_value());
    EXPECT_EQ(domain.invalid_schema_reason(), invalid_schema_reason::domain);

    EXPECT_EQ(invalid.code(), schema_error_code::invalid_schema);
    EXPECT_EQ(invalid.invalid_schema_reason(), invalid_schema_reason::keyword_shape);
    EXPECT_FALSE(invalid.instance_reason().has_value());
    EXPECT_FALSE(invalid.limit_reason().has_value());
    ASSERT_TRUE(invalid.source_range().has_value());
    EXPECT_EQ(invalid.source_range()->byte_offset, 4U);
    EXPECT_EQ(invalid.source_range()->byte_length, 2U);

    EXPECT_EQ(instance.instance_reason(), unsupported_instance_reason::duplicate_name);
    EXPECT_FALSE(instance.invalid_schema_reason().has_value());
    EXPECT_FALSE(instance.source_range().has_value());

    EXPECT_EQ(limit.limit_reason(), schema_limit_reason::max_branch_evaluations);
    EXPECT_FALSE(limit.instance_reason().has_value());

    EXPECT_EQ(plain.code(), schema_error_code::unresolved_reference);
    EXPECT_FALSE(plain.invalid_schema_reason().has_value());
    EXPECT_FALSE(plain.instance_reason().has_value());
    EXPECT_FALSE(plain.limit_reason().has_value());
    EXPECT_FALSE(plain.source_range().has_value());
    EXPECT_FALSE(plain.parser_error().has_value());
  }

  /*
   * Arrange: A parser syntax error with a source range.
   * Act: Wrap it as a schema syntax failure.
   * Expect: invalid_schema (syntax) with the parser error and its range unchanged.
   */
  TEST(P043_SchemaError, SyntaxKeepsTheParserError)
  {
    const auto parser_failure =
      error::syntax_at_range(3U, source_span{3U, 1U}, syntax_error_reason::unexpected_character);
    const auto failure = schema_error::syntax(parser_failure);

    EXPECT_EQ(failure.code(), schema_error_code::invalid_schema);
    EXPECT_EQ(failure.invalid_schema_reason(), invalid_schema_reason::syntax);
    ASSERT_TRUE(failure.parser_error().has_value());
    EXPECT_EQ(failure.parser_error()->code(), error_code::syntax);
    EXPECT_EQ(failure.parser_error()->syntax_reason(), syntax_error_reason::unexpected_character);
    EXPECT_EQ(failure.parser_error()->byte_offset(), 3U);
    ASSERT_TRUE(failure.source_range().has_value());
    EXPECT_EQ(failure.source_range()->byte_offset, 3U);
  }

  /*
   * Arrange: Storage and access failures from the parser layer.
   * Act: Translate them.
   * Expect: Same-named codes map one to one; anything else is internal_error; the original is kept.
   */
  TEST(P043_SchemaError, StorageFailuresReuseParserNames)
  {
    const auto allocation = schema_error::from_storage(error::without_location(error_code::allocation_failure));
    const auto ceiling = schema_error::from_storage(error::limit_without_location(resource_limit_reason::owned_bytes));
    const auto other = schema_error::from_storage(error::access_failure(access_error_reason::wrong_type));

    EXPECT_EQ(allocation.code(), schema_error_code::allocation_failure);
    EXPECT_EQ(ceiling.code(), schema_error_code::resource_limit);
    EXPECT_FALSE(ceiling.limit_reason().has_value());
    ASSERT_TRUE(ceiling.parser_error().has_value());
    EXPECT_EQ(ceiling.parser_error()->limit_reason(), resource_limit_reason::owned_bytes);
    EXPECT_EQ(other.code(), schema_error_code::internal_error);
    ASSERT_TRUE(other.parser_error().has_value());
    EXPECT_EQ(other.parser_error()->code(), error_code::wrong_type_or_access);
  }

  /*
   * Arrange: A schema error inside the library result type.
   * Act: Build success and failure results.
   * Expect: Both are inspectable without throwing; an assertion verdict is not modelled as an error.
   */
  TEST(P043_SchemaError, TravelsInTheLibraryResult)
  {
    const auto ok = result<void, schema_error>::success();
    const auto failed = result<void, schema_error>::failure(schema_error::of(schema_error_code::cancelled));

    EXPECT_TRUE(ok.has_value());
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code(), schema_error_code::cancelled);
  }

  /* =================================== Test_P043_SchemaDialect ==================================== */
  /*
   * Arrange: Draft-07 spellings and near misses.
   * Act: Match each against draft_07.
   * Expect: Only the two exact Draft-07 forms match; no normalization.
   */
  TEST(P043_SchemaDialect, MatchesOnlyExactDraft07Uris)
  {
    EXPECT_TRUE(matches_dialect_uri(schema_dialect::draft_07, "http://json-schema.org/draft-07/schema#"));
    EXPECT_TRUE(matches_dialect_uri(schema_dialect::draft_07, "http://json-schema.org/draft-07/schema"));
    EXPECT_FALSE(matches_dialect_uri(schema_dialect::draft_07, "https://json-schema.org/draft-07/schema#"));
    EXPECT_FALSE(matches_dialect_uri(schema_dialect::draft_07, "HTTP://json-schema.org/draft-07/schema#"));
    EXPECT_FALSE(matches_dialect_uri(schema_dialect::draft_07, "http://json-schema.org/draft-06/schema#"));
    EXPECT_FALSE(matches_dialect_uri(schema_dialect::draft_07, "http://json-schema.org/draft-07/schema##"));
    EXPECT_FALSE(matches_dialect_uri(schema_dialect::draft_07, ""));
    EXPECT_FALSE(matches_dialect_uri(static_cast<schema_dialect>(0), "http://json-schema.org/draft-07/schema#"));
  }

  /* =================================== Test_P043_SchemaBudget ==================================== */
  /*
   * Arrange: Default-constructed budgets and options.
   * Act: Read every field.
   * Expect: The defaults of ADR 0006 decisions 5, 7 and 12.
   */
  TEST(P043_SchemaBudget, DefaultsMatchTheDecisions)
  {
    const validation_budget budget{};
    EXPECT_EQ(budget.max_depth, resource_limits{}.max_depth);
    EXPECT_EQ(budget.max_depth, 512U);
    EXPECT_EQ(budget.max_reference_steps, 10'000U);
    EXPECT_EQ(budget.max_branch_evaluations, 100'000U);
    EXPECT_EQ(budget.max_equality_steps, 100'000U);
    EXPECT_EQ(budget.max_errors, 100U);
    EXPECT_EQ(budget.max_work_steps, 1'000'000U);

    const schema_compile_limits limits{};
    EXPECT_EQ(limits.max_schema_nodes, 100'000U);
    EXPECT_EQ(limits.max_compile_steps, 1'000'000U);

    const validation_options options{};
    EXPECT_EQ(options.mode, evaluation_mode::fail_fast);
    EXPECT_EQ(options.cancellation.poll, nullptr);
    EXPECT_EQ(options.cancellation.context, nullptr);
    EXPECT_EQ(options.work_storage.allocate, nullptr);
    EXPECT_EQ(options.work_storage.deallocate, nullptr);
  }
} // namespace
