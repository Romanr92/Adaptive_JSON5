# Public schema units (ADR 0006 decision 1); built only with ADAPTIVE_JSON5_BUILD_SCHEMA.
adaptive_json5_add_unit(adaptive_json5_schema_api_unit_tests schema_api_units
    schema_api_tests.cpp
    schema_registry_tests.cpp
    parser_only_tests.cpp)
target_link_libraries(adaptive_json5_schema_api_unit_tests PRIVATE AdaptiveJSON5::schema)

# Each schema public header compiles on its own, like the parser headers in tests/api.
foreach(header IN ITEMS schema.hpp schema/compiled_schema.hpp schema/schema_budget.hpp
        schema/schema_dialect.hpp schema/schema_error.hpp schema/schema_keyword.hpp
        schema/schema_registry.hpp schema/validate.hpp schema/validation_options.hpp
        schema/validation_report.hpp)
    string(MAKE_C_IDENTIFIER "${header}" header_id)
    set(header_unit "${CMAKE_CURRENT_BINARY_DIR}/public_headers/${header_id}.cpp")
    file(GENERATE OUTPUT "${header_unit}"
        CONTENT "// Public-header unit compilation: no other includes.\n#include <adaptive_json5/${header}>\n")
    target_sources(adaptive_json5_schema_api_unit_tests PRIVATE "${header_unit}")
endforeach()

# Validation engine units (P046); the concurrency case needs threads.
find_package(Threads REQUIRED)
adaptive_json5_add_unit(adaptive_json5_schema_validate_unit_tests schema_validate_units
    validate_tests.cpp)
target_link_libraries(adaptive_json5_schema_validate_unit_tests PRIVATE AdaptiveJSON5::schema Threads::Threads)

# J5-067: compile and validate use only caller storage (no global heap); the
# allocation probe replaces global operator new, so it gets its own executable.
adaptive_json5_add_unit(adaptive_json5_schema_pure_unit_tests schema_pure_units
    pure_evaluation_tests.cpp
    ../shared_infrastructure/global_allocation_probe.cpp)
target_link_libraries(adaptive_json5_schema_pure_unit_tests PRIVATE AdaptiveJSON5::schema)

# Assertion keywords (P047-P051), through the public API.
adaptive_json5_add_unit(adaptive_json5_schema_assertion_unit_tests schema_assertion_units
    type_value_tests.cpp
    numeric_tests.cpp)
target_link_libraries(adaptive_json5_schema_assertion_unit_tests PRIVATE AdaptiveJSON5::schema)
