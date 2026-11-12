#ifndef ADAPTIVE_JSON5_TEST_FAST_PATH_CONFIG_HPP_
#define ADAPTIVE_JSON5_TEST_FAST_PATH_CONFIG_HPP_

#include <cfloat>

// cmake/UnitTests.cmake defines this for every unit target from ADAPTIVE_JSON5_EISEL_LEMIRE.
#ifndef ADAPTIVE_JSON5_EXPECT_EISEL_LEMIRE
  #error "ADAPTIVE_JSON5_EXPECT_EISEL_LEMIRE must be defined by the unit-test build"
#endif

namespace adaptive_json5_test
{
  /**
   * @brief Whether Clinger's exact fast path is compiled in for this host.
   * @par Requirements
   * J5-037.
   * @note It needs floating arithmetic without extra precision (FLT_EVAL_METHOD == 0), the
   * same condition the library uses.
   */
#if defined(FLT_EVAL_METHOD) && (FLT_EVAL_METHOD == 0)
  constexpr bool c_clinger_enabled = true;
#else
  constexpr bool c_clinger_enabled = false;
#endif

  /**
   * @brief Whether the library was built with the Eisel-Lemire step (ADAPTIVE_JSON5_EISEL_LEMIRE).
   * @par Requirements
   * J5-037.
   */
  constexpr bool c_eisel_lemire_enabled = (ADAPTIVE_JSON5_EXPECT_EISEL_LEMIRE != 0);

  /**
   * @brief Whether a short decimal such as 1.5 converts without scratch on this build.
   * @par Requirements
   * J5-037.
   * @note Clinger covers it when compiled in; otherwise Eisel-Lemire does when built. With
   * neither, every finite nonzero number takes the exact path and needs scratch.
   */
  constexpr bool c_short_decimal_scratch_free = c_clinger_enabled || c_eisel_lemire_enabled;
} // namespace adaptive_json5_test

#endif /* ADAPTIVE_JSON5_TEST_FAST_PATH_CONFIG_HPP_ */
