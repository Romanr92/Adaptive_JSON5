#include <gtest/gtest.h>

/* Infrastructure units only; no production parser behavior is implemented. */

/* =================================== Test_J5_001 =================================== */

/* Verify the J5-001 development build setting only.
 * Arrange: Read the active compiler language-level macro.
 * Act: Keep its value for comparison.
 * Expect: The configured language supports C++17 or newer.
 */
TEST(J5_001_BuildConfiguration, UsesCxx17OrNewer) {
    /* Arrange */
#if defined(_MSVC_LANG)
    constexpr long language = _MSVC_LANG;
#else
    constexpr long language = __cplusplus;
#endif

    /* Act */
    const auto actual = language;

    /* Expect */
    EXPECT_GE(actual, 201703L);
}

/* ================================= End Test_J5_001 ================================= */

/* =================================== Test_J5_003 =================================== */

/* Verify the J5-003 development build setting only.
 * Arrange: Read the configured expected exception mode.
 * Act: Inspect the compiler exception-mode macros.
 * Expect: The actual compiler mode equals the CMake selection.
 */
TEST(J5_003_BuildConfiguration, AppliesSelectedExceptionMode) {
    /* Arrange */
    constexpr bool expected = ADAPTIVE_JSON5_EXPECT_EXCEPTIONS != 0;

    /* Act */
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    constexpr bool actual = true;
#else
    constexpr bool actual = false;
#endif

    /* Expect */
    EXPECT_EQ(actual, expected);
}

/* ================================= End Test_J5_003 ================================= */

/* =================================== Test_J5_101 =================================== */

/* Verify the J5-101 development build setting only.
 * Arrange: Read the configured expected RTTI mode.
 * Act: Inspect the compiler RTTI macros.
 * Expect: The actual compiler mode equals the CMake selection.
 */
TEST(J5_101_BuildConfiguration, AppliesSelectedRttiMode) {
    /* Arrange */
    constexpr bool expected = ADAPTIVE_JSON5_EXPECT_RTTI != 0;

    /* Act */
#if defined(__cpp_rtti) || defined(_CPPRTTI)
    constexpr bool actual = true;
#else
    constexpr bool actual = false;
#endif

    /* Expect */
    EXPECT_EQ(actual, expected);
}

/* ================================= End Test_J5_101 ================================= */
