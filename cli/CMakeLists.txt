# Host-only command-line tool (P056-P059, ADR 0007). The logic library holds argument
# parsing, dispatch and output formatting behind injectable I/O seams; it is linked by the
# executable and by the units in core/tests/cli. Only the executable touches files.
add_library(adaptive_json5_cli_core STATIC
    src/cli_app.cpp
    src/cli_options.cpp
    src/report.cpp
    src/validate_command.cpp)
target_include_directories(adaptive_json5_cli_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/includes")
target_link_libraries(adaptive_json5_cli_core PUBLIC AdaptiveJSON5::parser)
if(TARGET AdaptiveJSON5::schema)
    target_link_libraries(adaptive_json5_cli_core PUBLIC AdaptiveJSON5::schema)
    target_compile_definitions(adaptive_json5_cli_core PUBLIC ADAPTIVE_JSON5_CLI_HAS_SCHEMA=1)
endif()
adaptive_json5_configure_target(adaptive_json5_cli_core)

if(ADAPTIVE_JSON5_BUILD_EXECUTABLE)
    add_executable(adaptive_json5_examples
        src/main.cpp
        src/host_io.cpp)
    target_link_libraries(adaptive_json5_examples PRIVATE adaptive_json5_cli_core)
    adaptive_json5_configure_target(adaptive_json5_examples)
endif()
