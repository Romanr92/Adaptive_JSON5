#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/memory/storage.hpp"
#include "adaptive_json5/parser.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace
{
  using namespace adaptive_json5;
  namespace fs = std::filesystem;
  /* ==================================== Fixtures ===================================== */
  /**
   * @brief Host storage that counts live bytes so every case can check full release.
   * @par Requirements
   * P040; J5-095; J5-098.
   * @note Unlike the unit providers it has no fixed record limit, because corpus documents vary in size.
   */
  class counting_provider
  {
    private:
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<counting_provider*>(context);
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
        auto& self = *static_cast<counting_provider*>(context);
        self.m_live_bytes -= bytes;
        std::free(address);
      }

    public:
      std::size_t m_live_bytes{};

      [[nodiscard]] storage_callbacks callbacks() noexcept
      {
        return storage_callbacks{this, &allocate, &release};
      }
  };

  /**
   * @brief Expected outcome of one fixture file in one syntax mode.
   * @par Requirements
   * P040; J5-095.
   */
  enum class expectation
  {
    accept,
    reject,
    either // Implementation-defined: only clean completion is checked.
  };

  /**
   * @brief Pin project choices for every implementation-defined fixture without changing upstream labels.
   * @par Requirements
   * P041; J5-018; J5-025; J5-027; J5-095.
   */
  struct project_decision
  {
      const char* file;                   // Input pinned fixture basename.
      bool strict;                        // Expected strict acceptance.
      bool json5;                         // Expected JSON5 acceptance.
      error_code code{};                  // Expected category on rejection.
      std::size_t offset{};               // Expected rejection offset in bytes.
      std::size_t warnings{};             // Expected unpaired-surrogate warning count.
      std::array<std::size_t, 2> spans{}; // Expected warning offsets in bytes; lengths are six.
  };
  constexpr project_decision c_project_decisions[] = {
    {"i_number_double_huge_neg_exp.json", true, true},
    {"i_number_huge_exp.json", true, true},
    {"i_number_neg_int_huge_exp.json", true, true},
    {"i_number_pos_double_huge_exp.json", true, true},
    {"i_number_real_neg_overflow.json", true, true},
    {"i_number_real_pos_overflow.json", true, true},
    {"i_number_real_underflow.json", true, true},
    {"i_number_too_big_neg_int.json", true, true},
    {"i_number_too_big_pos_int.json", true, true},
    {"i_number_very_big_negative_int.json", true, true},
    {"i_object_key_lone_2nd_surrogate.json", true, true, {}, 0, 1, {2}},
    {"i_string_1st_surrogate_but_2nd_missing.json", true, true, {}, 0, 1, {2}},
    {"i_string_1st_valid_surrogate_2nd_invalid.json", true, true, {}, 0, 1, {2}},
    {"i_string_incomplete_surrogates_escape_valid.json", true, true, {}, 0, 2, {2, 8}},
    {"i_string_incomplete_surrogate_and_escape_valid.json", true, true, {}, 0, 1, {2}},
    {"i_string_incomplete_surrogate_pair.json", true, true, {}, 0, 1, {2}},
    {"i_string_invalid_lonely_surrogate.json", true, true, {}, 0, 1, {2}},
    {"i_string_invalid_surrogate.json", true, true, {}, 0, 1, {2}},
    {"i_string_inverted_surrogates_U+1D11E.json", true, true, {}, 0, 2, {2, 8}},
    {"i_string_lone_second_surrogate.json", true, true, {}, 0, 1, {2}},
    {"i_string_invalid_utf-8.json", false, false, error_code::encoding, 2},
    {"i_string_iso_latin_1.json", false, false, error_code::encoding, 2},
    {"i_string_lone_utf8_continuation_byte.json", false, false, error_code::encoding, 2},
    {"i_string_not_in_unicode_range.json", false, false, error_code::encoding, 2},
    {"i_string_overlong_sequence_2_bytes.json", false, false, error_code::encoding, 2},
    {"i_string_overlong_sequence_6_bytes.json", false, false, error_code::encoding, 2},
    {"i_string_overlong_sequence_6_bytes_null.json", false, false, error_code::encoding, 2},
    {"i_string_truncated-utf-8.json", false, false, error_code::encoding, 2},
    {"i_string_UTF-8_invalid_sequence.json", false, false, error_code::encoding, 7},
    {"i_string_UTF8_surrogate_U+D800.json", false, false, error_code::encoding, 2},
    {"i_string_UTF-16LE_with_BOM.json", false, false, error_code::encoding, 0},
    {"i_string_utf16BE_no_BOM.json", false, false, error_code::syntax, 0},
    {"i_string_utf16LE_no_BOM.json", false, false, error_code::syntax, 1},
    {"i_structure_500_nested_arrays.json", true, true},
    {"i_structure_UTF-8_BOM_empty_object.json", false, true, error_code::syntax, 0},
  };

  /**
   * @brief Return the documented project outcome for an i_ fixture, or either if not yet classified.
   * @par Requirements
   * P041; J5-095.
   * @note The inventory unit fails if a fixture lacks a classification.
   */
  expectation project_outcome(const std::string& file, syntax_mode mode)
  {
    for (const auto& decision : c_project_decisions)
    {
      if (file == decision.file)
      {
        return (mode == syntax_mode::strict_json ? decision.strict : decision.json5) ? expectation::accept
                                                                                     : expectation::reject;
      }
    }
    return expectation::either;
  }

  /**
   * @brief One fixture file parsed in one syntax mode.
   * @par Requirements
   * P040; J5-095.
   */
  struct corpus_case
  {
      std::string name;     // Unique GoogleTest-safe identifier.
      std::string path;     // Absolute fixture path.
      syntax_mode mode;     // Input language selection.
      expectation expected; // Required outcome.
  };

  /**
   * @brief Read a whole file as bytes, failing the test when it cannot be read.
   * @par Requirements
   * P040; J5-095.
   */
  std::string read_bytes(const std::string& path)
  {
    std::ifstream stream{path, std::ios::binary};
    EXPECT_TRUE(stream.good()) << path;
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  }

  /**
   * @brief Build a unique GoogleTest parameter name from a relative fixture path.
   * @par Requirements
   * P040; J5-095.
   */
  std::string case_name(const std::string& prefix, const std::string& relative, std::set<std::string>& used)
  {
    std::string name = prefix;
    for (const char c : relative)
    {
      name += (std::isalnum(static_cast<unsigned char>(c)) != 0) ? c : '_';
    }
    std::string unique = name;
    for (std::size_t suffix = 2U; !used.insert(unique).second; ++suffix)
    {
      unique = name + "_" + std::to_string(suffix);
    }
    return unique;
  }

  /**
   * @brief List regular files below root in a stable order, without throwing.
   * @par Requirements
   * P040; J5-095.
   */
  std::vector<fs::path> sorted_files(const fs::path& root)
  {
    std::vector<fs::path> files;
    std::error_code failure;
    for (fs::recursive_directory_iterator it{root, failure}, end; !failure && (it != end); it.increment(failure))
    {
      if (it->is_regular_file(failure))
      {
        files.push_back(it->path());
      }
    }
    std::sort(files.begin(), files.end());
    return files;
  }

  /**
   * @brief Upstream labels that contradict the specification, with the outcome the specification requires.
   * @par Requirements
   * P040; J5-095.
   * @note Record each deviation in tests/fixtures/third_party/README.md as well.
   */
  struct label_override
  {
      const char* relative; // Fixture path relative to its corpus root.
      syntax_mode mode;     // Mode whose upstream expectation is wrong.
      expectation expected; // Outcome required by the specification.
  };
  // A .json file is labeled valid JSON, but it contains a block comment, which RFC 8259 does not allow.
  constexpr label_override c_json5_tests_overrides[] = {
    {"comments/irregular-block-comment.json", syntax_mode::strict_json, expectation::reject},
  };

  /**
   * @brief Apply a documented override when one names this file and mode.
   * @par Requirements
   * P040; J5-095.
   */
  expectation corrected(const std::string& relative, syntax_mode mode, expectation upstream)
  {
    for (const auto& entry : c_json5_tests_overrides)
    {
      if ((relative == entry.relative) && (mode == entry.mode))
      {
        return entry.expected;
      }
    }
    return upstream;
  }

  /**
   * @brief Collect json5-tests cases using the upstream extension convention.
   * @par Requirements
   * P040; J5-095.
   * @note .json is valid JSON and JSON5, .json5 is JSON5 only, .js and .txt are invalid.
   * todo/ is upstream work in progress and misc/ is not fixture data, so both are excluded.
   */
  std::vector<corpus_case> json5_tests_cases()
  {
    const fs::path root{ADAPTIVE_JSON5_JSON5_TESTS_DIR};
    std::vector<corpus_case> cases;
    std::set<std::string> used;
    for (const auto& file : sorted_files(root))
    {
      const std::string relative = file.lexically_relative(root).generic_string();
      const std::string extension = file.extension().string();
      if ((relative.rfind("todo/", 0) == 0) || (relative.rfind("misc/", 0) == 0) ||
          (relative.find('/') == std::string::npos))
      {
        continue;
      }
      const bool json = (extension == ".json");
      const bool json5 = json || (extension == ".json5");
      if (!json5 && (extension != ".js") && (extension != ".txt"))
      {
        continue;
      }
      const std::string path = file.string();
      cases.push_back({case_name("Json5_", relative, used), path, syntax_mode::json5,
                       corrected(relative, syntax_mode::json5, json5 ? expectation::accept : expectation::reject)});
      cases.push_back(
        {case_name("Strict_", relative, used), path, syntax_mode::strict_json,
         corrected(relative, syntax_mode::strict_json, json ? expectation::accept : expectation::reject)});
    }
    return cases;
  }

  /**
   * @brief Collect JSONTestSuite cases using the upstream prefix convention.
   * @par Requirements
   * P040; J5-095.
   * @note Strict mode: y_ accept, n_ reject, i_ use documented project decisions.
   * JSON5 mode: y_ accept; i_ use project decisions; n_ need only complete cleanly,
   * since many n_ fixtures are valid JSON5.
   */
  std::vector<corpus_case> json_test_suite_cases()
  {
    const fs::path root{ADAPTIVE_JSON5_JSONTESTSUITE_DIR};
    std::vector<corpus_case> cases;
    std::set<std::string> used;
    for (const auto& file : sorted_files(root))
    {
      const std::string stem = file.filename().string();
      const char kind = stem.empty() ? '\0' : stem.front();
      if ((file.extension() != ".json") || (stem.size() < 2U) || (stem[1] != '_') ||
          ((kind != 'y') && (kind != 'n') && (kind != 'i')))
      {
        continue;
      }
      const std::string path = file.string();
      const expectation strict =
        (kind == 'y') ? expectation::accept
                      : ((kind == 'n') ? expectation::reject : project_outcome(stem, syntax_mode::strict_json));
      cases.push_back({case_name("Strict_", stem, used), path, syntax_mode::strict_json, strict});
      cases.push_back({case_name("Json5_", stem, used), path, syntax_mode::json5,
                       (kind == 'y')
                         ? expectation::accept
                         : ((kind == 'i') ? project_outcome(stem, syntax_mode::json5) : expectation::either)});
    }
    return cases;
  }

  /**
   * @brief Parse one case with default limits and check its outcome and full release.
   * @par Requirements
   * P040; J5-095; J5-098.
   */
  void expect_case(const corpus_case& param)
  {
    const std::string bytes = read_bytes(param.path);
    counting_provider provider;
    {
      const parse_options options{param.mode};
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      const auto parsed = parse_document({bytes.data(), bytes.size()}, options, accounting);
      if (param.expected == expectation::accept)
      {
        EXPECT_TRUE(parsed.has_value()) << param.path << " error code "
                                        << (parsed.has_value() ? 0 : static_cast<int>(parsed.error_if()->code))
                                        << " at byte " << (parsed.has_value() ? 0U : parsed.error_if()->byte_offset);
      }
      else if (param.expected == expectation::reject)
      {
        EXPECT_FALSE(parsed.has_value()) << param.path;
      }
      if (!parsed.has_value())
      {
        EXPECT_EQ(provider.m_live_bytes, 0U) << param.path;
      }
    }
    // Destroying an accepted document must also release everything.
    EXPECT_EQ(provider.m_live_bytes, 0U) << param.path;
  }

  /**
   * @brief Print the generated case name in failure output.
   * @par Requirements
   * P040; J5-095.
   */
  std::string param_name(const testing::TestParamInfo<corpus_case>& info)
  {
    return info.param.name;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_095 =================================== */
  class J5_095_Json5TestsCorpus : public testing::TestWithParam<corpus_case>
  {
  };
  /*
   * Verify every pinned json5-tests file is accepted or rejected as its extension requires.
   * Arrange: Each fixture file outside todo/ and misc/, in JSON5 and strict modes.
   * Act: Parse the complete file with default options through the public entry point.
   * Expect: The upstream outcome and no outstanding allocation afterwards.
   */
  TEST_P(J5_095_Json5TestsCorpus, MatchesUpstreamExpectation)
  {
    expect_case(GetParam());
  }
  INSTANTIATE_TEST_SUITE_P(Fixture, J5_095_Json5TestsCorpus, testing::ValuesIn(json5_tests_cases()), param_name);

  /* ----------------------------------------------------------------------------------- */

  class J5_095_JsonTestSuiteCorpus : public testing::TestWithParam<corpus_case>
  {
  };
  /*
   * Verify every pinned JSONTestSuite parsing file follows its y_/n_/i_ prefix.
   * Arrange: Each test_parsing file in strict and JSON5 modes.
   * Act: Parse the complete file with default options through the public entry point.
   * Expect: The required outcome where one exists, and no outstanding allocation afterwards.
   */
  TEST_P(J5_095_JsonTestSuiteCorpus, MatchesUpstreamExpectation)
  {
    expect_case(GetParam());
  }
  INSTANTIATE_TEST_SUITE_P(Fixture, J5_095_JsonTestSuiteCorpus, testing::ValuesIn(json_test_suite_cases()), param_name);

  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: The pinned i_ inventory and the explicit project decision table.
   * Act: Compare their filename sets.
   * Expect: Every fixture has exactly one classification and none are stale.
   */
  TEST(J5_095_ProjectDecisions, ClassifiesEveryImplementationDefinedFixture)
  {
    /* Arrange */
    std::set<std::string> files;
    std::set<std::string> decisions;
    /* Act */
    for (const auto& file : sorted_files(fs::path{ADAPTIVE_JSON5_JSONTESTSUITE_DIR}))
    {
      const auto name = file.filename().string();
      if (name.rfind("i_", 0) == 0 && file.extension() == ".json")
      {
        files.insert(name);
      }
    }
    for (const auto& decision : c_project_decisions)
    {
      EXPECT_TRUE(decisions.insert(decision.file).second) << decision.file;
    }
    /* Expect */
    EXPECT_EQ(files, decisions);
    EXPECT_EQ(files.size(), 35u);
  }

  /* ----------------------------------------------------------------------------------- */
  class J5_095_ProjectOutcome : public testing::TestWithParam<std::tuple<project_decision, syntax_mode>>
  {
  };
  /*
   * Arrange: One documented fixture choice and syntax mode with default limits.
   * Act: Parse its immutable bytes through the public API.
   * Expect: The promised outcome, exact failure location or ordered warning spans, and full cleanup.
   */
  TEST_P(J5_095_ProjectOutcome, EnforcesDocumentedBehaviour)
  {
    /* Arrange */
    const auto& decision = std::get<0>(GetParam());
    const auto mode = std::get<1>(GetParam());
    SCOPED_TRACE(decision.file);
    const auto path = fs::path{ADAPTIVE_JSON5_JSONTESTSUITE_DIR} / decision.file;
    const auto bytes = read_bytes(path.string());
    counting_provider provider;
    const parse_options options{mode};
    /* Act */
    {
      accounted_storage accounting{provider.callbacks(), options.limits.max_owned_bytes};
      const auto parsed = parse_document({bytes.data(), bytes.size()}, options, accounting);
      /* Expect */
      const bool accepted = mode == syntax_mode::strict_json ? decision.strict : decision.json5;
      ASSERT_EQ(parsed.has_value(), accepted);
      if (accepted)
      {
        ASSERT_EQ(parsed.value_if()->warnings.size(), decision.warnings);
        for (std::size_t index{}; index < decision.warnings; ++index)
        {
          const auto warning = parsed.value_if()->warnings.at(index);
          ASSERT_TRUE(warning.has_value());
          EXPECT_EQ(warning.value_if()->code, string_warning_code::unpaired_surrogate_escape);
          EXPECT_EQ(warning.value_if()->byte_offset, decision.spans[index]);
          EXPECT_EQ(warning.value_if()->byte_length, 6u);
        }
      }
      else
      {
        EXPECT_EQ(parsed.error_if()->code, decision.code);
        EXPECT_TRUE(parsed.error_if()->has_byte_offset);
        EXPECT_EQ(parsed.error_if()->byte_offset, decision.offset);
      }
    }
    EXPECT_EQ(provider.m_live_bytes, 0u);
  }
  INSTANTIATE_TEST_SUITE_P(P041, J5_095_ProjectOutcome,
                           testing::Combine(testing::ValuesIn(c_project_decisions),
                                            testing::Values(syntax_mode::strict_json, syntax_mode::json5)));
  /* ================================= End Test_J5_095 ================================= */
} // namespace
