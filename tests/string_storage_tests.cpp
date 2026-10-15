// SPDX-License-Identifier: Apache-2.0
#include "adaptive_json5/decoded_string_builder.hpp"
#include "adaptive_json5/utf8_export.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
  using namespace adaptive_json5;

  /* ==================================== Fixtures ===================================== */

  /**
   * @brief Fixed aligned caller storage with deterministic failure and release checks.
   * @par Requirements
   * P003; P013; J5-040; J5-046; J5-051.
   * @note Each slot records its actual allocation description; no heap is used.
   */
  class string_provider
  {
    private:
      struct alignas(std::max_align_t) slot
      {
          std::array<unsigned char, 512> data{};
          std::size_t bytes{};
          std::size_t alignment{};
          bool active{};
      };
      std::array<slot, 16> m_slots{};

      /**
       * @brief Allocate a reusable aligned slot or fail at the selected request.
       * @par Requirements
       * P013; J5-046.
       */
      static void* allocate(void* context, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<string_provider*>(context);
        ++self.m_attempts;
        if (self.m_attempts == self.m_fail_on || bytes > 512 || alignment > alignof(slot))
        {
          return nullptr;
        }
        for (auto& item : self.m_slots)
        {
          if (!item.active)
          {
            item.active = true;
            item.bytes = bytes;
            item.alignment = alignment;
            ++self.m_allocations;
            self.m_live_bytes += bytes;
            if (self.m_live_bytes > self.m_peak_bytes)
            {
              self.m_peak_bytes = self.m_live_bytes;
            }
            return item.data.data();
          }
        }
        return nullptr;
      }

      /**
       * @brief Verify the original allocation description and release its slot.
       * @par Requirements
       * P003; P013; J5-040; J5-051.
       */
      static void deallocate(void* context, void* address, std::size_t bytes, std::size_t alignment) noexcept
      {
        auto& self = *static_cast<string_provider*>(context);
        for (auto& item : self.m_slots)
        {
          if (item.data.data() == address && item.active)
          {
            EXPECT_EQ(bytes, item.bytes);
            EXPECT_EQ(alignment, item.alignment);
            item.active = false;
            self.m_live_bytes -= item.bytes;
            ++self.m_releases;
            return;
          }
        }
        ADD_FAILURE() << "Unknown allocation or duplicate release";
      }

    public:
      std::size_t m_attempts{};
      std::size_t m_allocations{};
      std::size_t m_releases{};
      std::size_t m_live_bytes{};
      std::size_t m_peak_bytes{};
      std::size_t m_fail_on{};

      /**
       * @brief Verify that all owners released their allocations before the provider.
       * @par Requirements
       * P013; J5-040.
       */
      ~string_provider()
      {
        EXPECT_EQ(m_live_bytes, 0u);
        EXPECT_EQ(m_allocations, m_releases);
      }

      /**
       * @brief Borrow this provider through the production callback interface.
       * @par Requirements
       * P013; J5-046.
       */
      storage_callbacks callbacks() noexcept
      {
        return {this, &allocate, &deallocate};
      }
  };

  /**
   * @brief Check exact stored units using the public checked accessor.
   * @par Requirements
   * P013; J5-018; J5-029.
   */
  void expect_units(const decoded_string& text, const std::u16string& expected)
  {
    ASSERT_EQ(text.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      const auto actual = text.unit_at(index);
      ASSERT_TRUE(actual.has_value()) << index;
      EXPECT_EQ(*actual.value_if(), expected[index]) << index;
    }
  }

  /**
   * @brief Check the minimal allocation-free failure representation.
   * @par Requirements
   * P013; J5-003.
   */
  template <typename T>
  void expect_error(const result<T>& actual, error_code code)
  {
    ASSERT_FALSE(actual.has_value());
    ASSERT_NE(actual.error_if(), nullptr);
    EXPECT_EQ(actual.error_if()->code, code);
    EXPECT_FALSE(actual.error_if()->has_byte_offset);
    EXPECT_EQ(actual.error_if()->byte_offset, 0u);
  }

  /* ================================== End Fixtures =================================== */

  /* =================================== Test_J5_018 =================================== */

  /**
   * @brief Independent scalar, unit and UTF-8 boundary expectations.
   * @par Requirements
   * P013; J5-018; J5-029.
   */
  struct scalar_case
  {
      const char* name;                 // Diagnostic case name.
      char32_t scalar;                  // Input Unicode scalar.
      std::u16string units;             // Expected UTF-16 units.
      std::vector<unsigned char> bytes; // Expected UTF-8 octets.
  };
  class J5_018_Scalar : public ::testing::TestWithParam<scalar_case>
  {
  };

  /* Verify independent boundary encodings in both directions.
   * Arrange: Select a scalar and literal expected units and bytes.
   * Act: Encode, build, finish, decode and export.
   * Expect: Exact units, scalar, consumed count and UTF-8 bytes with intact guards.
   */
  TEST_P(J5_018_Scalar, PreservesBoundaryRepresentations)
  {
    /* Arrange */
    const auto& param = GetParam();
    string_provider provider;
    decoded_string_builder builder(provider.callbacks(), 8);
    std::array<unsigned char, 8> output;
    output.fill(0xA5);
    /* Act */
    const auto encoded = encode_utf16(param.scalar);
    const auto appended = builder.append_scalar(param.scalar);
    auto text = builder.finish();
    const auto decoded = decode_utf16(text, 0);
    const auto measured = utf8_size(text);
    const auto exported = export_utf8(text, output.data() + 1, param.bytes.size());
    /* Expect */
    ASSERT_TRUE(encoded.has_value());
    ASSERT_EQ(encoded.value_if()->size, param.units.size());
    for (std::size_t i = 0; i < param.units.size(); ++i)
    {
      EXPECT_EQ(encoded.value_if()->units[i], param.units[i]);
    }
    ASSERT_TRUE(appended.has_value());
    expect_units(text, param.units);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value_if()->value, param.scalar);
    EXPECT_EQ(decoded.value_if()->unit_count, param.units.size());
    ASSERT_TRUE(measured.has_value());
    EXPECT_EQ(*measured.value_if(), param.bytes.size());
    ASSERT_TRUE(exported.has_value());
    EXPECT_EQ(*exported.value_if(), param.bytes.size());
    EXPECT_EQ(output.front(), 0xA5);
    for (std::size_t i = 0; i < param.bytes.size(); ++i)
    {
      EXPECT_EQ(output[i + 1], param.bytes[i]);
    }
    for (std::size_t i = param.bytes.size() + 1; i < output.size(); ++i)
    {
      EXPECT_EQ(output[i], 0xA5);
    }
  }

  INSTANTIATE_TEST_SUITE_P(
    Boundaries, J5_018_Scalar,
    ::testing::Values(scalar_case{"Nul", 0, {0}, {0}}, scalar_case{"AsciiMax", 0x7F, {0x7F}, {0x7F}},
                      scalar_case{"TwoByteMin", 0x80, {0x80}, {0xC2, 0x80}},
                      scalar_case{"TwoByteMax", 0x7FF, {0x7FF}, {0xDF, 0xBF}},
                      scalar_case{"ThreeByteMin", 0x800, {0x800}, {0xE0, 0xA0, 0x80}},
                      scalar_case{"BeforeSurrogates", 0xD7FF, {0xD7FF}, {0xED, 0x9F, 0xBF}},
                      scalar_case{"AfterSurrogates", 0xE000, {0xE000}, {0xEE, 0x80, 0x80}},
                      scalar_case{"BmpMax", 0xFFFF, {0xFFFF}, {0xEF, 0xBF, 0xBF}},
                      scalar_case{"SupplementaryMin", 0x10000, {0xD800, 0xDC00}, {0xF0, 0x90, 0x80, 0x80}},
                      scalar_case{"MusicalScore", 0x1F3BC, {0xD83C, 0xDFBC}, {0xF0, 0x9F, 0x8E, 0xBC}},
                      scalar_case{"ScalarMax", 0x10FFFF, {0xDBFF, 0xDFFF}, {0xF4, 0x8F, 0xBF, 0xBF}}),
    [](const ::testing::TestParamInfo<scalar_case>& info)
    {
      return info.param.name;
    });

  /* ----------------------------------------------------------------------------------- */

  class J5_018_InvalidScalar : public ::testing::TestWithParam<char32_t>
  {
  };

  /* Verify invalid scalars never enter the builder.
   * Arrange: Seed an existing unit and select a surrogate or out-of-range value.
   * Act: Encode directly and append the invalid scalar.
   * Expect: Encoding errors, no additional allocation and unchanged content.
   */
  TEST_P(J5_018_InvalidScalar, RejectsWithoutChangingContent)
  {
    /* Arrange */
    string_provider provider;
    decoded_string_builder builder(provider.callbacks(), 8);
    ASSERT_TRUE(builder.append_unit(u'A').has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    const auto encoded = encode_utf16(GetParam());
    const auto appended = builder.append_scalar(GetParam());
    auto text = builder.finish();
    /* Expect */
    expect_error(encoded, error_code::encoding);
    expect_error(appended, error_code::encoding);
    EXPECT_EQ(provider.m_attempts, attempts);
    expect_units(text, u"A");
  }
  INSTANTIATE_TEST_SUITE_P(Invalid, J5_018_InvalidScalar,
                           ::testing::Values(char32_t{0xD800}, char32_t{0xDBFF}, char32_t{0xDC00}, char32_t{0xDFFF},
                                             char32_t{0x110000}, std::numeric_limits<char32_t>::max()));

  /* ----------------------------------------------------------------------------------- */

  class J5_018_Unpaired : public ::testing::TestWithParam<std::u16string>
  {
  };

  /* Verify storage preserves unpaired units while checked export rejects them.
   * Arrange: Put invalid surrogate sequences after a valid ASCII prefix.
   * Act: Copy units, decode at the surrogate, measure and export.
   * Expect: Exact stored units, encoding failures and an untouched destination.
   */
  TEST_P(J5_018_Unpaired, PreservesUnitsButRejectsExportAtomically)
  {
    /* Arrange */
    string_provider provider;
    const auto& units = GetParam();
    auto copied = decoded_string::copy_units(provider.callbacks(), units.data(), units.size());
    ASSERT_TRUE(copied.has_value());
    std::array<unsigned char, 32> output;
    output.fill(0xA5);
    const auto before = output;
    /* Act */
    const auto decoded = decode_utf16(*copied.value_if(), 1);
    const auto measured = utf8_size(*copied.value_if());
    const auto exported = export_utf8(*copied.value_if(), output.data(), output.size());
    /* Expect */
    expect_units(*copied.value_if(), units);
    expect_error(decoded, error_code::encoding);
    expect_error(measured, error_code::encoding);
    expect_error(exported, error_code::encoding);
    EXPECT_EQ(output, before);
    EXPECT_EQ(provider.m_attempts, 1u);
  }
  INSTANTIATE_TEST_SUITE_P(Sequences, J5_018_Unpaired,
                           ::testing::Values(std::u16string{u'A', 0xD800}, std::u16string{u'A', 0xDBFF},
                                             std::u16string{u'A', 0xDC00}, std::u16string{u'A', 0xDFFF},
                                             std::u16string{u'A', 0xDC00, 0xD800}, std::u16string{u'A', 0xD800, u'B'},
                                             std::u16string{u'A', 0xD800, 0xDBFF}));

  /* ----------------------------------------------------------------------------------- */

  /* Verify pair decoding from a nonzero index and the low-unit boundary.
   * Arrange: Store ASCII followed by a valid pair.
   * Act: Decode from each unit index and beyond the end.
   * Expect: Correct scalar at the high unit; low-unit and invalid-index errors.
   */
  TEST(J5_018_Decoder, HandlesOffsetsAndInvalidIndices)
  {
    /* Arrange */
    string_provider provider;
    const char16_t units[]{u'A', 0xD83C, 0xDFBC};
    auto copied = decoded_string::copy_units(provider.callbacks(), units, 3);
    ASSERT_TRUE(copied.has_value());
    /* Act */
    const auto pair = decode_utf16(*copied.value_if(), 1);
    const auto low = decode_utf16(*copied.value_if(), 2);
    const auto end = decode_utf16(*copied.value_if(), 3);
    const auto huge = decode_utf16(*copied.value_if(), std::numeric_limits<std::size_t>::max());
    /* Expect */
    ASSERT_TRUE(pair.has_value());
    EXPECT_EQ(pair.value_if()->value, char32_t{0x1F3BC});
    EXPECT_EQ(pair.value_if()->unit_count, 2u);
    expect_error(low, error_code::encoding);
    expect_error(end, error_code::wrong_type_or_access);
    expect_error(huge, error_code::wrong_type_or_access);
  }

  /* ================================= End Test_J5_018 ================================= */

  /* =================================== Test_J5_029 =================================== */

  /* Verify every empty operation avoids the provider and permits null output.
   * Arrange: Use default strings and a zero-limit builder without callbacks.
   * Act: Copy empty input, finish twice, measure and export.
   * Expect: Empty successes and checked failures for reads and nonempty appends.
   */
  TEST(J5_029_StringStorage, HandlesEmptyWithoutProvider)
  {
    /* Arrange */
    decoded_string empty;
    decoded_string_builder builder({}, 0);
    /* Act */
    auto copied = decoded_string::copy_units({}, nullptr, 0);
    auto first = builder.finish();
    auto second = builder.finish();
    const auto measured = utf8_size(empty);
    const auto exported = export_utf8(empty, nullptr, 0);
    const auto append = builder.append_unit(u'A');
    /* Expect */
    ASSERT_TRUE(copied.has_value());
    EXPECT_EQ(copied.value_if()->size(), 0u);
    EXPECT_EQ(first.size(), 0u);
    EXPECT_EQ(second.size(), 0u);
    ASSERT_TRUE(measured.has_value());
    EXPECT_EQ(*measured.value_if(), 0u);
    ASSERT_TRUE(exported.has_value());
    EXPECT_EQ(*exported.value_if(), 0u);
    expect_error(empty.unit_at(0), error_code::wrong_type_or_access);
    expect_error(decode_utf16(empty, 0), error_code::wrong_type_or_access);
    expect_error(append, error_code::resource_limit);
  }

  /* ----------------------------------------------------------------------------------- */

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

  /* ----------------------------------------------------------------------------------- */

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

  /* ----------------------------------------------------------------------------------- */

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

  static_assert(!std::is_copy_constructible_v<decoded_string>);
  static_assert(std::is_nothrow_move_constructible_v<decoded_string>);
  static_assert(std::is_nothrow_destructible_v<decoded_string>);
  static_assert(noexcept(decoded_string::copy_units({}, nullptr, 0)));
  static_assert(noexcept(std::declval<decoded_string_builder&>().append_unit(char16_t{})));
  static_assert(noexcept(std::declval<decoded_string_builder&>().append_scalar(char32_t{})));
  static_assert(noexcept(std::declval<decoded_string_builder&>().finish()));
  static_assert(noexcept(utf8_size(std::declval<const decoded_string&>())));
  static_assert(noexcept(export_utf8(std::declval<const decoded_string&>(), nullptr, 0)));

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

  /* ----------------------------------------------------------------------------------- */

  /* Verify finish transfers ownership and a reused builder cannot overwrite old data.
   * Arrange: Build content requiring several growth operations.
   * Act: Finish, finish empty again, then reuse and destroy the builder.
   * Expect: Independent completed strings, no finish allocation and balanced cleanup.
   */
  TEST(J5_040_StringBuilder, TransfersStorageAndSupportsReuse)
  {
    /* Arrange */
    string_provider provider;
    auto first = [&provider]()
    {
      decoded_string_builder builder(provider.callbacks(), 20);
      for (char16_t unit : std::u16string{u'A', 0, 0xD800, u'B', 0xDC00})
      {
        EXPECT_TRUE(builder.append_unit(unit).has_value());
      }
      const auto attempts = provider.m_attempts;
      /* Act */
      auto text = builder.finish();
      auto empty = builder.finish();
      EXPECT_EQ(builder.size(), 0u);
      EXPECT_EQ(empty.size(), 0u);
      EXPECT_EQ(provider.m_attempts, attempts);
      EXPECT_TRUE(builder.append_unit(u'Z').has_value());
      auto second = builder.finish();
      /* Expect */
      expect_units(second, u"Z");
      expect_units(text, {u'A', 0, 0xD800, u'B', 0xDC00});
      return text;
    }();
    expect_units(first, {u'A', 0, 0xD800, u'B', 0xDC00});
    EXPECT_GT(provider.m_peak_bytes, provider.m_live_bytes);
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify abandoned builder storage is released without needing finish.
   * Arrange: Keep the provider outside the builder scope.
   * Act: Append enough units to grow and destroy the builder directly.
   * Expect: All successful allocations are released with exact descriptions.
   */
  TEST(J5_040_StringBuilder, ReleasesUnfinishedContent)
  {
    /* Arrange */
    string_provider provider;
    {
      decoded_string_builder builder(provider.callbacks(), 20);
      /* Act */
      for (int i = 0; i < 9; ++i)
      {
        ASSERT_TRUE(builder.append_unit(u'A').has_value());
      }
    }
    /* Expect */
    EXPECT_GT(provider.m_allocations, 1u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }

  /* ----------------------------------------------------------------------------------- */

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

  /* ----------------------------------------------------------------------------------- */

  class J5_046_GrowthFailure : public ::testing::TestWithParam<std::size_t>
  {
  };

  /* Verify each growth allocation can fail without losing accepted content.
   * Arrange: Fail one of the five allocations needed to append nine units.
   * Act: Append until failure, disable injection and retry the same unit.
   * Expect: Failure preserves size; retry and finish preserve all units and cleanup.
   */
  TEST_P(J5_046_GrowthFailure, PreservesContentAndAllowsRetry)
  {
    /* Arrange */
    string_provider provider;
    provider.m_fail_on = GetParam();
    decoded_string_builder builder(provider.callbacks(), 9);
    std::u16string expected;
    bool failed = false;
    /* Act */
    for (char16_t unit : std::u16string{u'A', 0, 0xD800, u'B', 0xDC00, u'C', u'D', u'E', u'F'})
    {
      const auto appended = builder.append_unit(unit);
      if (!appended.has_value())
      {
        /* Expect */
        failed = true;
        expect_error(appended, error_code::allocation_failure);
        EXPECT_EQ(builder.size(), expected.size());
        provider.m_fail_on = 0;
        ASSERT_TRUE(builder.append_unit(unit).has_value());
      }
      expected.push_back(unit);
    }
    auto text = builder.finish();
    EXPECT_TRUE(failed);
    expect_units(text, expected);
  }
  INSTANTIATE_TEST_SUITE_P(Allocations, J5_046_GrowthFailure, ::testing::Values(1u, 2u, 3u, 4u, 5u));

  /* ----------------------------------------------------------------------------------- */

  /* Verify supplementary appends never publish only half a pair.
   * Arrange: Seed ASCII and inject failure on the pair's required allocation.
   * Act: Append a supplementary scalar, then finish the existing content.
   * Expect: Allocation failure and exactly the original ASCII unit.
   */
  TEST(J5_046_StringBuilder, PairAllocationFailureIsAtomic)
  {
    /* Arrange */
    string_provider provider;
    decoded_string_builder builder(provider.callbacks(), 3);
    ASSERT_TRUE(builder.append_unit(u'A').has_value());
    provider.m_fail_on = provider.m_attempts + 1;
    /* Act */
    const auto appended = builder.append_scalar(0x1F3BC);
    auto text = builder.finish();
    /* Expect */
    expect_error(appended, error_code::allocation_failure);
    expect_units(text, u"A");
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify exact unit limits and surrogate-pair rejection without allocation.
   * Arrange: Give the builder room for ASCII and a complete pair only.
   * Act: Append at and beyond the limit; also try a pair with only one free unit.
   * Expect: Complete accepted content and resource errors without provider requests.
   */
  TEST(J5_046_StringBuilder, EnforcesExactUnitLimits)
  {
    /* Arrange */
    string_provider provider;
    decoded_string_builder exact(provider.callbacks(), 3);
    decoded_string_builder short_pair(provider.callbacks(), 2);
    ASSERT_TRUE(exact.append_unit(u'A').has_value());
    ASSERT_TRUE(short_pair.append_unit(u'B').has_value());
    /* Act */
    const auto accepted = exact.append_scalar(0x1F3BC);
    const auto attempts = provider.m_attempts;
    const auto beyond = exact.append_unit(u'C');
    const auto partial = short_pair.append_scalar(0x1F3BC);
    auto full = exact.finish();
    auto original = short_pair.finish();
    /* Expect */
    ASSERT_TRUE(accepted.has_value());
    expect_error(beyond, error_code::resource_limit);
    expect_error(partial, error_code::resource_limit);
    EXPECT_EQ(provider.m_attempts, attempts);
    expect_units(full, {u'A', 0xD83C, 0xDFBC});
    expect_units(original, u"B");
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify an unusable provider fails without publishing a unit.
   * Arrange: Construct a nonzero-limit builder without storage callbacks.
   * Act: Append a unit and finish after failure.
   * Expect: Allocation error and an empty completed string.
   */
  TEST(J5_046_StringBuilder, RejectsMissingProvider)
  {
    /* Arrange */
    decoded_string_builder builder({}, 3);
    /* Act */
    const auto actual = builder.append_unit(u'A');
    auto text = builder.finish();
    /* Expect */
    expect_error(actual, error_code::allocation_failure);
    EXPECT_EQ(text.size(), 0u);
  }

  /* ================================= End Test_J5_046 ================================= */
} // namespace
