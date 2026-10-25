#include "../support/document_provider.hpp"
#include "adaptive_json5/parser.hpp"

#include <array>
#include <atomic>
#include <gtest/gtest.h>
#include <string_view>
#include <thread>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_050 ==================================== */
  /**
   * @brief Collect one worker's observations without sharing parser state or GoogleTest assertions.
   * @par Requirements
   * P041; J5-050; J5-051; J5-052.
   */
  struct worker_result
  {
      bool correct = true;      // Expected all parse outcomes and retained values match.
      std::size_t iterations{}; // Observed completed success/failure/reuse cycles.
  };

  /**
   * @brief Exercise independent parsing and storage while other workers do the same.
   * @par Requirements
   * P041; J5-040; J5-046; J5-050; J5-051; J5-052.
   * @note Each worker writes only its own result; input and baseline options are immutable.
   */
  void run_worker(const parse_options& baseline, std::string_view valid, std::atomic<std::size_t>& ready,
                  const std::atomic<bool>& start, worker_result& observed)
  {
    document_provider provider;
    accounted_storage accounting{provider.callbacks(), baseline.limits.max_owned_bytes};
    ready.fetch_add(1, std::memory_order_release);
    while (!start.load(std::memory_order_acquire))
    {
      std::this_thread::yield();
    }
    constexpr std::string_view malformed{"[true,] trailing"};
    constexpr std::string_view invalid_encoding{"[\"\xff\"]"};
    for (std::size_t iteration{}; iteration < 64; ++iteration)
    {
      {
        // Keep a document alive while the same worker performs failing calls and reuses accounting.
        const auto retained = parse_document({valid.data(), valid.size()}, baseline, accounting);
        if (!retained.has_value())
        {
          observed.correct = false;
          return;
        }
        const auto syntax = parse_document({malformed.data(), malformed.size()}, baseline, accounting);
        const auto encoding = parse_document({invalid_encoding.data(), invalid_encoding.size()}, baseline, accounting);
        parse_options limited = baseline;
        limited.limits.max_nodes = 0;
        const auto resource = parse_document({valid.data(), valid.size()}, limited, accounting);
        provider.m_fail_on = provider.m_attempts + 1;
        const auto allocation = parse_document({valid.data(), valid.size()}, baseline, accounting);
        provider.m_fail_on = 0;
        const auto retried = parse_document({valid.data(), valid.size()}, baseline, accounting);
        observed.correct = observed.correct && !syntax.has_value() && !encoding.has_value() && !resource.has_value() &&
                           !allocation.has_value() && retried.has_value();
        if (!observed.correct)
        {
          return;
        }
        observed.correct = syntax.error_if()->code == error_code::syntax &&
                           encoding.error_if()->code == error_code::encoding &&
                           resource.error_if()->code == error_code::resource_limit &&
                           allocation.error_if()->code == error_code::allocation_failure;
        const auto root = retained.value_if()->values.root();
        if (!root.has_value())
        {
          observed.correct = false;
          return;
        }
        const auto count = root.value_if()->container_size();
        const auto member = root.value_if()->member_at(0);
        if (!count.has_value() || !member.has_value())
        {
          observed.correct = false;
          return;
        }
        const auto boolean = member.value_if()->as_boolean();
        observed.correct = observed.correct && *count.value_if() == 2 && boolean.has_value() && *boolean.value_if() &&
                           retained.value_if()->warnings.size() == 1 && retried.value_if()->warnings.size() == 1;
      }
      observed.correct = observed.correct && provider.m_live_bytes == 0 && accounting.current_bytes() == 0;
      ++observed.iterations;
    }
  }

  class J5_050_ConcurrentParsing : public testing::TestWithParam<syntax_mode>
  {
  };
  /*
   * Arrange: Four workers sharing immutable source/options and owning separate storage/accounting.
   * Act: Start together, then repeatedly parse successes and syntax/encoding/resource/allocation failures.
   * Expect: Stable results, preserved live documents, successful reuse and complete storage release.
   */
  TEST_P(J5_050_ConcurrentParsing, IsolatesSuccessFailuresAndLiveDocuments)
  {
    /* Arrange */
    constexpr std::string_view input{R"({"flag":true,"text":"\uD800"})"};
    const parse_options options{GetParam()};
    std::atomic<std::size_t> ready{};
    std::atomic<bool> start{};
    std::array<worker_result, 4> results{};
    std::array<std::thread, 4> workers;
    /* Act */
    for (std::size_t index{}; index < workers.size(); ++index)
    {
      workers[index] = std::thread(
        [&, index]
        {
          run_worker(options, input, ready, start, results[index]);
        });
    }
    while (ready.load(std::memory_order_acquire) != workers.size())
    {
      std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (auto& worker : workers)
    {
      worker.join();
    }
    /* Expect */
    for (std::size_t index{}; index < results.size(); ++index)
    {
      SCOPED_TRACE(index);
      EXPECT_TRUE(results[index].correct);
      EXPECT_EQ(results[index].iterations, 64u);
    }
  }
  INSTANTIATE_TEST_SUITE_P(P041, J5_050_ConcurrentParsing,
                           testing::Values(syntax_mode::strict_json, syntax_mode::json5));
  /* ================================= End Test_J5_050 ================================== */
} // namespace
