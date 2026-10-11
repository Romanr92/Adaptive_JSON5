if(NOT ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR)
    find_package(GTest 1.17.0 EXACT CONFIG REQUIRED)
else()
    if(NOT EXISTS "${ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR must point to an existing GoogleTest checkout")
    endif()
    # Local test dependency only. CMake never downloads it.
    set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Install GoogleTest" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "Use the host project's MSVC runtime" FORCE)
    add_subdirectory("${ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)
    adaptive_json5_configure_target(gtest)
    adaptive_json5_configure_target(gtest_main)
endif()

add_executable(adaptive_json5_unit_tests build_configuration_tests.cpp)
target_link_libraries(adaptive_json5_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_unit_tests)
target_compile_definitions(adaptive_json5_unit_tests PRIVATE
    ADAPTIVE_JSON5_EXPECT_EXCEPTIONS=$<BOOL:${ADAPTIVE_JSON5_ENABLE_EXCEPTIONS}>
    ADAPTIVE_JSON5_EXPECT_RTTI=$<BOOL:${ADAPTIVE_JSON5_ENABLE_RTTI}>)
add_test(NAME build_configuration_units COMMAND adaptive_json5_unit_tests --gtest_fail_if_no_test_linked)
