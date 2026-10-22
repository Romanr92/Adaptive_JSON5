#include "../support/document_provider.hpp"
#include "adaptive_json5/value/document.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "adaptive_json5/value/value_iterator.hpp"

#include <gtest/gtest.h>
#include <utility>

namespace
{
  using namespace adaptive_json5;
  using adaptive_json5_test::document_provider;

  /* =================================== Test_J5_039 ==================================== */
  /*
   * Arrange: A default view with no owning document.
   * Act: Request each typed value and inspect its explicit failure information.
   * Expect: Accessors consistently report access errors without allocating.
   */
  TEST(J5_039_ValueAccess, InvalidViewExposesTypedErrors)
  {
    /* Arrange */
    const value_view view;
    /* Act */
    const auto boolean = view.as_boolean();
    const auto string = view.as_string();
    const auto number = view.as_number();
    const auto member = view.member_name();
    /* Expect */
    // Callers need readable error results from every typed accessor, including pointer-valued results.
    ASSERT_NE(boolean.error_if(), nullptr);
    EXPECT_EQ(boolean.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_NE(string.error_if(), nullptr);
    EXPECT_EQ(string.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_NE(number.error_if(), nullptr);
    EXPECT_EQ(number.error_if()->code, error_code::wrong_type_or_access);
    ASSERT_NE(member.error_if(), nullptr);
    EXPECT_EQ(member.error_if()->code, error_code::wrong_type_or_access);
  }
  /* ----------------------------------------------------------------------------------- */
  /*
   * Arrange: A validated number whose spelling has been moved to another owner.
   * Act: Try to insert the empty moved-from number, then insert its valid owner.
   * Expect: Empty spelling is rejected without adding a record; the owner remains usable.
   */
  TEST(J5_039_ValueAccess, RejectsMovedFromNumberWithoutAddingRecord)
  {
    /* Arrange */
    document_provider provider;
    document values{provider.callbacks(), 4};
    auto number = owned_number::copy_spelling({"12", 2}, syntax_mode::json5, provider.callbacks(), 2);
    ASSERT_TRUE(number.has_value());
    owned_number owner{std::move(*number.value_if())};
    /* Act */
    // A moved-from number is an ownership state, never a valid zero-length numeric token.
    const auto rejected = values.make_number(std::move(*number.value_if()));
    /* Expect */
    ASSERT_FALSE(rejected.has_value());
    EXPECT_EQ(rejected.error_if()->code, error_code::wrong_type_or_access);
    EXPECT_EQ(values.size(), 0u);
    const auto accepted = values.make_number(std::move(owner));
    ASSERT_TRUE(accepted.has_value());
    EXPECT_EQ(values.size(), 1u);
  }
  /* ================================= End Test_J5_039 ================================== */

  /* =================================== Test_J5_018 ==================================== */
  /*
   * Arrange: A high surrogate followed by a BMP unit above the low-surrogate range.
   * Act: Size and export the retained units through checked UTF-8 APIs.
   * Expect: Encoding errors preserve caller output and original decoded units.
   */
  TEST(J5_018_Utf8Boundary, RejectsHighSurrogateFollowedByUpperBmpUnit)
  {
    /* Arrange */
    document_provider provider;
    const char16_t units[] = {0xD800, 0xE000};
    const auto text = decoded_string::copy_units(provider.callbacks(), units, 2);
    ASSERT_TRUE(text.has_value());
    unsigned char output[] = {0xA5, 0xA5, 0xA5, 0xA5};
    /* Act */
    // The second unit exists but cannot complete the pair; it must not be silently emitted separately.
    const auto size = utf8_size(*text.value_if());
    const auto exported = export_utf8(*text.value_if(), output, sizeof(output));
    /* Expect */
    ASSERT_FALSE(size.has_value());
    ASSERT_FALSE(exported.has_value());
    EXPECT_EQ(size.error_if()->code, error_code::encoding);
    EXPECT_EQ(exported.error_if()->code, error_code::encoding);
    for (const auto byte : output)
      EXPECT_EQ(byte, 0xA5);
    EXPECT_EQ(*text.value_if()->unit_at(1).value_if(), 0xE000);
  }
  /* ================================= End Test_J5_018 ================================== */
} // namespace
