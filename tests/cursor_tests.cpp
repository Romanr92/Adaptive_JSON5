// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/cursor.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
  using adaptive_json5::cursor;
  using adaptive_json5::input_view;

  /* =================================== Test_J5_004 =================================== */

  static_assert(std::is_nothrow_default_constructible_v<cursor>);
  static_assert(std::is_nothrow_copy_constructible_v<cursor>);
  static_assert(std::is_nothrow_move_constructible_v<cursor>);
  static_assert(std::is_nothrow_copy_assignable_v<cursor>);
  static_assert(std::is_nothrow_move_assignable_v<cursor>);

  constexpr cursor c_default_cursor{};
  static_assert(c_default_cursor.is_at_end());
  static_assert(c_default_cursor.offset() == 0U);
  static_assert(c_default_cursor.remaining() == 0U);
  static_assert(!c_default_cursor.peek().has_value());

  /* Verify default-constructed cursor is immediately at end.
   * Arrange: Construct a default cursor without an input view.
   * Act: Inspect end condition, offset, remaining count and peek.
   * Expect: Cursor is at end, offset and remaining are zero, peek returns nullopt.
   */
  TEST(J5_004_Cursor, DefaultConstructedIsAtEnd)
  {
    /* Arrange */
    /* Act */
    const cursor actual{};
    /* Expect */
    EXPECT_TRUE(actual.is_at_end());
    EXPECT_EQ(actual.offset(), 0U);
    EXPECT_EQ(actual.remaining(), 0U);
    EXPECT_FALSE(actual.peek().has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify cursor over empty non-null view is at end without reading.
   * Arrange: Create an input_view pointing to bytes but with length zero.
   * Act: Construct the cursor and test boundary conditions.
   * Expect: Cursor is immediately at end and peek/next return nullopt.
   */
  TEST(J5_004_Cursor, EmptyNonNullViewIsAtEnd)
  {
    /* Arrange */
    const char dummy[] = {'x'};
    const input_view view{dummy, 0};
    /* Act */
    cursor actual{view};
    /* Expect */
    EXPECT_TRUE(actual.is_at_end());
    EXPECT_EQ(actual.offset(), 0U);
    EXPECT_EQ(actual.remaining(), 0U);
    EXPECT_FALSE(actual.peek().has_value());
    EXPECT_FALSE(actual.next().has_value());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify cursor consumes bytes with explicit length without requiring a terminating NUL.
   * Arrange: Provide four bytes and bound the input_view to the first three without trailing NUL.
   * Act: Consume the three bytes one by one and observe offset progression.
   * Expect: Exactly three bytes are read, offset reaches 3, cursor reaches end, subsequent reads return nullopt.
   */
  TEST(J5_004_Cursor, ConsumesExactLengthWithoutTerminatingNul)
  {
    /* Arrange */
    const char bytes[] = {'a', 'b', 'c', 'd'};
    const input_view view{bytes, 3};
    /* Act */
    cursor actual{view};
    /* Expect */
    EXPECT_FALSE(actual.is_at_end());
    EXPECT_EQ(actual.remaining(), 3U);
    EXPECT_EQ(actual.offset(), 0U);

    EXPECT_EQ(actual.peek(), 'a');
    EXPECT_EQ(actual.next(), 'a');
    EXPECT_EQ(actual.offset(), 1U);
    EXPECT_EQ(actual.remaining(), 2U);

    EXPECT_EQ(actual.peek(), 'b');
    EXPECT_EQ(actual.next(), 'b');
    EXPECT_EQ(actual.offset(), 2U);
    EXPECT_EQ(actual.remaining(), 1U);

    EXPECT_EQ(actual.peek(), 'c');
    EXPECT_EQ(actual.next(), 'c');
    EXPECT_EQ(actual.offset(), 3U);
    EXPECT_EQ(actual.remaining(), 0U);

    EXPECT_TRUE(actual.is_at_end());
    EXPECT_FALSE(actual.peek().has_value());
    EXPECT_FALSE(actual.next().has_value());
    EXPECT_EQ(actual.offset(), 3U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify embedded zero bytes count toward length and do not terminate reading.
   * Arrange: Provide an input buffer containing an embedded NUL byte.
   * Act: Step across the embedded NUL byte and verify cursor state.
   * Expect: Embedded NUL is read as a regular character and reading continues until buffer end.
   */
  TEST(J5_004_Cursor, ReadsAcrossEmbeddedZeroBytes)
  {
    /* Arrange */
    const char bytes[] = {'x', '\0', 'y'};
    const input_view view{bytes, 3};
    /* Act */
    cursor actual{view};
    /* Expect */
    EXPECT_EQ(actual.next(), 'x');
    EXPECT_EQ(actual.offset(), 1U);

    EXPECT_EQ(actual.peek(), '\0');
    EXPECT_EQ(actual.next(), '\0');
    EXPECT_EQ(actual.offset(), 2U);
    EXPECT_FALSE(actual.is_at_end());

    EXPECT_EQ(actual.peek(), 'y');
    EXPECT_EQ(actual.next(), 'y');
    EXPECT_EQ(actual.offset(), 3U);
    EXPECT_TRUE(actual.is_at_end());
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify lookahead peeks arbitrary valid offsets without advancing position.
   * Arrange: Construct a cursor over a multi-character buffer.
   * Act: Peek ahead with delta 0, 1, 2 and past end.
   * Expect: Deltas within remaining return the correct byte; delta >= remaining returns nullopt.
   */
  TEST(J5_004_Cursor, PeekLookaheadBoundsAgainstRemaining)
  {
    /* Arrange */
    const char bytes[] = {'1', '2', '3'};
    const input_view view{bytes, 3};
    /* Act */
    const cursor actual{view};
    /* Expect */
    EXPECT_EQ(actual.peek(0), '1');
    EXPECT_EQ(actual.peek(1), '2');
    EXPECT_EQ(actual.peek(2), '3');
    EXPECT_FALSE(actual.peek(3).has_value());
    EXPECT_FALSE(actual.peek(100).has_value());
    EXPECT_EQ(actual.offset(), 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify advance clamps position at the end of input without overflow.
   * Arrange: Construct a cursor over two bytes.
   * Act: Advance beyond the total buffer size.
   * Expect: Offset is clamped at size, remaining is zero, cursor is at end.
   */
  TEST(J5_004_Cursor, AdvanceClampsAtEnd)
  {
    /* Arrange */
    const char bytes[] = {'p', 'q'};
    const input_view view{bytes, 2};
    /* Act */
    cursor actual{view};
    actual.advance(10);
    /* Expect */
    EXPECT_TRUE(actual.is_at_end());
    EXPECT_EQ(actual.offset(), 2U);
    EXPECT_EQ(actual.remaining(), 0U);
    EXPECT_FALSE(actual.peek().has_value());
  }

  /* ================================= End Test_J5_004 ================================= */

  /* =================================== Test_J5_041 =================================== */

  /* Verify span extraction bounds against current offset and start position.
   * Arrange: Consume three characters of a five-character input.
   * Act: Extract spans from offset 0, 1, and invalid start offsets.
   * Expect: Valid start offsets return exact slices; start >= current returns empty view.
   */
  TEST(J5_041_Cursor, SpansSubrangeUpToCurrentOffset)
  {
    /* Arrange */
    const char bytes[] = {'h', 'e', 'l', 'l', 'o'};
    const input_view view{bytes, 5};
    cursor actual{view};
    actual.advance(3); // offset now at 3 ('l')
    /* Act */
    const input_view span_all = actual.span_from(0);
    const input_view span_mid = actual.span_from(1);
    const input_view span_at_offset = actual.span_from(3);
    const input_view span_past_offset = actual.span_from(4);
    /* Expect */
    EXPECT_EQ(span_all.data, bytes);
    EXPECT_EQ(span_all.size, 3U);

    EXPECT_EQ(span_mid.data, bytes + 1);
    EXPECT_EQ(span_mid.size, 2U);

    EXPECT_EQ(span_at_offset.data, nullptr);
    EXPECT_EQ(span_at_offset.size, 0U);

    EXPECT_EQ(span_past_offset.data, nullptr);
    EXPECT_EQ(span_past_offset.size, 0U);
  }

  /* ================================= End Test_J5_041 ================================= */

  /* =================================== Test_J5_044 =================================== */

  /* Verify cursor preserves exact zero-based byte offset for diagnostics at every step.
   * Arrange: Construct a cursor over a string with whitespace and delimiters.
   * Act: Step and skip characters.
   * Expect: Offset matches the exact zero-based index of the current input byte.
   */
  TEST(J5_044_Cursor, PreservesExactByteOffsetForDiagnostics)
  {
    /* Arrange */
    const char bytes[] = {'{', ' ', '"', 'a', '"', '}'};
    const input_view view{bytes, 6};
    cursor actual{view};
    /* Act & Expect */
    EXPECT_EQ(actual.offset(), 0U);
    EXPECT_EQ(actual.next(), '{');
    EXPECT_EQ(actual.offset(), 1U);

    actual.advance(2); // skips ' ' and '"'
    EXPECT_EQ(actual.offset(), 3U);
    EXPECT_EQ(actual.peek(), 'a');

    actual.advance(3); // reaches end
    EXPECT_EQ(actual.offset(), 6U);
    EXPECT_TRUE(actual.is_at_end());
  }

  /* ================================= End Test_J5_044 ================================= */
} // namespace
