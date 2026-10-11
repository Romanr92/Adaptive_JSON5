# Data/documentation target only. The future CLI is tracked in issue #24.
add_custom_target(adaptive_json5_example_files SOURCES
    README.md manifest.json
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

# The developer will add main.cpp after the parser API exists.
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
