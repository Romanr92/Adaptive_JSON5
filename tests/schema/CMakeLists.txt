# Public schema units (ADR 0006 decision 1); built only with ADAPTIVE_JSON5_BUILD_SCHEMA.
adaptive_json5_add_unit(adaptive_json5_schema_api_unit_tests schema_api_units
    schema_api_tests.cpp
    parser_only_tests.cpp)
target_link_libraries(adaptive_json5_schema_api_unit_tests PRIVATE AdaptiveJSON5::schema)

# Each schema public header compiles on its own, like the parser headers in tests/api.
foreach(header IN ITEMS schema.hpp schema/schema_budget.hpp schema/schema_dialect.hpp
        schema/schema_error.hpp schema/validation_options.hpp)
    string(MAKE_C_IDENTIFIER "${header}" header_id)
    set(header_unit "${CMAKE_CURRENT_BINARY_DIR}/public_headers/${header_id}.cpp")
    file(GENERATE OUTPUT "${header_unit}"
        CONTENT "// Public-header unit compilation: no other includes.\n#include <adaptive_json5/${header}>\n")
    target_sources(adaptive_json5_schema_api_unit_tests PRIVATE "${header_unit}")
endforeach()
