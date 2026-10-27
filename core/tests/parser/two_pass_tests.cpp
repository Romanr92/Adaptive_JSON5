#include "adaptive_json5/error.hpp"
#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/syntax_error_reason.hpp"
#include "parser/parser_engine.hpp"
#include "parser/parser_frame.hpp"
#include "value/value_record.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
#include <ostream>
#include <string>

namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5::detail;
  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Host provider that records the size of every allocation request.
   * @par Requirements
   * P033; J5-046; J5-048.
   * @note Only these test callbacks use malloc. Records at most 128 requests.
   */
  class recording_provider
  {
    private:
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<recording_provider*>(context);

        if (self.m_attempts < self.m_sizes.size())
        {
          self.m_sizes[self.m_attempts] = bytes;
        }

        ++self.m_attempts;

        if (alignment > alignof(std::max_align_t))
        {
          return nullptr;
        }

        void* address = std::malloc(bytes);

        if (address != nullptr)
        {
          self.m_live_bytes += bytes;
        }

        return address;
      }

      static void release(void* context, void* address, std::size_t bytes, std::size_t) noexcept
      {
        auto& self = *static_cast<recording_provider*>(context);
        self.m_live_bytes -= bytes;
        std::free(address);
      }

    public:
      std::array<std::size_t, 128> m_sizes{};
      std::size_t m_attempts{};
      std::size_t m_live_bytes{};

      [[nodiscard]] storage_callbacks callbacks() noexcept
      {
        return storage_callbacks{this, &allocate, &release};
      }

      /**
       * @brief Count recorded requests of exactly bytes.
       * @par Requirements
       * J5-046.
       */
      [[nodiscard]] std::size_t requests_of(std::size_t bytes) const noexcept
      {
        std::size_t count{};

        for (std::size_t index{}; (index < m_attempts) && (index < m_sizes.size()); ++index)
        {
          count += (m_sizes[index] == bytes) ? 1U : 0U;
        }

        return count;
      }

      /**
       * @brief Count recorded requests sized for 1 to max_count elements of element bytes.
       * @note Bounded so that, for example, 7 records (1456 bytes) are not mistaken for 13 frames.
       * @par Requirements
       * J5-046.
       */
      [[nodiscard]] std::size_t requests_for_counts(std::size_t element, std::size_t max_count) const noexcept
      {
        std::size_t count{};

        for (std::size_t elements = 1; elements <= max_count; ++elements)
        {
          count += requests_of(elements * element);
        }

        return count;
      }
  };

  /**
   * @brief Describe one document that must fail in the pre-pass.
   * @par Requirements
   * P033; P037; J5-046; J5-047.
   */
  struct prepass_failure_case
  {
      const char* name;
      const char* input;       // Complete document bytes.
      std::size_t max_depth;   // Configured nesting limit.
      bool complete;           // True for run_complete(), false for run().
      error_code code;         // Expected error category.
      std::size_t byte_offset; // Expected error offset; 0 for unlocated limit errors.
  };

  std::ostream& operator<<(std::ostream& stream, const prepass_failure_case& value)
  {
    return stream << value.name;
  }

  class PrepassFailureUnits : public ::testing::TestWithParam<prepass_failure_case>
  {
  };
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_046 =================================== */

  /* Verify records and frames are each allocated once, at exactly the counted sizes.
   * Arrange: An object with seven values and a maximum nesting of three.
   * Act: Run the engine over a recording provider.
   * Expect: One request of 7 records and one of 3 frames, and no other request sized for
   *         1 to 8 records or frames (ADR 0003, decision 7).
   */
  TEST(J5_046_TwoPassParse, RecordsAndFramesAreReservedOnceExactly)
  {
    /* Arrange */
    const std::string input = R"({"a":[1,2,{"b":null}],"c":true})";
    recording_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    {
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run_complete();
      /* Expect */
      ASSERT_TRUE(parsed.has_value());
      EXPECT_EQ(parsed.value_if()->values.size(), 7U);
    }
    EXPECT_EQ(provider.requests_of(7U * sizeof(value_record)), 1U);
    EXPECT_EQ(provider.requests_for_counts(sizeof(value_record), 8U), 1U);
    EXPECT_EQ(provider.requests_of(3U * sizeof(parser_frame)), 1U);
    EXPECT_EQ(provider.requests_for_counts(sizeof(parser_frame), 8U), 1U);
    EXPECT_EQ(provider.m_live_bytes, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a scalar root reserves one record and no frames.
   * Arrange: The document "true".
   * Act: Run the engine.
   * Expect: One record request and no request sized in frames.
   */
  TEST(J5_046_TwoPassParse, ScalarRootReservesNoFrames)
  {
    /* Arrange */
    const std::string input = "true";
    recording_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run_complete();
    /* Expect */
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(provider.m_attempts, 1U);
    EXPECT_EQ(provider.requests_of(sizeof(value_record)), 1U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify syntax, limit and trailing-content failures are found before any allocation.
   * Arrange: The case document and nesting limit.
   * Act: Run the engine, completely or as a prefix.
   * Expect: The case error at its offset and no provider request.
   */
  TEST_P(PrepassFailureUnits, FailsBeforeAnyAllocation)
  {
    /* Arrange */
    const prepass_failure_case& param = GetParam();
    const std::string input = param.input;
    recording_provider provider;
    parse_options options;
    options.limits.max_depth = param.max_depth;
    options.limits.max_string_units = 4;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = param.complete ? engine.run_complete() : engine.run();
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, param.code);
    EXPECT_EQ(parsed.error_if()->byte_offset, param.byte_offset);
    EXPECT_EQ(provider.m_attempts, 0U);
  }

  // Strings are limited to 4 units in every case.
  INSTANTIATE_TEST_SUITE_P(
    J5_046_TwoPassParse, PrepassFailureUnits,
    ::testing::Values(prepass_failure_case{"MissingValueAtEnd", R"(["a",1,)", 512, false, error_code::syntax, 7},
                      prepass_failure_case{"MissingComma", R"({"a":1 "b":2})", 512, false, error_code::syntax, 7},
                      prepass_failure_case{"MismatchedClose", R"([{"a":[1}])", 512, false, error_code::syntax, 8},
                      prepass_failure_case{"LateStringOverLimit", R"(["ab","abcde"])", 512, false,
                                           error_code::resource_limit, 11},
                      prepass_failure_case{"DepthOverLimit", "[[[1]]]", 2, false, error_code::resource_limit, 0},
                      prepass_failure_case{"TrailingContent", R"({"a":1} x)", 512, true, error_code::syntax, 8},
                      prepass_failure_case{"UnknownWordLate", R"([1,2,foo])", 512, false, error_code::syntax, 5}),
    [](const ::testing::TestParamInfo<prepass_failure_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  /* Verify nesting above the inline 512 levels uses one released block of kind bits.
   * Arrange: max_depth 600 and a document nesting 600 arrays.
   * Act: Run the engine over a recording provider.
   * Expect: Success; the first request is ceil(600 / 8) bits rounded up by one byte, and
   *         every byte is released afterwards (ADR 0003, decision 7, D1).
   */
  TEST(J5_046_TwoPassParse, DeepNestingAboveInlineBitsUsesOneReleasedBlock)
  {
    /* Arrange */
    constexpr std::size_t c_depth = 600;
    const std::string input = std::string(c_depth, '[') + std::string(c_depth, ']');
    recording_provider provider;
    parse_options options;
    options.limits.max_depth = c_depth;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    /* Act */
    {
      parser_engine engine{{input.data(), input.size()}, options, accounting};
      const auto parsed = engine.run_complete();
      /* Expect */
      ASSERT_TRUE(parsed.has_value());
      EXPECT_EQ(parsed.value_if()->values.size(), c_depth);
    }
    EXPECT_EQ(provider.m_sizes[0], (c_depth / 8U) + 1U);
    EXPECT_EQ(provider.requests_of(c_depth * sizeof(parser_frame)), 1U);
    EXPECT_EQ(provider.m_live_bytes, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the default 512 levels need no allocation for kind bits.
   * Arrange: The default max_depth and a document nesting 512 arrays, with one unclosed level.
   * Act: Run the engine.
   * Expect: A syntax error at the end of input and no provider request.
   */
  TEST(J5_046_TwoPassParse, InlineBitsCoverDefaultDepth)
  {
    /* Arrange */
    constexpr std::size_t c_depth = 512;
    const std::string input = std::string(c_depth, '[') + std::string(c_depth - 1U, ']');
    recording_provider provider;
    parse_options options;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run_complete();
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
    EXPECT_EQ(parsed.error_if()->byte_offset, input.size());
    EXPECT_EQ(provider.m_attempts, 0U);
  }
  /* ================================= End Test_J5_046 ================================= */
  /* =================================== Test_J5_032 =================================== */

  /* Verify a duplicate in an otherwise valid document is still reported with both spans.
   * Arrange: reject_duplicates and {"a":1,"a":2}.
   * Act: Run the engine completely.
   * Expect: duplicate_policy at the second name, with both name spans (ADR 0003, decision 7, C1).
   */
  TEST(J5_032_TwoPassDuplicates, DuplicateInValidDocumentKeepsBothSpans)
  {
    /* Arrange */
    const std::string input = R"({"a":1,"a":2})";
    recording_provider provider;
    parse_options options;
    options.duplicate_names = duplicate_name_policy::reject_duplicates;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run_complete();
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    const error& failure = *parsed.error_if();
    EXPECT_EQ(failure.code, error_code::duplicate_policy);
    EXPECT_EQ(failure.byte_offset, 7U);
    ASSERT_TRUE(failure.duplicate_names.has_value());
    EXPECT_EQ(failure.duplicate_names->first.byte_offset, 1U);
    EXPECT_EQ(failure.duplicate_names->first.byte_length, 3U);
    EXPECT_EQ(failure.duplicate_names->repeated.byte_offset, 7U);
    EXPECT_EQ(failure.duplicate_names->repeated.byte_length, 3U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a later syntax error takes precedence over an earlier duplicate.
   * Arrange: reject_duplicates and {"a":1,"a":2,] with a wrong closing bracket.
   * Act: Run the engine completely.
   * Expect: The syntax error at the bracket and no allocation, because the pre-pass found it
   *         (ADR 0003, decision 7, known deviation C1).
   */
  TEST(J5_032_TwoPassDuplicates, LaterSyntaxErrorTakesPrecedence)
  {
    /* Arrange */
    const std::string input = R"({"a":1,"a":2,])";
    recording_provider provider;
    parse_options options;
    options.duplicate_names = duplicate_name_policy::reject_duplicates;
    accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
    parser_engine engine{{input.data(), input.size()}, options, accounting};
    /* Act */
    const auto parsed = engine.run_complete();
    /* Expect */
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error_if()->code, error_code::syntax);
    EXPECT_EQ(parsed.error_if()->byte_offset, 13U);
    EXPECT_EQ(provider.m_attempts, 0U);
  }
  /* ================================= End Test_J5_032 ================================= */
} // namespace
