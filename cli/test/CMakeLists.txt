# GoogleTest units of the host CLI (P056-P059, ADR 0009): the logic library with a fake host
# (in-memory files, captured output, failure-injecting storage). Host-only; never linked into
# the parser or schema libraries.
adaptive_json5_add_unit(adaptive_json5_cli_unit_tests cli_units
    cli_dispatch_tests.cpp
    cli_diagnostics_tests.cpp
    cli_run_tests.cpp
    cli_package_tests.cpp)
target_link_libraries(adaptive_json5_cli_unit_tests PRIVATE adaptive_json5_cli_core)
