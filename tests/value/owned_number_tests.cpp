// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/value/owned_number.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  /* ===================================== Fixtures ===================================== */
  /**
   * @brief Host storage with per-request failure injection and release validation.
   * @par Requirements
   * P029; J5-040; J5-046; J5-051.
   * @note Fixed records track allocations; only these test callbacks use malloc.
   */
  class lexer_provider
  {
    private:
      struct allocation_record
      {
          void* address{};
          std::size_t bytes{};
          std::size_t alignment{};
      };
      std::array<allocation_record, 16> m_records{};

      /**
       * @brief Allocate or inject failure on the selected request.
       * @par Requirements
       * P029; J5-046.
       */
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<lexer_provider*>(context);
        ++self.m_attempts;
        if (self.m_attempts == self.m_fail_on || alignment > alignof(std::max_align_t))
        {
          return nullptr;
        }
        for (auto& record : self.m_records)
        {
          if (record.address == nullptr)
          {
            record.address = std::malloc(bytes);
            if (record.address != nullptr)
            {
              record.bytes = bytes;
              record.alignment = alignment;
              ++self.m_allocations;
              self.m_live_bytes += bytes;
            }
            return record.address;
          }
        }
        return nullptr;
      }

      /**
       * @brief Verify and release one original allocation exactly once.
       * @par Requirements
       * P029; J5-040; J5-051.
       */
      static void release(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<lexer_provider*>(context);
        for (auto& record : self.m_records)
        {
          if (record.address == address && address != nullptr)
          {
            EXPECT_EQ(record.bytes, bytes);
            EXPECT_EQ(record.alignment, alignment);
            self.m_live_bytes -= record.bytes;
            ++self.m_releases;
            std::free(address);
            record = {};
            return;
          }
        }
        ADD_FAILURE() << "Unknown allocation or duplicate release";
      }

    public:
      std::size_t m_attempts{};
      std::size_t m_fail_on{};
      std::size_t m_allocations{};
      std::size_t m_releases{};
      std::size_t m_live_bytes{};

      /**
       * @brief Confirm owners released all allocations before provider destruction.
       * @par Requirements
       * P029; J5-040.
       */
      ~lexer_provider()
      {
        EXPECT_EQ(m_live_bytes, 0u);
        EXPECT_EQ(m_allocations, m_releases);
        for (const auto& record : m_records)
        {
          // Reclaim leaked test storage after reporting a production ownership failure.
          std::free(record.address);
        }
      }

      /**
       * @brief Borrow the test provider through the public callback interface.
       * @par Requirements
       * P029; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &release};
      }
  };

  /* =================================== End Fixtures =================================== */
  /* ==================================== Test_J5_034 ==================================== */
  static_assert(std::is_nothrow_move_constructible_v<owned_number>);
  static_assert(std::is_nothrow_destructible_v<owned_number>);
  static_assert(!std::is_copy_constructible_v<owned_number>);
  static_assert(!std::is_move_assignable_v<owned_number>);
  static_assert(noexcept(owned_number::copy_spelling({}, syntax_mode::json5, {}, 0)));
  static_assert(noexcept(std::declval<const owned_number&>().spelling()));
  static_assert(noexcept(std::declval<const owned_number&>().kind()));
  /**
   * @brief Exact spelling and independent numeric-family expectations.
   * @par Requirements
   * P029; J5-034; J5-038.
   */
  struct owned_case
  {
      const char* name;          // Input case label.
      std::string text;          // Input numeric bytes.
      number_kind kind;          // Expected spelling family in JSON5.
      bool strict_ok;            // Expected strict JSON acceptance.
      std::size_t strict_offset; // Expected strict syntax error byte offset.
  };
  class J5_034_OwnedNumber : public testing::TestWithParam<std::tuple<owned_case, syntax_mode>>
  {
  };
  /* Retain original numeric bytes with one exact-size allocation.
   * Arrange: Mutable input, explicit storage and a limit equal to its byte length.
   * Act: Validate and copy in each syntax mode.
   * Expect: Exact spelling/kind and independent storage, or strict rejection before allocation.
   */
  TEST_P(J5_034_OwnedNumber, RetainsExactSpellingAndKind)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    std::string input = item.text;
    lexer_provider provider;
    /* Act */
    auto actual = owned_number::copy_spelling({input.data(), input.size()}, mode, provider.callbacks(), input.size());
    /* Expect */
    if (mode == syntax_mode::strict_json && !item.strict_ok)
    {
      ASSERT_FALSE(actual.has_value());
      EXPECT_EQ(actual.error_if()->code, error_code::syntax);
      EXPECT_TRUE(actual.error_if()->has_byte_offset);
      EXPECT_EQ(actual.error_if()->byte_offset, item.strict_offset);
      EXPECT_EQ(provider.m_attempts, 0u);
      return;
    }
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(actual.value_if()->kind(), item.kind);
    const auto view = actual.value_if()->spelling();
    EXPECT_EQ(view.size, item.text.size());
    ASSERT_NE(view.data, nullptr);
    EXPECT_NE(view.data, input.data());
    EXPECT_EQ(std::string(view.data, view.size), item.text);
    EXPECT_EQ(provider.m_attempts, 1u);
    EXPECT_EQ(provider.m_live_bytes, item.text.size());
    input.assign(input.size(), '?');
    EXPECT_EQ(std::string(view.data, view.size), item.text);
  }
  INSTANTIATE_TEST_SUITE_P(
    P029, J5_034_OwnedNumber,
    testing::Combine(
      testing::Values(owned_case{"Zero", "0", number_kind::decimal, true, 0},
                      owned_case{"NegativeZero", "-0", number_kind::decimal, true, 0},
                      owned_case{"FractionZero", "-0.000", number_kind::decimal, true, 0},
                      owned_case{"ExponentZero", "-0e+9999", number_kind::decimal, true, 0},
                      owned_case{"FractionSpelling", "1.2300E+003", number_kind::decimal, true, 0},
                      owned_case{"BeyondDouble", "9007199254740993", number_kind::decimal, true, 0},
                      owned_case{"BeyondUnsigned", "18446744073709551616", number_kind::decimal, true, 0},
                      owned_case{"BeyondSigned", "-9223372036854775809", number_kind::decimal, true, 0},
                      owned_case{"HugeFiniteExponent", "1e999999999999999999999", number_kind::decimal, true, 0},
                      owned_case{"TinyFiniteExponent", "1e-99999999999999999999", number_kind::decimal, true, 0},
                      owned_case{"Plus", "+1", number_kind::decimal, false, 0},
                      owned_case{"LeadingPoint", ".5", number_kind::decimal, false, 0},
                      owned_case{"TrailingPoint", "5.", number_kind::decimal, false, 2},
                      owned_case{"HexNegativeZero", "-0X000", number_kind::hexadecimal, false, 0},
                      owned_case{"HugeHex", "0x10000000000000001", number_kind::hexadecimal, false, 0},
                      owned_case{"Infinity", "Infinity", number_kind::infinity, false, 0},
                      owned_case{"PlusInfinity", "+Infinity", number_kind::infinity, false, 0},
                      owned_case{"MinusInfinity", "-Infinity", number_kind::infinity, false, 0},
                      owned_case{"NaN", "NaN", number_kind::nan, false, 0},
                      owned_case{"PlusNaN", "+NaN", number_kind::nan, false, 0},
                      owned_case{"MinusNaN", "-NaN", number_kind::nan, false, 0}),
      testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<owned_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ----------------------------------------------------------------------------------- */
  /* Preserve long integer, fraction, exponent and hexadecimal text without conversion.
   * Arrange: Literal digit runs beyond machine numeric capacities.
   * Act: Copy with exact component byte limits.
   * Expect: Unmodified owned text and explicit finite numeric families.
   */
  TEST(J5_034_OwnedNumberRange, PreservesLargeSpellingsWithoutConversion)
  {
    /* Arrange */
    const std::pair<std::string, number_kind> inputs[] = {{std::string(4096, '9'), number_kind::decimal},
                                                          {"0." + std::string(4096, '0') + "1", number_kind::decimal},
                                                          {"1e" + std::string(4096, '9'), number_kind::decimal},
                                                          {"-0e-" + std::string(4096, '9'), number_kind::decimal},
                                                          {"+0X" + std::string(4096, 'F'), number_kind::hexadecimal}};
    for (const auto& item : inputs)
    {
      lexer_provider provider;
      /* Act */
      auto actual = owned_number::copy_spelling({item.first.data(), item.first.size()}, syntax_mode::json5,
                                                provider.callbacks(), item.first.size());
      /* Expect */
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.value_if()->kind(), item.second);
      const auto view = actual.value_if()->spelling();
      EXPECT_EQ(std::string(view.data, view.size), item.first);
      EXPECT_EQ(provider.m_live_bytes, item.first.size());
    }
  }
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief Explicit invalid complete numeric-spelling expectations shared by both modes.
   * @par Requirements
   * P029; J5-034.
   */
  struct invalid_case
  {
      const char* name;   // Input case label.
      std::string text;   // Input bytes including invalid surrounding content.
      error_code code;    // Expected error category.
      std::size_t offset; // Expected error byte offset relative to input.
  };
  class J5_034_InvalidOwnedNumber : public testing::TestWithParam<std::tuple<invalid_case, syntax_mode>>
  {
  };
  /* Reject no-match, malformed spellings and surrounding content before allocation.
   * Arrange: Caller storage and complete invalid byte buffers.
   * Act: Attempt owned numeric construction in both modes.
   * Expect: Exact error category/offset, no allocation and unchanged input.
   */
  TEST_P(J5_034_InvalidOwnedNumber, RejectsBeforeAllocation)
  {
    /* Arrange */
    const auto& item = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    lexer_provider provider;
    const auto original = item.text;
    /* Act */
    auto actual = owned_number::copy_spelling({item.text.data(), item.text.size()}, mode, provider.callbacks(), 64);
    /* Expect */
    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error_if()->code, item.code);
    EXPECT_TRUE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, item.offset);
    EXPECT_EQ(provider.m_attempts, 0u);
    EXPECT_EQ(item.text, original);
  }
  INSTANTIATE_TEST_SUITE_P(
    P029, J5_034_InvalidOwnedNumber,
    testing::Combine(testing::Values(invalid_case{"Empty", "", error_code::syntax, 0},
                                     invalid_case{"Name", "true", error_code::syntax, 0},
                                     invalid_case{"LeadingSpace", " 1", error_code::syntax, 0},
                                     invalid_case{"TrailingSpace", "1 ", error_code::syntax, 1},
                                     invalid_case{"ExtraToken", "1 2", error_code::syntax, 1},
                                     invalid_case{"Punctuation", "1,", error_code::syntax, 1},
                                     invalid_case{"LeadingZero", "01", error_code::syntax, 1},
                                     invalid_case{"IncompleteExponent", "1e+", error_code::syntax, 3},
                                     invalid_case{"Suffix", "1foo", error_code::syntax, 1},
                                     invalid_case{"Binary", "0b1", error_code::syntax, 1},
                                     invalid_case{"Nul", std::string("1\0", 2), error_code::syntax, 1},
                                     invalid_case{"Encoding", std::string("1") + "\x80", error_code::encoding, 1}),
                     testing::Values(syntax_mode::json5, syntax_mode::strict_json)),
    [](const testing::TestParamInfo<std::tuple<invalid_case, syntax_mode>>& info)
    {
      return std::string(std::get<0>(info.param).name) +
             (std::get<1>(info.param) == syntax_mode::json5 ? "Json5" : "StrictJson");
    });
  /* ================================== End Test_J5_034 ================================== */
  /* ==================================== Test_J5_040 ==================================== */
  /* Transfer ownership across repeated moves without invalidating the retained view.
   * Arrange: Copy a spelling from mutable temporary source storage.
   * Act: Move the owner twice, overwrite and destroy input, and inspect both moved-from owners.
   * Expect: Empty moved-from spellings, retained metadata/view and exactly one final release.
   */
  TEST(J5_040_OwnedNumber, PreservesViewsAcrossMovesAndInputDestruction)
  {
    /* Arrange */
    lexer_provider provider;
    std::optional<owned_number> retained;
    input_view borrowed{};
    {
      std::string input = "-0.00e+10";
      auto actual = owned_number::copy_spelling({input.data(), input.size()}, syntax_mode::json5, provider.callbacks(),
                                                input.size());
      ASSERT_TRUE(actual.has_value());
      borrowed = actual.value_if()->spelling();
      /* Act */
      owned_number intermediate{std::move(*actual.value_if())};
      retained.emplace(std::move(intermediate));
      input.assign(input.size(), '?');
      /* Expect */
      EXPECT_EQ(actual.value_if()->spelling().size, 0u);
      EXPECT_EQ(actual.value_if()->spelling().data, nullptr);
      EXPECT_EQ(intermediate.spelling().size, 0u);
      EXPECT_EQ(intermediate.spelling().data, nullptr);
      EXPECT_EQ(actual.value_if()->kind(), number_kind::decimal);
      EXPECT_EQ(intermediate.kind(), number_kind::decimal);
      EXPECT_EQ(provider.m_releases, 0u);
    }
    EXPECT_EQ(std::string(borrowed.data, borrowed.size), "-0.00e+10");
    EXPECT_EQ(retained->spelling().data, borrowed.data);
    retained.reset();
    EXPECT_EQ(provider.m_allocations, 1u);
    EXPECT_EQ(provider.m_releases, 1u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  /* ================================== End Test_J5_040 ================================== */
  /* ==================================== Test_J5_046 ==================================== */
  /* Fail the sole allocation and retry while an independent successful owner stays alive.
   * Arrange: Retain one number, then arm failure on the next provider request.
   * Act: Copy a second spelling, fail, disarm failure and retry.
   * Expect: Explicit allocation error without disturbing retained content; subsequent success.
   */
  TEST(J5_046_OwnedNumber, ReportsAllocationFailureAndSupportsReuse)
  {
    /* Arrange */
    lexer_provider provider;
    const std::string input = "9007199254740993";
    auto retained = owned_number::copy_spelling({"-0", 2}, syntax_mode::json5, provider.callbacks(), 2);
    ASSERT_TRUE(retained.has_value());
    provider.m_fail_on = provider.m_attempts + 1;
    const auto before = provider.m_live_bytes;
    /* Act */
    auto failed =
      owned_number::copy_spelling({input.data(), input.size()}, syntax_mode::json5, provider.callbacks(), input.size());
    /* Expect */
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error_if()->code, error_code::allocation_failure);
    EXPECT_FALSE(failed.error_if()->has_byte_offset);
    EXPECT_EQ(provider.m_live_bytes, before);
    const auto view = retained.value_if()->spelling();
    EXPECT_EQ(std::string(view.data, view.size), "-0");
    provider.m_fail_on = 0;
    auto retry =
      owned_number::copy_spelling({input.data(), input.size()}, syntax_mode::json5, provider.callbacks(), input.size());
    ASSERT_TRUE(retry.has_value());
    const auto copied = retry.value_if()->spelling();
    EXPECT_EQ(std::string(copied.data, copied.size), input);
    auto missing = owned_number::copy_spelling({"1", 1}, syntax_mode::json5, {}, 1);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error_if()->code, error_code::allocation_failure);
  }
  /* ----------------------------------------------------------------------------------- */
  /* Validate pointer/length and enforce size bounds before syntax scanning or allocation.
   * Arrange: Invalid null input and lengths below/equal/above the configured byte limit.
   * Act: Construct owned numbers with caller storage.
   * Expect: Explicit access/limit errors or exact-limit success with no rejected allocations.
   */
  TEST(J5_046_OwnedNumber, ChecksInputAndByteLimitsBeforeAllocation)
  {
    /* Arrange */
    lexer_provider provider;
    const std::string input = "123";
    /* Act */
    auto invalid = owned_number::copy_spelling({nullptr, 1}, syntax_mode::json5, provider.callbacks(), 8);
    auto empty = owned_number::copy_spelling({nullptr, 0}, syntax_mode::json5, provider.callbacks(), 0);
    auto short_limit =
      owned_number::copy_spelling({input.data(), input.size()}, syntax_mode::json5, provider.callbacks(), 2);
    auto zero_limit =
      owned_number::copy_spelling({input.data(), input.size()}, syntax_mode::json5, provider.callbacks(), 0);
    /* Expect */
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_FALSE(invalid.error_if()->has_byte_offset);
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error_if()->code, error_code::syntax);
    EXPECT_TRUE(empty.error_if()->has_byte_offset);
    EXPECT_EQ(empty.error_if()->byte_offset, 0u);
    for (const auto* result : {&short_limit, &zero_limit})
    {
      ASSERT_FALSE(result->has_value());
      EXPECT_EQ(result->error_if()->code, error_code::resource_limit);
      EXPECT_TRUE(result->error_if()->has_byte_offset);
      EXPECT_EQ(result->error_if()->byte_offset, 0u);
    }
    EXPECT_EQ(provider.m_attempts, 0u);
    for (const std::size_t limit : {3u, 4u})
    {
      auto actual =
        owned_number::copy_spelling({input.data(), input.size()}, syntax_mode::json5, provider.callbacks(), limit);
      ASSERT_TRUE(actual.has_value());
      EXPECT_EQ(actual.value_if()->spelling().size, 3u);
    }
    // A size rejection must precede dereferencing the nonempty input view.
    const char invalid_syntax = '?';
    auto limited = owned_number::copy_spelling({&invalid_syntax, 1}, syntax_mode::json5, provider.callbacks(), 0);
    ASSERT_FALSE(limited.has_value());
    EXPECT_EQ(limited.error_if()->code, error_code::resource_limit);
  }
  /* ================================== End Test_J5_046 ================================== */
} // namespace
