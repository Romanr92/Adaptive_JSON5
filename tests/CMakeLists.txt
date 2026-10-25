adaptive_json5_add_unit(adaptive_json5_document_unit_tests document_units
    value/document_tests.cpp
    value/access_boundary_tests.cpp
    value/member_name_span_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_owned_number_unit_tests owned_number_units
    value/owned_number_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_diagnostics_unit_tests diagnostics_units
    api/error_range_tests.cpp
    input/text_position_tests.cpp
    value/warning_list_tests.cpp
    value/retained_source_tests.cpp
    parser/document_diagnostics_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_numeric_conversion_unit_tests numeric_units
    value/integer_conversion_tests.cpp
    value/floating_conversion_tests.cpp
    value/number_comparison_tests.cpp
    value/numeric_locale_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_unit_tests build_configuration_units
    library/build_configuration_tests.cpp)
target_compile_definitions(adaptive_json5_unit_tests PRIVATE
    ADAPTIVE_JSON5_EXPECT_EXCEPTIONS=$<BOOL:${ADAPTIVE_JSON5_ENABLE_EXCEPTIONS}>
    ADAPTIVE_JSON5_EXPECT_RTTI=$<BOOL:${ADAPTIVE_JSON5_ENABLE_RTTI}>)

adaptive_json5_add_unit(adaptive_json5_result_unit_tests result_units
    api/result_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_api_unit_tests api_configuration_units
    input/input_view_tests.cpp
    input/parse_options_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_storage_unit_tests storage_units
    memory/storage_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_public_string_storage_unit_tests string_storage_units
    value/string_storage_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_accounted_storage_unit_tests resource_guards_units
    memory/accounted_storage_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_public_library_unit_tests public_library_units
    library/public_library_tests.cpp)

adaptive_json5_add_unit(adaptive_json5_complete_document_unit_tests complete_document_units
    parser/complete_document_tests.cpp
    parser/surrogate_warning_tests.cpp)

# Host concurrency units need threads; the parser target acquires no thread dependency.
find_package(Threads REQUIRED)
adaptive_json5_add_unit(adaptive_json5_concurrency_unit_tests concurrency_units
    parser/concurrency_tests.cpp)
target_link_libraries(adaptive_json5_concurrency_unit_tests PRIVATE Threads::Threads)

adaptive_json5_add_unit(adaptive_json5_pure_memory_unit_tests
    pure_memory_units
    library/pure_memory_tests.cpp)
# The import scan reads production sources; parsing itself stays in memory.
target_compile_definitions(adaptive_json5_pure_memory_unit_tests PRIVATE
    "ADAPTIVE_JSON5_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")

adaptive_json5_add_unit(adaptive_json5_grammar_evidence_unit_tests
    grammar_evidence_units
    parser/grammar_evidence_tests.cpp)

# Compile every public header in its own test translation unit, without private paths.
set(adaptive_json5_public_headers
    error.hpp
    input.hpp
    options.hpp
    parser.hpp
    result.hpp
    text_position.hpp
    memory/accounted_storage.hpp
    memory/owned_block.hpp
    memory/resource_limits.hpp
    memory/storage.hpp
    value/decoded_name_equal.hpp
    value/decoded_string.hpp
    value/document.hpp
    value/floating_conversion.hpp
    value/floating_conversion_policy.hpp
    value/integer_conversion.hpp
    value/member_lookup.hpp
    value/number_comparison.hpp
    value/number_kind.hpp
    value/numeric_conversion.hpp
    value/numeric_conversion_reason.hpp
    value/owned_number.hpp
    value/retained_source.hpp
    value/string_warning.hpp
    value/utf8_export.hpp
    value/value_index.hpp
    value/value_iterator.hpp
    value/value_kind.hpp
    value/value_view.hpp
    value/warning_list.hpp)
foreach(header IN LISTS adaptive_json5_public_headers)
    string(MAKE_C_IDENTIFIER "${header}" header_id)
    set(header_unit "${CMAKE_CURRENT_BINARY_DIR}/public_headers/${header_id}.cpp")
    file(GENERATE OUTPUT "${header_unit}"
        CONTENT "// Public-header unit compilation: no other includes.\n#include <adaptive_json5/${header}>\n")
    target_sources(adaptive_json5_public_library_unit_tests PRIVATE "${header_unit}")
endforeach()
