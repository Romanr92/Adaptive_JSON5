# Host-only third-party conformance fixtures, included only when the
# Official JSON & JSON5 Validation Tests are enabled. The committed copies
# under tests/fixtures/third_party are preferred. When a copy is missing, the
# pinned archive is downloaded with a verified SHA256 into the build tree.
# Provenance and licenses: tests/fixtures/third_party/README.md and LICENSES/.
include(FetchContent)

set(ADAPTIVE_JSON5_JSON5_TESTS_SOURCE_DIR "" CACHE PATH
    "Offline json5-tests checkout to use instead of the committed copy")
set(ADAPTIVE_JSON5_JSONTESTSUITE_SOURCE_DIR "" CACHE PATH
    "Offline JSONTestSuite checkout to use instead of the committed copy")
set(ADAPTIVE_JSON5_JSON_SCHEMA_TEST_SUITE_SOURCE_DIR "" CACHE PATH
    "Offline JSON-Schema-Test-Suite checkout to use instead of the committed copy")

# Resolve one fixture set and store its root directory in out_var.
# marker is a path relative to the root that must exist in a usable copy.
function(adaptive_json5_resolve_fixture
        name override committed marker url sha256 out_var)
    if(override)
        if(NOT EXISTS "${override}/${marker}")
            message(FATAL_ERROR
                "${name} override must contain ${marker}: ${override}")
        endif()
        set(${out_var} "${override}" PARENT_SCOPE)
        return()
    endif()
    if(EXISTS "${committed}/${marker}")
        set(${out_var} "${committed}" PARENT_SCOPE)
        return()
    endif()
    message(STATUS
        "${name} fixtures missing from the source tree; downloading the "
        "pinned archive")
    string(TOLOWER "adaptive_json5_fixture_${name}" content_name)
    # SOURCE_SUBDIR names a directory without CMakeLists.txt, so nothing is
    # added to the build.
    FetchContent_Declare(${content_name}
        URL "${url}"
        URL_HASH SHA256=${sha256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR adaptive_json5_no_build)
    FetchContent_MakeAvailable(${content_name})
    FetchContent_GetProperties(${content_name} SOURCE_DIR fetched_dir)
    if(NOT EXISTS "${fetched_dir}/${marker}")
        message(FATAL_ERROR "${name} archive does not contain ${marker}")
    endif()
    set(${out_var} "${fetched_dir}" PARENT_SCOPE)
endfunction()

set(adaptive_json5_fixture_root
    "${PROJECT_SOURCE_DIR}/tests/fixtures/third_party")

adaptive_json5_resolve_fixture(json5_tests
    "${ADAPTIVE_JSON5_JSON5_TESTS_SOURCE_DIR}"
    "${adaptive_json5_fixture_root}/json5-tests"
    "LICENSE.md"
    "https://github.com/json5/json5-tests/archive/\
ceb24d4080137d70833f86c25659c1331b80a387.tar.gz"
    30d041035ce29dc4f01d1836b7885c89689fc1bdee4c384bc2bc113c737c0a28
    ADAPTIVE_JSON5_JSON5_TESTS_DIR)

adaptive_json5_resolve_fixture(jsontestsuite
    "${ADAPTIVE_JSON5_JSONTESTSUITE_SOURCE_DIR}"
    "${adaptive_json5_fixture_root}/JSONTestSuite"
    "test_parsing"
    "https://github.com/nst/JSONTestSuite/archive/\
1ef36fa01286573e846ac449e8683f8833c5b26a.tar.gz"
    32ab2fb37cc26679dbd9164dbb7053bcef10134643d53cb0cef45de7680dcdcc
    ADAPTIVE_JSON5_JSONTESTSUITE_DIR)

# JSON Schema Draft-07 conformance (P055, J5-096): release Test-JSON-Schema-Acceptance-1.041,
# the newest release with the tests/draft7 and remotes layout.
adaptive_json5_resolve_fixture(json_schema_test_suite
    "${ADAPTIVE_JSON5_JSON_SCHEMA_TEST_SUITE_SOURCE_DIR}"
    "${adaptive_json5_fixture_root}/JSON-Schema-Test-Suite"
    "tests/draft7/type.json"
    "https://github.com/json-schema-org/JSON-Schema-Test-Suite/archive/245a45abc90190c0bff04d97e6d2b541b4bb9f12.tar.gz"
    b60bdf58fd414e9b86c8be70b03e3b2d596b8bc3bc822a2083d86041cea4b20e
    ADAPTIVE_JSON5_JSON_SCHEMA_TEST_SUITE_DIR)

message(STATUS "json5-tests fixtures: ${ADAPTIVE_JSON5_JSON5_TESTS_DIR}")
message(STATUS
    "JSONTestSuite fixtures: ${ADAPTIVE_JSON5_JSONTESTSUITE_DIR}/test_parsing")

# Expose the resolved roots to a fixture-driven test target.
function(adaptive_json5_use_conformance_fixtures target)
    set(json5_dir "${ADAPTIVE_JSON5_JSON5_TESTS_DIR}")
    set(suite_dir "${ADAPTIVE_JSON5_JSONTESTSUITE_DIR}/test_parsing")
    target_compile_definitions(${target} PRIVATE
        "ADAPTIVE_JSON5_JSON5_TESTS_DIR=\"${json5_dir}\""
        "ADAPTIVE_JSON5_JSONTESTSUITE_DIR=\"${suite_dir}\"")
endfunction()

# Expose the JSON-Schema-Test-Suite root and the embedded meta-schema source (whose
# Draft-07 text the suite's remote references need) to a schema conformance target.
function(adaptive_json5_use_schema_conformance_fixtures target)
    target_compile_definitions(${target} PRIVATE
        "ADAPTIVE_JSON5_JSON_SCHEMA_TEST_SUITE_DIR=\"${ADAPTIVE_JSON5_JSON_SCHEMA_TEST_SUITE_DIR}\""
        "ADAPTIVE_JSON5_META_SCHEMA_SOURCE=\"${PROJECT_SOURCE_DIR}/core/src/schema/meta_schema.cpp\"")
endfunction()
