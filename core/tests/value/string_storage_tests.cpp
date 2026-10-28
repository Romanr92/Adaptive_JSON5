#include "../../../tests/shared_infrastructure/string_provider.hpp"
#include "adaptive_json5/value/utf8_export.hpp"
#include "memory/decoded_string_builder.hpp"
#include "unicode/utf16.hpp"
#include "unicode/utf16_decode.hpp"

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

  using namespace adaptive_json5_test;
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
   * Act: Encode, reserve the expected units, build, finish, decode and export.
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
    const auto reserved = builder.reserve_exact(param.units.size());
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
    ASSERT_TRUE(reserved.has_value());
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
    // Room for the seed and a pair, so only the invalid scalar can cause the failure.
    ASSERT_TRUE(builder.reserve_exact(3).has_value());
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
   * Act: Reserve zero units, copy empty input, finish twice, measure, export and append.
   * Expect: Empty successes, checked read failures, and an append past the empty
   *         reservation rejected as an internal error.
   */
  TEST(J5_029_StringStorage, HandlesEmptyWithoutProvider)
  {
    /* Arrange */
    decoded_string empty;
    decoded_string_builder builder({}, 0);
    /* Act */
    const auto reserved = builder.reserve_exact(0);
    auto copied = decoded_string::copy_units({}, nullptr, 0);
    auto first = builder.finish();
    auto second = builder.finish();
    const auto measured = utf8_size(empty);
    const auto exported = export_utf8(empty, nullptr, 0);
    const auto append = builder.append_unit(u'A');
    /* Expect */
    EXPECT_TRUE(reserved.has_value());
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
    // Appends never grow storage, so content beyond the reservation is a contract violation.
    expect_error(append, error_code::wrong_type_or_access);
  }

  /* ----------------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------------------- */

  /* ----------------------------------------------------------------------------------- */

  /* ================================= End Test_J5_029 ================================= */

  /* =================================== Test_J5_040 =================================== */

  static_assert(!std::is_copy_constructible_v<decoded_string>);
  static_assert(std::is_nothrow_move_constructible_v<decoded_string>);
  static_assert(std::is_nothrow_destructible_v<decoded_string>);
  static_assert(noexcept(decoded_string::copy_units({}, nullptr, 0)));
  static_assert(noexcept(std::declval<decoded_string_builder&>().reserve_exact(std::size_t{})));
  static_assert(noexcept(std::declval<decoded_string_builder&>().append_unit(char16_t{})));
  static_assert(noexcept(std::declval<decoded_string_builder&>().append_scalar(char32_t{})));
  static_assert(noexcept(std::declval<decoded_string_builder&>().finish()));
  static_assert(noexcept(utf8_size(std::declval<const decoded_string&>())));
  static_assert(noexcept(export_utf8(std::declval<const decoded_string&>(), nullptr, 0)));

  /* ----------------------------------------------------------------------------------- */

  /* Verify finish transfers ownership and a reused builder cannot overwrite old data.
   * Arrange: Reserve and build five units, including NUL and unpaired surrogates.
   * Act: Finish, finish empty again, then reserve again, reuse and destroy the builder.
   * Expect: Independent completed strings, no finish allocation and balanced cleanup.
   */
  TEST(J5_040_StringBuilder, TransfersStorageAndSupportsReuse)
  {
    /* Arrange */
    string_provider provider;
    auto first = [&provider]()
    {
      decoded_string_builder builder(provider.callbacks(), 20);
      EXPECT_TRUE(builder.reserve_exact(5).has_value());
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
      // finish() leaves the builder empty, so a new string takes a new reservation.
      EXPECT_TRUE(builder.reserve_exact(1).has_value());
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
   * Act: Reserve and append nine units, then destroy the builder directly.
   * Expect: One allocation, released with its exact description.
   */
  TEST(J5_040_StringBuilder, ReleasesUnfinishedContent)
  {
    /* Arrange */
    string_provider provider;
    {
      decoded_string_builder builder(provider.callbacks(), 20);
      /* Act */
      ASSERT_TRUE(builder.reserve_exact(9).has_value());
      for (int i = 0; i < 9; ++i)
      {
        ASSERT_TRUE(builder.append_unit(u'A').has_value());
      }
    }
    /* Expect */
    EXPECT_EQ(provider.m_allocations, 1u);
    EXPECT_EQ(provider.m_live_bytes, 0u);
    EXPECT_EQ(provider.m_allocations, provider.m_releases);
  }

  /* ----------------------------------------------------------------------------------- */

  /* ================================= End Test_J5_040 ================================= */

  /* =================================== Test_J5_046 =================================== */

  /* ----------------------------------------------------------------------------------- */

  /* Verify the single reservation can fail and be retried without leftover state.
   * Arrange: Inject failure into the first allocation request.
   * Act: Reserve nine units, retry after disabling injection, then append and finish.
   * Expect: allocation_failure with no content or capacity, then one successful
   *         allocation holding exactly the nine units.
   */
  TEST(J5_046_StringBuilder, ReservationFailureAllowsRetry)
  {
    /* Arrange */
    const std::u16string units{u'A', 0, 0xD800, u'B', 0xDC00, u'C', u'D', u'E', u'F'};
    string_provider provider;
    provider.m_fail_on = 1;
    decoded_string_builder builder(provider.callbacks(), units.size());
    /* Act */
    const auto failed = builder.reserve_exact(units.size());
    const auto failed_append = builder.append_unit(u'A');
    provider.m_fail_on = 0;
    const auto retried = builder.reserve_exact(units.size());
    for (char16_t unit : units)
    {
      ASSERT_TRUE(builder.append_unit(unit).has_value());
    }
    auto text = builder.finish();
    /* Expect */
    expect_error(failed, error_code::allocation_failure);
    // A failed reservation leaves no capacity, so nothing can be appended.
    expect_error(failed_append, error_code::wrong_type_or_access);
    ASSERT_TRUE(retried.has_value());
    expect_units(text, units);
    EXPECT_EQ(provider.m_allocations, 1u);
    EXPECT_EQ(provider.m_live_bytes, units.size() * sizeof(char16_t));
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify supplementary appends never publish only half a pair.
   * Arrange: Reserve two units and seed ASCII, leaving room for one unit only.
   * Act: Append a supplementary scalar, then finish the existing content.
   * Expect: wrong_type_or_access, no provider request and exactly the original ASCII unit.
   */
  TEST(J5_046_StringBuilder, PairBeyondReservationIsAtomic)
  {
    /* Arrange */
    string_provider provider;
    decoded_string_builder builder(provider.callbacks(), 3);
    ASSERT_TRUE(builder.reserve_exact(2).has_value());
    ASSERT_TRUE(builder.append_unit(u'A').has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    const auto appended = builder.append_scalar(0x1F3BC);
    auto text = builder.finish();
    /* Expect */
    expect_error(appended, error_code::wrong_type_or_access);
    EXPECT_EQ(provider.m_attempts, attempts);
    expect_units(text, u"A");
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify the unit limit applies to the reservation, before any provider request.
   * Arrange: A builder limited to three units.
   * Act: Reserve four units, then exactly three, fill them with ASCII and a pair, and
   *      append one more unit.
   * Expect: resource_limit without a provider request for four units; complete content
   *         at the limit; wrong_type_or_access for the unit beyond the reservation.
   */
  TEST(J5_046_StringBuilder, EnforcesExactUnitLimits)
  {
    /* Arrange */
    string_provider provider;
    decoded_string_builder exact(provider.callbacks(), 3);
    /* Act */
    const auto above = exact.reserve_exact(4);
    const auto attempts_after_above = provider.m_attempts;
    const auto reserved = exact.reserve_exact(3);
    const auto first = exact.append_unit(u'A');
    const auto pair = exact.append_scalar(0x1F3BC);
    const auto beyond = exact.append_unit(u'C');
    auto full = exact.finish();
    /* Expect */
    expect_error(above, error_code::resource_limit);
    EXPECT_EQ(attempts_after_above, 0u);
    ASSERT_TRUE(reserved.has_value());
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(pair.has_value());
    expect_error(beyond, error_code::wrong_type_or_access);
    EXPECT_EQ(provider.m_allocations, 1u);
    expect_units(full, {u'A', 0xD83C, 0xDFBC});
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify appending past the reservation fails without allocating or changing content.
   * Arrange: Reserve two units and fill them.
   * Act: Append a third unit, then try a second reservation.
   * Expect: wrong_type_or_access for both, no new provider request, unchanged content
   *         and exactly the reserved bytes live.
   */
  TEST(J5_046_StringBuilder, AppendPastReservationFailsWithoutAllocating)
  {
    /* Arrange */
    string_provider provider;
    decoded_string_builder builder(provider.callbacks(), 8);
    ASSERT_TRUE(builder.reserve_exact(2).has_value());
    ASSERT_TRUE(builder.append_unit(u'A').has_value());
    ASSERT_TRUE(builder.append_unit(u'B').has_value());
    const auto attempts = provider.m_attempts;
    /* Act */
    const auto beyond = builder.append_unit(u'C');
    const auto second_reservation = builder.reserve_exact(4);
    /* Expect */
    expect_error(beyond, error_code::wrong_type_or_access);
    expect_error(second_reservation, error_code::wrong_type_or_access);
    EXPECT_EQ(provider.m_attempts, attempts);
    EXPECT_EQ(builder.size(), 2u);
    EXPECT_EQ(provider.m_live_bytes, 2 * sizeof(char16_t));
    auto text = builder.finish();
    expect_units(text, u"AB");
  }

  /* ----------------------------------------------------------------------------------- */

  /* Verify an unusable provider fails without publishing a unit.
   * Arrange: Construct a nonzero-limit builder without storage callbacks.
   * Act: Reserve one unit, append a unit and finish after failure.
   * Expect: Allocation error for the reservation, no appendable capacity and an
   *         empty completed string.
   */
  TEST(J5_046_StringBuilder, RejectsMissingProvider)
  {
    /* Arrange */
    decoded_string_builder builder({}, 3);
    /* Act */
    const auto reserved = builder.reserve_exact(1);
    const auto appended = builder.append_unit(u'A');
    auto text = builder.finish();
    /* Expect */
    expect_error(reserved, error_code::allocation_failure);
    expect_error(appended, error_code::wrong_type_or_access);
    EXPECT_EQ(text.size(), 0u);
  }

  /* ================================= End Test_J5_046 ================================= */
} // namespace
