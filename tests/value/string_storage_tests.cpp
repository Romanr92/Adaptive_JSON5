#include "../support/string_provider.hpp"
#include "adaptive_json5/memory/owned_block.hpp"
#include "adaptive_json5/value/utf8_export.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <utility>
namespace
{
  using namespace adaptive_json5;
  using namespace adaptive_json5_test;
  /* =================================== Test_J5_029 =================================== */
  /* Verify counted storage retains raw controls and all units after embedded NUL.
   * Arrange: Supply an explicit prefix of a nonterminated array.
   * Act: Copy that prefix and export into an exact-size region.
   * Expect: Exact units and bytes with unchanged input and output suffix.
   */
  TEST(J5_029_StringStorage, PreservesNulControlsAndExplicitLength)
  {
    /* Arrange */
    string_provider provider;
    const char16_t input[]{u'A', 0, u'B', 9, 1, u'Z'};
    const std::u16string expected{u'A', 0, u'B', 9, 1};
    std::array<unsigned char, 6> output;
    output.fill(0xA5);
    /* Act */
    auto copied = decoded_string::copy_units(provider.callbacks(), input, 5);
    ASSERT_TRUE(copied.has_value());
    const auto exported = export_utf8(*copied.value_if(), output.data(), 5);
    /* Expect */
    expect_units(*copied.value_if(), expected);
    ASSERT_TRUE(exported.has_value());
    EXPECT_EQ(*exported.value_if(), 5u);
    EXPECT_EQ(output, (std::array<unsigned char, 6>{0x41, 0, 0x42, 9, 1, 0xA5}));
    EXPECT_EQ(input[5], u'Z');
    expect_error(copied.value_if()->unit_at(5), error_code::wrong_type_or_access);
    expect_error(copied.value_if()->unit_at(std::numeric_limits<std::size_t>::max()), error_code::wrong_type_or_access);
  }
  /* ================================= End Test_J5_029 ================================= */
  /* =================================== Test_J5_029 =================================== */
  /* Verify undersized buffers and null output fail before writing.
   * Arrange: Store ASCII, two-, three- and four-byte scalars with guarded output.
   * Act: Try every insufficient capacity, then null nonempty output.
   * Expect: Resource/access errors without changes to output or source.
   */
  TEST(J5_029_Utf8Export, RejectsEveryShortCapacityWithoutWriting)
  {
    /* Arrange */
    string_provider provider;
    const std::u16string units{u'A', 0x80, 0x800, 0xD83C, 0xDFBC};
    auto copied = decoded_string::copy_units(provider.callbacks(), units.data(), units.size());
    ASSERT_TRUE(copied.has_value());
    std::array<unsigned char, 12> output;
    output.fill(0xA5);
    const auto before = output;
    /* Act */
    for (std::size_t capacity = 0; capacity < 10; ++capacity)
    {
      const auto actual = export_utf8(*copied.value_if(), output.data() + 1, capacity);
      /* Expect */
      expect_error(actual, error_code::resource_limit);
      EXPECT_EQ(output, before) << capacity;
    }
    expect_error(export_utf8(*copied.value_if(), nullptr, 10), error_code::wrong_type_or_access);
    expect_units(*copied.value_if(), units);
  }
  /* ================================= End Test_J5_029 ================================= */
  /* =================================== Test_J5_029 =================================== */
  /* Verify successful mixed-width export returns exact content with extra capacity.
   * Arrange: Store independent literal units and initialize output with sentinels.
   * Act: Export a mixed-width string.
   * Expect: Exact ten UTF-8 bytes, returned length and unchanged spare bytes.
   */
  TEST(J5_029_Utf8Export, WritesMixedWidthsWithoutTerminator)
  {
    /* Arrange */
    string_provider provider;
    const std::u16string units{u'A', 0x80, 0x800, 0xD83C, 0xDFBC};
    auto copied = decoded_string::copy_units(provider.callbacks(), units.data(), units.size());
    ASSERT_TRUE(copied.has_value());
    std::array<unsigned char, 12> output;
    output.fill(0xA5);
    /* Act */
    const auto actual = export_utf8(*copied.value_if(), output.data(), output.size());
    /* Expect */
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(*actual.value_if(), 10u);
    EXPECT_EQ(output,
              (std::array<unsigned char, 12>{0x41, 0xC2, 0x80, 0xE0, 0xA0, 0x80, 0xF0, 0x9F, 0x8E, 0xBC, 0xA5, 0xA5}));
    EXPECT_EQ(provider.m_attempts, 1u);
  }
  /* ================================= End Test_J5_029 ================================= */
  /* =================================== Test_J5_040 =================================== */
  /* Verify copied content survives input mutation, destruction and ownership moves.
   * Arrange: Construct counted input in a nested scope.
   * Act: Copy, overwrite and destroy input; move the string twice.
   * Expect: Exact content, empty moved-from owners and one final release.
   */
  TEST(J5_040_StringStorage, OwnsInputAcrossMovesAndDestruction)
  {
    /* Arrange */
    string_provider provider;
    {
      auto text = [&provider]()
      {
        std::array<char16_t, 3> input{u'A', 0, u'B'};
        auto copied = decoded_string::copy_units(provider.callbacks(), input.data(), input.size());
        input.fill(u'X');
        return copied;
      }();
      ASSERT_TRUE(text.has_value());
      /* Act */
      decoded_string moved(std::move(*text.value_if()));
      decoded_string moved_again(std::move(moved));
      /* Expect */
      expect_units(moved_again, {u'A', 0, u'B'});
      EXPECT_EQ(text.value_if()->size(), 0u);
      EXPECT_EQ(moved.size(), 0u);
      EXPECT_EQ(provider.m_releases, 0u);
    }
    EXPECT_EQ(provider.m_releases, 1u);
  }
  /* ================================= End Test_J5_040 ================================= */
  /* =================================== Test_J5_040 =================================== */
  /* Verify move assignment releases through the original provider before adoption.
   * Arrange: Allocate destination and source blocks through different providers.
   * Act: Move-assign, self-move, then assign an empty block.
   * Expect: Correct release routing, empty source and unchanged self-move ownership.
   */
  TEST(J5_040_OwnedBlock, MoveAssignmentPreservesProviderAndSelfMove)
  {
    /* Arrange */
    string_provider old_provider;
    string_provider new_provider;
    auto destination = allocate_block(old_provider.callbacks(), 8, alignof(char16_t));
    auto source = allocate_block(new_provider.callbacks(), 16, alignof(char16_t));
    ASSERT_TRUE(destination.has_value());
    ASSERT_TRUE(source.has_value());
    void* const address = source.value_if()->data();
    /* Act */
    *destination.value_if() = std::move(*source.value_if());
    auto& owner = *destination.value_if();
    owner = std::move(owner);
    /* Expect */
    EXPECT_EQ(old_provider.m_releases, 1u);
    EXPECT_EQ(new_provider.m_releases, 0u);
    EXPECT_EQ(owner.data(), address);
    EXPECT_EQ(owner.size(), 16u);
    EXPECT_EQ(source.value_if()->data(), nullptr);
    EXPECT_EQ(source.value_if()->size(), 0u);
    EXPECT_EQ(source.value_if()->alignment(), 0u);
    owner = owned_block{};
    EXPECT_EQ(owner.data(), nullptr);
    EXPECT_EQ(new_provider.m_releases, 1u);
  }
  /* ================================= End Test_J5_040 ================================= */
  /* =================================== Test_J5_046 =================================== */
  /* Verify malformed copy requests and exhausted providers return explicit failures.
   * Arrange: Select null input, oversized count, missing callbacks and injected exhaustion.
   * Act: Request independent unit copies.
   * Expect: Correct error categories and no unnecessary provider calls.
   */
  TEST(J5_046_StringStorage, RejectsInvalidAndUnrepresentableRequests)
  {
    /* Arrange */
    string_provider provider;
    const char16_t unit = u'A';
    const auto callbacks = provider.callbacks();
    /* Act */
    const auto null_input = decoded_string::copy_units(callbacks, nullptr, 1);
    const auto overflow = decoded_string::copy_units(callbacks, &unit, std::numeric_limits<std::size_t>::max());
    const auto missing = decoded_string::copy_units({}, &unit, 1);
    const auto no_allocate = decoded_string::copy_units({&provider, nullptr, callbacks.deallocate}, &unit, 1);
    const auto no_release = decoded_string::copy_units({&provider, callbacks.allocate, nullptr}, &unit, 1);
    provider.m_fail_on = 1;
    const auto exhausted = decoded_string::copy_units(callbacks, &unit, 1);
    /* Expect */
    expect_error(null_input, error_code::wrong_type_or_access);
    expect_error(overflow, error_code::resource_limit);
    expect_error(missing, error_code::allocation_failure);
    expect_error(no_allocate, error_code::allocation_failure);
    expect_error(no_release, error_code::allocation_failure);
    expect_error(exhausted, error_code::allocation_failure);
    EXPECT_EQ(provider.m_attempts, 1u);
    EXPECT_EQ(provider.m_allocations, 0u);
  }
  /* ================================= End Test_J5_046 ================================= */
} // namespace
