# Host-only command-line tool (P056-P059, ADR 0009). The logic library holds argument
# parsing, dispatch and output formatting behind injectable I/O seams; it is linked by the
# executable and by the units in cli/test. Only the executable touches files.
# The tool is versioned independently of the library (P059, J5-093).
set(ADAPTIVE_JSON5_CLI_VERSION "0.1.0" CACHE STRING "Version of the adaptive_json5_examples tool")
configure_file(cli_version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/cli/cli_version.hpp" @ONLY)

add_library(adaptive_json5_cli_core STATIC
    src/cli_app.cpp
    src/cli_options.cpp
    src/report.cpp
    src/validate_command.cpp
    src/inspect_command.cpp
    src/run_command.cpp)
target_include_directories(adaptive_json5_cli_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/includes"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(adaptive_json5_cli_core PUBLIC AdaptiveJSON5::parser)
if(TARGET AdaptiveJSON5::schema)
    target_link_libraries(adaptive_json5_cli_core PUBLIC AdaptiveJSON5::schema)
    target_compile_definitions(adaptive_json5_cli_core PUBLIC ADAPTIVE_JSON5_CLI_HAS_SCHEMA=1)
endif()
adaptive_json5_configure_target(adaptive_json5_cli_core)

# The CLI's own GoogleTest units (owner decision 2026-11-25: they live in cli/test).
if(ADAPTIVE_JSON5_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(ADAPTIVE_JSON5_BUILD_EXECUTABLE)
    add_executable(adaptive_json5_examples
        src/main.cpp
        src/host_io.cpp)
    target_link_libraries(adaptive_json5_examples PRIVATE adaptive_json5_cli_core)
    adaptive_json5_configure_target(adaptive_json5_examples)
    # Installed as its own component with its own version (P059): parser and embedded
    # consumers never need it.
    include(GNUInstallDirs)
    install(TARGETS adaptive_json5_examples RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT cli)

    # Smoke tests: the real executable on known example files, compared with checked-in
    # expected output (cli/test/expected). Run with: ctest -L cli_smoke.
    enable_testing()
    include(test/cli_smoke.cmake)
endif()
