include(FetchContent)

# Host-only dependency. This module is included only when unit tests are enabled.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Install GoogleTest" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "Use the host project MSVC runtime" FORCE)

if(ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR)
    if(NOT EXISTS "${ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR must point to an existing GoogleTest checkout")
    endif()
    set(FETCHCONTENT_SOURCE_DIR_GOOGLETEST "${ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR}")
endif()

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/tools/gtest")
FetchContent_MakeAvailable(googletest)
adaptive_json5_configure_target(gtest)
adaptive_json5_configure_target(gtest_main)
