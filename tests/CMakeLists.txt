include("${PROJECT_SOURCE_DIR}/cmake/Dependencies.cmake")
include(GoogleTest)

add_executable(adaptive_json5_non_finite_unit_tests non_finite_tests.cpp)
target_include_directories(adaptive_json5_non_finite_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_non_finite_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_non_finite_unit_tests)
gtest_discover_tests(adaptive_json5_non_finite_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS non_finite_units TIMEOUT 10)

add_executable(adaptive_json5_hexadecimal_unit_tests hexadecimal_tests.cpp)
target_include_directories(adaptive_json5_hexadecimal_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_hexadecimal_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_hexadecimal_unit_tests)
gtest_discover_tests(adaptive_json5_hexadecimal_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS hexadecimal_units TIMEOUT 10)

add_executable(adaptive_json5_decimal_unit_tests decimal_tests.cpp)
target_include_directories(adaptive_json5_decimal_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_decimal_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_decimal_unit_tests)
gtest_discover_tests(adaptive_json5_decimal_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS decimal_units TIMEOUT 10)

add_executable(adaptive_json5_identifier_unit_tests identifier_tests.cpp)
target_include_directories(adaptive_json5_identifier_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_identifier_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_identifier_unit_tests)
gtest_discover_tests(adaptive_json5_identifier_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS identifier_units TIMEOUT 10)

add_executable(adaptive_json5_string_lexer_unit_tests string_lexer_tests.cpp)
target_include_directories(adaptive_json5_string_lexer_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_string_lexer_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_string_lexer_unit_tests)
gtest_discover_tests(adaptive_json5_string_lexer_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS string_lexer_units TIMEOUT 10)

add_executable(adaptive_json5_string_warning_unit_tests string_warning_tests.cpp)
target_include_directories(adaptive_json5_string_warning_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_string_warning_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_string_warning_unit_tests)
gtest_discover_tests(adaptive_json5_string_warning_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS string_warning_units TIMEOUT 10)

add_executable(adaptive_json5_line_continuation_unit_tests line_continuation_tests.cpp)
target_include_directories(adaptive_json5_line_continuation_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_line_continuation_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_line_continuation_unit_tests)
gtest_discover_tests(adaptive_json5_line_continuation_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS line_continuation_units TIMEOUT 10)

add_executable(adaptive_json5_non_escape_unit_tests non_escape_tests.cpp)
target_include_directories(adaptive_json5_non_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_non_escape_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_non_escape_unit_tests)
gtest_discover_tests(adaptive_json5_non_escape_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS non_escape_units TIMEOUT 10)

add_executable(adaptive_json5_unicode_escape_unit_tests unicode_escape_tests.cpp)
target_include_directories(adaptive_json5_unicode_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_unicode_escape_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_unicode_escape_unit_tests)
gtest_discover_tests(adaptive_json5_unicode_escape_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS unicode_escape_units TIMEOUT 10)

add_executable(adaptive_json5_hex_escape_unit_tests hex_escape_tests.cpp)
target_include_directories(adaptive_json5_hex_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_hex_escape_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_hex_escape_unit_tests)
gtest_discover_tests(adaptive_json5_hex_escape_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS hex_escape_units TIMEOUT 10)

add_executable(adaptive_json5_simple_escape_unit_tests simple_escape_tests.cpp)
target_include_directories(adaptive_json5_simple_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_simple_escape_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_simple_escape_unit_tests)
gtest_discover_tests(adaptive_json5_simple_escape_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS simple_escape_units TIMEOUT 10)

add_executable(adaptive_json5_string_boundary_unit_tests string_boundary_tests.cpp)
target_include_directories(adaptive_json5_string_boundary_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_string_boundary_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_string_boundary_unit_tests)
gtest_discover_tests(adaptive_json5_string_boundary_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS string_boundary_units TIMEOUT 10)

add_executable(adaptive_json5_string_storage_unit_tests string_storage_tests.cpp)
target_include_directories(adaptive_json5_string_storage_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_string_storage_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_string_storage_unit_tests)
gtest_discover_tests(adaptive_json5_string_storage_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS string_storage_units TIMEOUT 10)

add_executable(adaptive_json5_lexer_trivia_unit_tests lexer_trivia_tests.cpp)
target_include_directories(adaptive_json5_lexer_trivia_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_lexer_trivia_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_lexer_trivia_unit_tests)
gtest_discover_tests(adaptive_json5_lexer_trivia_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS lexer_trivia_units TIMEOUT 10)

add_executable(adaptive_json5_unit_tests build_configuration_tests.cpp)
target_link_libraries(adaptive_json5_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_unit_tests)
target_compile_definitions(adaptive_json5_unit_tests PRIVATE
    ADAPTIVE_JSON5_EXPECT_EXCEPTIONS=$<BOOL:${ADAPTIVE_JSON5_ENABLE_EXCEPTIONS}>
    ADAPTIVE_JSON5_EXPECT_RTTI=$<BOOL:${ADAPTIVE_JSON5_ENABLE_RTTI}>)
gtest_discover_tests(adaptive_json5_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS build_configuration_units)

add_executable(adaptive_json5_result_unit_tests result_tests.cpp)
target_include_directories(adaptive_json5_result_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_result_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_result_unit_tests)
gtest_discover_tests(adaptive_json5_result_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS result_units)

add_executable(adaptive_json5_storage_unit_tests storage_tests.cpp)
target_include_directories(adaptive_json5_storage_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_storage_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_storage_unit_tests)
gtest_discover_tests(adaptive_json5_storage_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS storage_units)

add_executable(adaptive_json5_api_unit_tests input_view_tests.cpp parse_options_tests.cpp cursor_tests.cpp)
target_include_directories(adaptive_json5_api_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_api_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_api_unit_tests)
gtest_discover_tests(adaptive_json5_api_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS api_configuration_units)

# Contract-first P005 units intentionally require the developer's utf8.hpp.
add_executable(adaptive_json5_utf8_unit_tests utf8_tests.cpp)
target_include_directories(adaptive_json5_utf8_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_utf8_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_utf8_unit_tests)

gtest_discover_tests(adaptive_json5_utf8_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS utf8_units)

add_executable(adaptive_json5_unicode_table_unit_tests unicode_tables_tests.cpp)
target_include_directories(adaptive_json5_unicode_table_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_unicode_table_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_unicode_table_unit_tests)

gtest_discover_tests(adaptive_json5_unicode_table_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS unicode_table_units)

add_executable(adaptive_json5_unicode_unit_tests unicode_tests.cpp)
target_include_directories(adaptive_json5_unicode_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_unicode_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_unicode_unit_tests)

gtest_discover_tests(adaptive_json5_unicode_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS unicode_units TIMEOUT 10)

add_executable(adaptive_json5_line_terminator_unit_tests line_terminator_tests.cpp)
target_include_directories(adaptive_json5_line_terminator_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_line_terminator_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_line_terminator_unit_tests)

gtest_discover_tests(adaptive_json5_line_terminator_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS line_terminator_units TIMEOUT 10)

add_executable(adaptive_json5_whitespace_unit_tests whitespace_tests.cpp)
target_include_directories(adaptive_json5_whitespace_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(adaptive_json5_whitespace_unit_tests PRIVATE AdaptiveJSON5::build_options GTest::gtest_main)
adaptive_json5_configure_target(adaptive_json5_whitespace_unit_tests)

gtest_discover_tests(adaptive_json5_whitespace_unit_tests
    EXTRA_ARGS --gtest_fail_if_no_test_linked
    NO_PRETTY_VALUES
    PROPERTIES LABELS whitespace_units TIMEOUT 10)
