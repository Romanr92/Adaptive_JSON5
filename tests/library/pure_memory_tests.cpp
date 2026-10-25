#include "adaptive_json5/memory/accounted_storage.hpp"
#include "adaptive_json5/parser.hpp"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <new>
#include <regex>
#include <set>
#include <string>
#include <string_view>

/* ==================================== Fixtures ===================================== */
// Count default-heap use while a parse runs. Replacing the global operators applies
// to this whole test executable; only calls made while counting are recorded.
namespace
{
  bool g_counting = false;
  std::size_t g_default_heap_calls = 0;

  /**
   * @brief Allocate from malloc and record the call while counting.
   * @par Requirements
   * P041; J5-042.
   * @note Failure aborts: this test-only replacement must work with exceptions OFF.
   */
  void* counted_allocate(std::size_t bytes)
  {
    if (g_counting)
    {
      ++g_default_heap_calls;
    }
    void* address = std::malloc((bytes == 0U) ? 1U : bytes);
    if (address == nullptr)
    {
      std::abort();
    }
    return address;
  }
} // namespace

void* operator new(std::size_t bytes)
{
  return counted_allocate(bytes);
}
void* operator new[](std::size_t bytes)
{
  return counted_allocate(bytes);
}
void operator delete(void* address) noexcept
{
  std::free(address);
}
void operator delete[](void* address) noexcept
{
  std::free(address);
}
void operator delete(void* address, std::size_t) noexcept
{
  std::free(address);
}
void operator delete[](void* address, std::size_t) noexcept
{
  std::free(address);
}
/* ================================== End Fixtures =================================== */

namespace
{
  using namespace adaptive_json5;
  namespace fs = std::filesystem;
  /* =================================== Test_J5_042 =================================== */
  /**
   * @brief Standard headers production code may include: language support only.
   * @par Requirements
   * P041; J5-042.
   * @note Anything for files, streams, processes, threads, time, locale or networking
   * is absent on purpose. Adding a header here is a reviewed decision.
   */
  const std::set<std::string> c_allowed_standard_headers{"algorithm",   "array",   "cmath",  "cstddef",  "cstdint",
                                                         "cstring",     "limits",  "new",    "optional", "string_view",
                                                         "type_traits", "utility", "variant"};

  /* Verify production code includes no header for files, processes, threads, time, locale or I/O.
   * Arrange: Every header and source under includes/, src/includes/ and src/src/.
   * Act: Collect each angle-bracket include.
   * Expect: Every one is on the language-support allowlist, and production sources exist.
   */
  TEST(J5_042_PureMemory, ProductionIncludesOnlyLanguageSupportHeaders)
  {
    /* Arrange */
    const fs::path root{ADAPTIVE_JSON5_SOURCE_DIR};
    const std::regex include{R"(^\s*#\s*include\s*<([^>]+)>)"};
    std::size_t files = 0U;
    /* Act */
    for (const char* tree : {"includes", "src/includes", "src/src"})
    {
      std::error_code failure;
      for (fs::recursive_directory_iterator it{root / tree, failure}, end; !failure && (it != end);
           it.increment(failure))
      {
        const auto extension = it->path().extension();
        if (!it->is_regular_file(failure) || ((extension != ".hpp") && (extension != ".cpp")))
        {
          continue;
        }
        ++files;
        std::ifstream stream{it->path()};
        std::string line;
        while (std::getline(stream, line))
        {
          std::smatch match;
          /* Expect */
          if (std::regex_search(line, match, include))
          {
            EXPECT_EQ(c_allowed_standard_headers.count(match[1].str()), 1U)
              << it->path().generic_string() << " includes <" << match[1].str() << ">";
          }
        }
      }
      ASSERT_FALSE(failure) << tree;
    }
    EXPECT_GT(files, 50U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify a complete parse allocates only through the caller's storage, never the default heap.
   * Arrange: A document with every value kind, warnings and retained source, and a malloc-backed provider.
   * Act: Count global operator new calls while parsing and destroying the document.
   * Expect: Success, provider allocations, and no default-heap call.
   */
  TEST(J5_042_PureMemory, ParseUsesOnlyCallerStorage)
  {
    /* Arrange */
    struct provider
    {
        std::size_t m_allocations{};
        static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
        {
          ++static_cast<provider*>(context)->m_allocations;
          return (alignment > alignof(std::max_align_t)) ? nullptr : std::malloc(bytes);
        }
        static void release(void*, void* address, std::size_t, std::size_t) noexcept
        {
          std::free(address);
        }
    } storage;
    constexpr std::string_view c_input{"{a:'x\\u00e9',b:[1,0x2,-Infinity,null,true],'c':\"\xE2\x80\xA8\"}"};
    parse_options options{syntax_mode::json5};
    options.source_text = source_retention_policy::retain_copy;
    accounted_storage accounting{{&storage, &provider::allocate, &provider::release}, options.limits.max_owned_bytes};
    bool parsed_ok = false;
    // Control: prove the replacement operator is live before trusting a zero count.
    g_counting = true;
    delete new int{1};
    g_counting = false;
    ASSERT_EQ(g_default_heap_calls, 1U);
    /* Act */
    g_default_heap_calls = 0U;
    g_counting = true;
    {
      const auto parsed = parse_document({c_input.data(), c_input.size()}, options, accounting);
      parsed_ok = parsed.has_value();
    }
    g_counting = false;
    /* Expect */
    EXPECT_TRUE(parsed_ok);
    EXPECT_GT(storage.m_allocations, 0U);
    EXPECT_EQ(g_default_heap_calls, 0U);
    EXPECT_EQ(accounting.current_bytes(), 0U);
  }
  /* ================================= End Test_J5_042 ================================= */
} // namespace
