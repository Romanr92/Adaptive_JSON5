# Host-only registration shared by public and internal GoogleTest units.
function(adaptive_json5_add_unit target label)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE AdaptiveJSON5::parser GTest::gtest_main)
    adaptive_json5_configure_target(${target})
    gtest_discover_tests(${target}
        EXTRA_ARGS --gtest_fail_if_no_test_linked
        NO_PRETTY_VALUES
        PROPERTIES LABELS ${label} TIMEOUT 10)
endfunction()
