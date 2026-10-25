adaptive_json5_add_unit(adaptive_json5_parser_engine_unit_tests parser_engine_units
    parser/parser_engine_tests.cpp
    parser/array_grammar_tests.cpp
    parser/object_grammar_tests.cpp
    parser/duplicate_policy_tests.cpp
    parser/parser_transition_tests.cpp
    parser/parser_close_tests.cpp
    parser/parser_warning_tests.cpp)
target_include_directories(adaptive_json5_parser_engine_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_primitive_parser_unit_tests primitive_parser_units
    parser/primitive_parser_tests.cpp)
target_include_directories(adaptive_json5_primitive_parser_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_resource_guards_unit_tests resource_guards_units
    memory/resource_guards_tests.cpp)
target_include_directories(adaptive_json5_resource_guards_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_token_completion_unit_tests token_completion_units
    lexer/token_completion_tests.cpp)
target_include_directories(adaptive_json5_token_completion_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_non_finite_unit_tests non_finite_units
    lexer/number/non_finite_tests.cpp)
target_include_directories(adaptive_json5_non_finite_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_hexadecimal_unit_tests hexadecimal_units
    lexer/number/hexadecimal_tests.cpp)
target_include_directories(adaptive_json5_hexadecimal_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_decimal_unit_tests decimal_units
    lexer/number/decimal_tests.cpp)
target_include_directories(adaptive_json5_decimal_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_identifier_unit_tests identifier_units
    lexer/identifier/identifier_tests.cpp)
target_include_directories(adaptive_json5_identifier_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_string_lexer_unit_tests string_lexer_units
    lexer/string/string_lexer_tests.cpp
    lexer/lexer_boundary_tests.cpp)
target_include_directories(adaptive_json5_string_lexer_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_string_warning_unit_tests string_warning_units
    lexer/string/string_warning_tests.cpp)
target_include_directories(adaptive_json5_string_warning_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_line_continuation_unit_tests line_continuation_units
    lexer/string/line_continuation_tests.cpp)
target_include_directories(adaptive_json5_line_continuation_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_non_escape_unit_tests non_escape_units
    lexer/string/non_escape_tests.cpp)
target_include_directories(adaptive_json5_non_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_unicode_escape_unit_tests unicode_escape_units
    unicode/unicode_escape_tests.cpp)
target_include_directories(adaptive_json5_unicode_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_hex_escape_unit_tests hex_escape_units
    lexer/string/hex_escape_tests.cpp)
target_include_directories(adaptive_json5_hex_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_simple_escape_unit_tests simple_escape_units
    lexer/string/simple_escape_tests.cpp)
target_include_directories(adaptive_json5_simple_escape_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_string_boundary_unit_tests string_boundary_units
    lexer/string/string_boundary_tests.cpp)
target_include_directories(adaptive_json5_string_boundary_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_string_storage_unit_tests string_storage_units
    value/string_storage_tests.cpp)
target_include_directories(adaptive_json5_string_storage_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_lexer_trivia_unit_tests lexer_trivia_units
    lexer/lexer_trivia_tests.cpp)
target_include_directories(adaptive_json5_lexer_trivia_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_checked_size_unit_tests storage_units
    common/checked_size_tests.cpp)
target_include_directories(adaptive_json5_checked_size_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_cursor_unit_tests api_configuration_units
    input/cursor_tests.cpp)
target_include_directories(adaptive_json5_cursor_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_utf8_unit_tests utf8_units
    unicode/utf8_tests.cpp)
target_include_directories(adaptive_json5_utf8_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_unicode_table_unit_tests unicode_table_units
    unicode/unicode_tables_tests.cpp)
target_include_directories(adaptive_json5_unicode_table_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_unicode_unit_tests unicode_units
    unicode/unicode_tests.cpp)
target_include_directories(adaptive_json5_unicode_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_line_terminator_unit_tests line_terminator_units
    lexer/line_terminator_tests.cpp)
target_include_directories(adaptive_json5_line_terminator_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_whitespace_unit_tests whitespace_units
    lexer/whitespace_tests.cpp)
target_include_directories(adaptive_json5_whitespace_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_value_storage_unit_tests document_units
    value/value_storage_tests.cpp
    value/relationship_validation_tests.cpp
    value/attachment_validation_tests.cpp)
target_include_directories(adaptive_json5_value_storage_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")

adaptive_json5_add_unit(adaptive_json5_numeric_internal_unit_tests numeric_units
    value/big_unsigned_tests.cpp
    value/number_analysis_tests.cpp)
target_include_directories(adaptive_json5_numeric_internal_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/core/includes")
