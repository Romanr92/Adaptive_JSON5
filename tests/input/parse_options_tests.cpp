// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/options.hpp"

#include <gtest/gtest.h>
#include <type_traits>

/* =================================== Test_J5_005 =================================== */

static_assert(std::is_nothrow_default_constructible_v<adaptive_json5::parse_options>);
constexpr adaptive_json5::parse_options default_options{};
static_assert(default_options.syntax == adaptive_json5::syntax_mode::json5);
static_assert(adaptive_json5::syntax_mode::json5 != adaptive_json5::syntax_mode::strict_json);

/* Verify the documented default syntax selection.
 * Arrange: Choose default parse options.
 * Act: Construct the options.
 * Expect: JSON5 is selected.
 */
TEST(J5_005_ParseOptions, DefaultsToJson5)
{
  /* Arrange */
  using adaptive_json5::parse_options;
  /* Act */
  const parse_options actual{};
  /* Expect */
  EXPECT_EQ(actual.syntax, adaptive_json5::syntax_mode::json5);
}

/* ----------------------------------------------------------------------------------- */

/* Verify explicit JSON5 selection.
 * Arrange: Choose the named JSON5 mode.
 * Act: Construct the options with that mode.
 * Expect: The selected mode is JSON5.
 */
TEST(J5_005_ParseOptions, SelectsJson5Explicitly)
{
  /* Arrange */
  constexpr auto selected = adaptive_json5::syntax_mode::json5;
  /* Act */
  const adaptive_json5::parse_options actual{selected};
  /* Expect */
  EXPECT_EQ(actual.syntax, selected);
}

/* ----------------------------------------------------------------------------------- */

/* Verify strict JSON can be selected independently of the default.
 * Arrange: Choose strict JSON and construct separate default options.
 * Act: Construct strict options.
 * Expect: Strict JSON is retained while the independent default remains JSON5.
 */
TEST(J5_005_ParseOptions, SelectsStrictJsonExplicitly)
{
  /* Arrange */
  constexpr auto selected = adaptive_json5::syntax_mode::strict_json;
  const adaptive_json5::parse_options defaults{};
  /* Act */
  const adaptive_json5::parse_options actual{selected};
  /* Expect */
  EXPECT_EQ(actual.syntax, selected);
  EXPECT_EQ(defaults.syntax, adaptive_json5::syntax_mode::json5);
  EXPECT_NE(actual.syntax, defaults.syntax);
}

/* ================================= End Test_J5_005 ================================= */
