# Data/documentation target. The future CLI is tracked in issue #24.
add_custom_target(adaptive_json5_example_files SOURCES
    README.md manifest.json
    data/parse_file.json5
    data/read_value.json5
    data/handle_error.json5
    expected/parse_file.txt
    expected/read_value.txt
    expected/handle_error.txt
    check_example.cmake
    valid/strict.json
    valid/features.json5
    valid/unicode.json5
    valid/duplicates.json5
    valid/exact-numbers.json5
    valid/bom.json5
    valid/root-string.json5
    invalid/trailing-root.json5
    invalid/array-hole.json5
    invalid/token-suffix.json5
    invalid/unterminated-string.json5
    invalid/invalid-escape.json5
    invalid/leading-zero.json
    schema/device.schema.json
    schema/device-valid.json5
    schema/device-invalid.json5
)

# Small programs for issue #95; each one uses public headers only.
foreach(program parse_file read_value handle_error)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${program}.cpp")
        add_executable(adaptive_json5_example_${program} ${program}.cpp)
        target_link_libraries(adaptive_json5_example_${program} PRIVATE AdaptiveJSON5::parser)
        adaptive_json5_configure_target(adaptive_json5_example_${program})
        # The programs read paths relative to the repository root.
        add_test(NAME example_${program}
            COMMAND ${CMAKE_COMMAND}
                -DPROGRAM=$<TARGET_FILE:adaptive_json5_example_${program}>
                -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/expected/${program}.txt
                -P ${CMAKE_CURRENT_SOURCE_DIR}/check_example.cmake
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    else()
        message(STATUS "Example program ${program}.cpp is not written yet")
    endif()
endforeach()

# The developer will add main.cpp for the future command-line executable.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    if(NOT TARGET AdaptiveJSON5::parser)
        message(FATAL_ERROR "The example executable requires the implemented AdaptiveJSON5::parser target")
    endif()
    add_executable(adaptive_json5_examples main.cpp)
    target_link_libraries(adaptive_json5_examples PRIVATE AdaptiveJSON5::parser)
    adaptive_json5_configure_target(adaptive_json5_examples)
else()
    message(STATUS "Example executable pending production C++; exposing example data only")
endif()
