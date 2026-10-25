#include "../shared_infrastructure/guard_provider.hpp"
#include "adaptive_json5/error.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/value/retained_source.hpp"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  /**
   * @brief Read retained bytes as a string view for comparison.
   * @par Requirements
   * P039; J5-041.
   */
  std::string_view text_of(const retained_source& source)
  {
    const input_view text = source.text();
    return (text.data == nullptr) ? std::string_view{} : std::string_view{text.data, text.size};
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_041 =================================== */
  static_assert(std::is_nothrow_default_constructible_v<retained_source>);
  static_assert(std::is_nothrow_move_constructible_v<retained_source>);
  static_assert(!std::is_copy_constructible_v<retained_source>);
  static_assert(!std::is_move_assignable_v<retained_source>);

  /* Verify a copy is independent of the caller's bytes and allocates exactly their size.
   * Arrange: Mutable caller bytes and a guarded provider.
   * Act: Copy, then overwrite and destroy the caller bytes.
   * Expect: The retained text is unchanged and exactly the input size is outstanding.
   */
  TEST(J5_041_RetainedSource, CopiesIndependentlyOfInput)
  {
    /* Arrange */
    guard_provider provider;
    auto bytes = std::string{"{a: 'b'}"};
    /* Act */
    auto copied = retained_source::copy(input_view{bytes.data(), bytes.size()}, provider.callbacks());
    const std::size_t outstanding = provider.m_live_bytes;
    bytes.assign(bytes.size(), '#');
    bytes = std::string{};
    /* Expect */
    ASSERT_TRUE(copied.has_value());
    EXPECT_TRUE(copied.value_if()->is_retained());
    EXPECT_EQ(text_of(*copied.value_if()), "{a: 'b'}");
    EXPECT_EQ(outstanding, 8U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify moving transfers the copy and leaves the source retaining nothing.
   * Arrange: Copy "[1]".
   * Act: Move-construct a second object.
   * Expect: The destination reads "[1]"; the source is not retained and has an empty view.
   */
  TEST(J5_041_RetainedSource, MoveTransfersCopy)
  {
    /* Arrange */
    guard_provider provider;
    constexpr std::string_view c_input{"[1]"};
    auto copied = retained_source::copy(input_view{c_input.data(), c_input.size()}, provider.callbacks());
    ASSERT_TRUE(copied.has_value());
    /* Act */
    const retained_source moved{std::move(*copied.value_if())};
    /* Expect */
    EXPECT_TRUE(moved.is_retained());
    EXPECT_EQ(text_of(moved), "[1]");
    EXPECT_FALSE(copied.value_if()->is_retained());
    EXPECT_EQ(copied.value_if()->text().size, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify empty, default and invalid inputs retain nothing without provider calls.
   * Arrange: A default object, empty input and null data with a nonzero size.
   * Act: Copy the empty and invalid inputs.
   * Expect: Nothing retained; null data is wrong_type_or_access; no provider request.
   */
  TEST(J5_041_RetainedSource, EmptyAndInvalidInputRetainNothing)
  {
    /* Arrange */
    guard_provider provider;
    const retained_source unset{};
    /* Act */
    const auto empty = retained_source::copy(input_view{nullptr, 0U}, provider.callbacks());
    const auto invalid = retained_source::copy(input_view{nullptr, 4U}, provider.callbacks());
    /* Expect */
    EXPECT_FALSE(unset.is_retained());
    EXPECT_EQ(unset.text().size, 0U);
    ASSERT_TRUE(empty.has_value());
    EXPECT_FALSE(empty.value_if()->is_retained());
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(provider.m_attempts, 0U);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify provider exhaustion is reported and leaves nothing allocated.
   * Arrange: Inject failure on the first provider request.
   * Act: Copy three bytes.
   * Expect: allocation_failure and no outstanding bytes.
   */
  TEST(J5_041_RetainedSource, ReportsAllocationFailure)
  {
    /* Arrange */
    guard_provider provider;
    provider.m_fail_on = 1U;
    constexpr std::string_view c_input{"[1]"};
    /* Act */
    const auto copied = retained_source::copy(input_view{c_input.data(), c_input.size()}, provider.callbacks());
    /* Expect */
    ASSERT_FALSE(copied.has_value());
    EXPECT_EQ(copied.error_if()->code, error_code::allocation_failure);
    EXPECT_EQ(provider.m_live_bytes, 0U);
  }
  /* ================================= End Test_J5_041 ================================= */
} // namespace
