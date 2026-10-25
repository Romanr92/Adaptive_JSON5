#include "adaptive_json5/input.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <type_traits>

/* =================================== Test_J5_004 =================================== */

// A view must not provide writable access through its data pointer.
static_assert(std::is_same_v<decltype(adaptive_json5::input_view{}.data), const char*>);
static_assert(std::is_same_v<decltype(adaptive_json5::input_view{}.size), std::size_t>);
static_assert(std::is_nothrow_default_constructible_v<adaptive_json5::input_view>);
constexpr adaptive_json5::input_view empty_input{};
static_assert(empty_input.data == nullptr && empty_input.size == 0);

/* Verify the default input description is empty.
 * Arrange: Choose the default input view.
 * Act: Construct the view.
 * Expect: The data pointer is null and byte count is zero.
 */
TEST(J5_004_InputView, DefaultsToEmptyBuffer)
{
  /* Arrange */
  using adaptive_json5::input_view;
  /* Act */
  const input_view actual{};
  /* Expect */
  EXPECT_EQ(actual.data, nullptr);
  EXPECT_EQ(actual.size, 0U);
}

/* ----------------------------------------------------------------------------------- */

/* Verify empty input need not have a null pointer.
 * Arrange: Provide readable caller storage and a zero byte count.
 * Act: Construct an empty borrowed view.
 * Expect: The original pointer is retained while the byte count stays zero.
 */
TEST(J5_004_InputView, AllowsNonNullEmptyBuffer)
{
  /* Arrange */
  const char bytes[] = {'x'};
  /* Act */
  const adaptive_json5::input_view actual{bytes, 0};
  /* Expect */
  EXPECT_EQ(actual.data, bytes);
  EXPECT_EQ(actual.size, 0U);
}

/* ----------------------------------------------------------------------------------- */

/* Verify explicit length without a terminating NUL.
 * Arrange: Provide four const bytes and select a three-byte prefix.
 * Act: Construct the view and inspect its description.
 * Expect: The pointer and selected length are preserved; caller storage is unchanged.
 */
TEST(J5_004_InputView, DescribesReadOnlyNonTerminatedPrefix)
{
  /* Arrange */
  const char bytes[] = {'[', '0', ']', 'x'};
  const std::array<char, 4> before = {'[', '0', ']', 'x'};
  /* Act */
  const adaptive_json5::input_view actual{bytes, 3};
  /* Expect */
  EXPECT_EQ(actual.data, bytes);
  EXPECT_EQ(actual.size, 3U);
  EXPECT_EQ(actual.data[actual.size - 1], ']');
  EXPECT_TRUE(std::equal(before.begin(), before.end(), bytes));
}

/* ----------------------------------------------------------------------------------- */

/* Verify zero bytes do not determine input-view length.
 * Arrange: Provide three bytes with an embedded zero.
 * Act: Construct a view over the complete array.
 * Expect: The byte count includes the zero and the final byte remains represented.
 */
TEST(J5_004_InputView, RetainsEmbeddedZeroInByteCount)
{
  /* Arrange */
  const char bytes[] = {'a', '\0', 'b'};
  /* Act */
  const adaptive_json5::input_view actual{bytes, sizeof(bytes)};
  /* Expect */
  EXPECT_EQ(actual.data, bytes);
  EXPECT_EQ(actual.size, 3U);
  EXPECT_EQ(actual.data[1], '\0');
  EXPECT_EQ(actual.data[2], 'b');
}

/* ----------------------------------------------------------------------------------- */

/* Verify the view retains caller-owned storage.
 * Arrange: Create a view over mutable caller storage.
 * Act: Change the original bytes while no reader operation is active.
 * Expect: The view refers to the changed original storage, not an owned copy.
 */
TEST(J5_004_InputView, BorrowsCallerStorageWithoutCopying)
{
  /* Arrange */
  char bytes[] = {'a', 'b'};
  const adaptive_json5::input_view actual{bytes, sizeof(bytes)};
  /* Act */
  bytes[0] = 'x';
  /* Expect */
  EXPECT_EQ(actual.data, bytes);
  EXPECT_EQ(actual.size, 2U);
  EXPECT_EQ(actual.data[0], 'x');
}

/* ================================= End Test_J5_004 ================================= */
