#include "../shared_infrastructure/document_provider.hpp"
#include "../shared_infrastructure/fast_path_config.hpp"
#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/json5_document.hpp"
#include "adaptive_json5/json5_value.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/parser.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
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
  /* ----------------------------------------------------------------------------------- */
  /**
   * @brief Read one shared document's numbers as the documented thread rule allows.
   * @par Requirements
   * J5-037; J5-050.
   * @note Navigation and scratch-free as_double() use no storage, so the ratio is read with
   * the plain as_double() when a fast path makes it scratch-free on this build, and with this
   * worker's provider otherwise. The long number always goes through as_double(scratch).
   * Each worker writes only its own result.
   */
  void read_shared_document(const json5_document& document, storage_callbacks scratch, double long_expected,
                            std::atomic<std::size_t>& ready, const std::atomic<bool>& start, worker_result& observed)
  {
    ready.fetch_add(1, std::memory_order_release);
    while (!start.load(std::memory_order_acquire))
    {
      std::this_thread::yield();
    }
    const json5_value root = document.root();
    for (std::size_t read{}; read < 2000U; ++read)
    {
      // The plain read is concurrent-safe only while 1.5 needs no scratch; otherwise the
      // workers would share the document's storage, so they use their own provider instead.
      const auto ratio = adaptive_json5_test::c_short_decimal_scratch_free ? root.member("ratio").as_double()
                                                                           : root.member("ratio").as_double(scratch);
      const auto port = root.member("port").as_uint64();
      const auto long_value = root.member("long").as_double(scratch);
      observed.correct = observed.correct && ratio.has_value() && (*ratio.value_if() == 1.5) && port.has_value() &&
                         (*port.value_if() == 8080U) && long_value.has_value() &&
                         (*long_value.value_if() == long_expected);
      ++observed.iterations;
    }
  }
  /*
   * Verify the json5_value thread rule: several threads read one document concurrently.
   * Arrange: One document in a caller buffer with a short ratio (scratch-free when a fast path
   *          is compiled in) and a 300-digit number; one guarded scratch provider per worker,
   *          owned by the main thread.
   * Act: Four workers navigate and read both numbers 2000 times, the long one with their own
   *      scratch provider.
   * Expect: Every read is correct, every provider is fully released, and the document's
   *         bytes and peak are unchanged. Without a race detector this shows behaviour, not
   *         the absence of a data race; the rule rests on these reads not touching shared state.
   */
  TEST(J5_050_ConcurrentReads, ThreadsWithOwnScratchReadOneDocument)
  {
    /* Arrange */
    const std::string text = "{ port: 8080, ratio: 1.5, long: 0." + std::string(300U, '1') + " }";
    alignas(std::max_align_t) static std::byte buffer[16384];
    const auto parsed = json5_document::parse({text.data(), text.size()}, buffer, sizeof(buffer));
    ASSERT_TRUE(parsed.has_value());
    const json5_document& document = *parsed.value_if();
    const auto long_expected = document.root().member("long").as_double();
    ASSERT_TRUE(long_expected.has_value());
    const std::size_t current_before = document.current_bytes();
    const std::size_t peak_before = document.peak_bytes();
    std::array<adaptive_json5_test::guard_provider, 4> providers;
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
          read_shared_document(document, providers[index].callbacks(), *long_expected.value_if(), ready, start,
                               results[index]);
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
      EXPECT_EQ(results[index].iterations, 2000u);
      EXPECT_GT(providers[index].m_attempts, 0u);
      EXPECT_EQ(providers[index].m_live_bytes, 0u);
    }
    EXPECT_EQ(document.current_bytes(), current_before);
    EXPECT_EQ(document.peak_bytes(), peak_before);
  }
  /* ================================= End Test_J5_050 ================================== */
} // namespace
