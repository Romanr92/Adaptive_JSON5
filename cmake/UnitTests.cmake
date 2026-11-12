# Host-only registration shared by public and internal GoogleTest units.
function(adaptive_json5_add_unit target label)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE AdaptiveJSON5::parser GTest::gtest_main)
    adaptive_json5_configure_target(${target})
    # Scratch-free number expectations depend on the compiled fast paths (ADR 0005);
    # tests/shared_infrastructure/fast_path_config.hpp reads this.
    target_compile_definitions(${target} PRIVATE
        ADAPTIVE_JSON5_EXPECT_EISEL_LEMIRE=$<BOOL:${ADAPTIVE_JSON5_EISEL_LEMIRE}>)
    # Allow process startup and scheduling delays when all host CPU workers are busy.
    # This is a hang guard, not a parser execution-time requirement.
    gtest_discover_tests(${target}
        EXTRA_ARGS --gtest_fail_if_no_test_linked
        NO_PRETTY_VALUES
        PROPERTIES LABELS ${label} TIMEOUT 60)
endfunction()
