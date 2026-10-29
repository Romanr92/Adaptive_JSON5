#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>
#include <set>
#include <string>

namespace
{
  namespace fs = std::filesystem;
  /* ==================================== Fixtures ===================================== */

  const std::string c_umbrella = "adaptive_json5/adaptive_json5.hpp";

  /**
   * @brief Collect every quoted include of a file.
   * @par Requirements
   * J5-092.
   * @param path File to read.
   * @return The include paths, as written between the quotes.
   */
  std::set<std::string> quoted_includes(const fs::path& path)
  {
    const std::regex include{R"re(^\s*#\s*include\s*"([^"]+)")re"};
    std::set<std::string> found;
    std::ifstream stream{path};
    std::string line;
    while (std::getline(stream, line))
    {
      std::smatch match;
      if (std::regex_search(line, match, include))
      {
        found.insert(match[1].str());
      }
    }
    return found;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_092 =================================== */

  /* Verify the umbrella includes every public header except the optional hosted ones.
   * Arrange: Every .hpp under includes/adaptive_json5/, as an include path.
   * Act: Read the quoted includes of the umbrella header.
   * Expect: Each public header outside hosted/ is included, and the public tree is not empty.
   */
  TEST(J5_092_UmbrellaCoverage, IncludesEveryPublicHeaderExceptHosted)
  {
    /* Arrange */
    const fs::path root{ADAPTIVE_JSON5_SOURCE_DIR "/includes"};
    std::set<std::string> public_headers;
    std::error_code failure;
    for (fs::recursive_directory_iterator it{root / "adaptive_json5", failure}, end; !failure && (it != end);
         it.increment(failure))
    {
      if (it->is_regular_file(failure) && (it->path().extension() == ".hpp"))
      {
        public_headers.insert(fs::relative(it->path(), root).generic_string());
      }
    }
    ASSERT_FALSE(failure);
    /* Act */
    const std::set<std::string> included = quoted_includes(root / c_umbrella);
    /* Expect */
    EXPECT_GT(public_headers.size(), 30U);
    for (const std::string& header : public_headers)
    {
      if ((header == c_umbrella) || (header.rfind("adaptive_json5/hosted/", 0) == 0U))
      {
        continue;
      }
      EXPECT_EQ(included.count(header), 1U) << c_umbrella << " does not include " << header;
    }
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the umbrella includes only existing public headers outside hosted/.
   * Arrange: The quoted includes of the umbrella header.
   * Act: Resolve each one under includes/.
   * Expect: Each starts with adaptive_json5/, is not under hosted/ and names an existing file.
   */
  TEST(J5_092_UmbrellaCoverage, IncludesNoHostedOrPrivateHeader)
  {
    /* Arrange */
    const fs::path root{ADAPTIVE_JSON5_SOURCE_DIR "/includes"};
    const std::set<std::string> included = quoted_includes(root / c_umbrella);
    /* Act */
    ASSERT_FALSE(included.empty());
    for (const std::string& header : included)
    {
      /* Expect */
      EXPECT_EQ(header.rfind("adaptive_json5/", 0), 0U) << header;
      EXPECT_NE(header.rfind("adaptive_json5/hosted/", 0), 0U) << header;
      EXPECT_TRUE(fs::is_regular_file(root / header)) << header;
    }
  }
  /* ================================= End Test_J5_092 ================================= */
} // namespace
