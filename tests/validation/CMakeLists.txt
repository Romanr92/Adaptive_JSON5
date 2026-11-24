# Official JSON & JSON5 Validation Tests: public-interface GoogleTest cases
# over pinned third-party fixtures. Enabled only by
# ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS and run as their own quality gate.
adaptive_json5_add_unit(adaptive_json5_conformance_validation_tests
    validation_tests
    conformance_tests.cpp)
adaptive_json5_use_conformance_fixtures(
    adaptive_json5_conformance_validation_tests)

# JSON Schema Draft-07 conformance (P055, J5-096) over the pinned JSON-Schema-Test-Suite;
# only with the optional schema component.
if(ADAPTIVE_JSON5_BUILD_SCHEMA)
    adaptive_json5_add_unit(adaptive_json5_schema_conformance_validation_tests
        schema_validation_tests
        schema_conformance_tests.cpp)
    target_link_libraries(adaptive_json5_schema_conformance_validation_tests PRIVATE AdaptiveJSON5::schema)
    adaptive_json5_use_schema_conformance_fixtures(
        adaptive_json5_schema_conformance_validation_tests)
endif()
