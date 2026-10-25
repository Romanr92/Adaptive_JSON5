# Official JSON & JSON5 Validation Tests: public-interface GoogleTest cases
# over pinned third-party fixtures. Enabled only by
# ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS and run as their own quality gate.
adaptive_json5_add_unit(adaptive_json5_conformance_validation_tests
    validation_tests
    conformance_tests.cpp)
adaptive_json5_use_conformance_fixtures(
    adaptive_json5_conformance_validation_tests)
