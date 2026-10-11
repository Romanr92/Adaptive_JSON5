include("${PROJECT_SOURCE_DIR}/cmake/Dependencies.cmake")

add_executable(adaptive_json5_unit_tests build_configuration_tests.cpp)
target_link_libraries(adaptive_json5_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_unit_tests)
target_compile_definitions(adaptive_json5_unit_tests PRIVATE
    ADAPTIVE_JSON5_EXPECT_EXCEPTIONS=$<BOOL:${ADAPTIVE_JSON5_ENABLE_EXCEPTIONS}>
    ADAPTIVE_JSON5_EXPECT_RTTI=$<BOOL:${ADAPTIVE_JSON5_ENABLE_RTTI}>)
add_test(NAME build_configuration_units COMMAND adaptive_json5_unit_tests --gtest_fail_if_no_test_linked)
