#include "../support/guard_provider.hpp"
#include "adaptive_json5/input.hpp"
#include "adaptive_json5/options.hpp"
#include "adaptive_json5/value/floating_conversion.hpp"
#include "adaptive_json5/value/integer_conversion.hpp"
#include "adaptive_json5/value/number_comparison.hpp"
#include "adaptive_json5/value/numeric_conversion.hpp"
#include "adaptive_json5/value/owned_number.hpp"

#include <array>
#include <clocale>
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace
{
  using namespace adaptive_json5;
  /* ==================================== Fixtures ===================================== */
  using adaptive_json5_test::guard_provider;

  constexpr std::size_t c_max_spelling_bytes = 4096;

  /**
   * @brief Restore the process locale captured at construction.
   * @par Requirements
   * P038; J5-052.
   * @note Test-only: the library never calls locale functions. Restoration runs even when an assertion returns early.
   */
  class locale_restorer
  {
    private:
      std::string m_saved;

    public:
      /**
       * @brief Capture the current LC_ALL setting.
       * @par Requirements
       * P038; J5-052.
       */
      locale_restorer()
      {
        const char* current = std::setlocale(LC_ALL, nullptr);
        m_saved = (current != nullptr) ? current : "C";
      }

      locale_restorer(const locale_restorer&) = delete;
      locale_restorer& operator=(const locale_restorer&) = delete;

      /**
       * @brief Reinstate the captured LC_ALL setting.
       * @par Requirements
       * P038; J5-052.
       */
      ~locale_restorer()
      {
        std::setlocale(LC_ALL, m_saved.c_str());
      }
  };

  /**
   * @brief Select the first installed locale whose decimal separator is a comma.
   * @par Requirements
   * P038; J5-052.
   * @return True when such a locale is now active.
   * @note Names differ between C runtimes, so several common spellings are tried.
   */
  bool activate_comma_locale()
  {
    constexpr std::array<const char*, 9> c_candidates{"de_DE.UTF-8",         "de_DE.utf8",  "de_DE",      "de-DE",
                                                      "German_Germany.1252", "fr_FR.UTF-8", "fr_FR.utf8", "fr-FR",
                                                      "French_France.1252"};

    for (const char* name : c_candidates)
    {
      if ((std::setlocale(LC_ALL, name) != nullptr) && (std::strcmp(std::localeconv()->decimal_point, ",") == 0))
      {
        return true;
      }
    }

    return false;
  }

  /**
   * @brief Record conversion and comparison results that must not depend on the locale.
   * @par Requirements
   * P038; J5-052.
   */
  struct numeric_snapshot
  {
      bool valid{};
      double fraction{};
      float single{};
      int integer{};
      bool integral{};
      number_order order{};
  };

  /**
   * @brief Convert and compare fixed spellings under the current process locale.
   * @par Requirements
   * P038; J5-035; J5-036; J5-037; J5-052.
   * @return Every result, with valid set only when each operation succeeded.
   */
  numeric_snapshot take_snapshot()
  {
    guard_provider number_storage;
    guard_provider scratch;
    const auto fraction = owned_number::copy_spelling(input_view{"1.5", 3}, syntax_mode::json5,
                                                      number_storage.callbacks(), c_max_spelling_bytes);
    const auto close = owned_number::copy_spelling(input_view{"0.10000000000000000001", 22}, syntax_mode::json5,
                                                   number_storage.callbacks(), c_max_spelling_bytes);
    const auto tenth = owned_number::copy_spelling(input_view{"0.1", 3}, syntax_mode::json5, number_storage.callbacks(),
                                                   c_max_spelling_bytes);
    const auto scaled = owned_number::copy_spelling(input_view{"1.0e2", 5}, syntax_mode::json5,
                                                    number_storage.callbacks(), c_max_spelling_bytes);
    numeric_snapshot snapshot{};
    if (!fraction.has_value() || !close.has_value() || !tenth.has_value() || !scaled.has_value())
    {
      return snapshot;
    }

    const auto as_double = to_double(*fraction.value_if(), scratch.callbacks());
    const auto as_float = to_float(*tenth.value_if(), scratch.callbacks());
    const auto as_int = to_integer<int>(*scaled.value_if());
    const auto integral = is_integral_number(*scaled.value_if());
    const auto order = compare_numbers(*tenth.value_if(), *close.value_if(), scratch.callbacks());
    if (!as_double.has_value() || !as_float.has_value() || !as_int.has_value() || !integral.has_value() ||
        !order.has_value())
    {
      return snapshot;
    }

    snapshot.valid = true;
    snapshot.fraction = *as_double.value_if();
    snapshot.single = *as_float.value_if();
    snapshot.integer = *as_int.value_if();
    snapshot.integral = *integral.value_if();
    snapshot.order = *order.value_if();
    return snapshot;
  }
  /* ================================== End Fixtures =================================== */
  /* =================================== Test_J5_052 =================================== */
  /* Verify numeric operations give identical results under the "C" locale and a comma-separator locale.
   * Arrange: Take results under "C", then activate an installed locale whose decimal separator is a comma.
   * Act: Take the same results under that locale.
   * Expect: Identical successful results; skipped when no comma-separator locale is installed.
   */
  TEST(J5_052_NumericLocale, ResultsIgnoreProcessLocale)
  {
    /* Arrange */
    const locale_restorer restore;
    ASSERT_NE(std::setlocale(LC_ALL, "C"), nullptr);
    const numeric_snapshot reference = take_snapshot();
    ASSERT_TRUE(reference.valid);
    if (!activate_comma_locale())
    {
      GTEST_SKIP() << "No locale with a comma decimal separator is installed; locale independence not exercised";
    }
    /* Act */
    const numeric_snapshot localized = take_snapshot();
    /* Expect */
    ASSERT_TRUE(localized.valid);
    EXPECT_EQ(localized.fraction, 1.5);
    EXPECT_EQ(localized.fraction, reference.fraction);
    EXPECT_EQ(localized.single, reference.single);
    EXPECT_EQ(localized.integer, 100);
    EXPECT_EQ(localized.integer, reference.integer);
    EXPECT_TRUE(localized.integral);
    EXPECT_EQ(localized.order, number_order::less);
    EXPECT_EQ(localized.order, reference.order);
  }
  /* ================================= End Test_J5_052 ================================= */
} // namespace
