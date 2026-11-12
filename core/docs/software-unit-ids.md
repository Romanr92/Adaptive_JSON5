# Software unit ID registry

Every production function has a unique software unit ID of the form
`SWU-<MODULE>-<number>`, written in its Doxygen block on a single line,
`@par ID SWU-<MODULE>-<number>`. When a
function is declared in a header and defined in a `.cpp` file, both Doxygen blocks
carry the same ID. The IDs are used for traceability (see
[safety traceability](../../docs/traceability.md#safety-traceability-iso-26262)).

## Rules

- `<MODULE>` is the source folder that holds the function's definition: `core/src/<topic>/`,
  `core/includes/<topic>/` or `includes/adaptive_json5/<topic>/`, written in capitals.
  Functions defined only in a header directly under `includes/adaptive_json5/` use `API`.
- `<number>` has three digits and counts up per module.
- A new function takes the **next free ID** of its module from the table below, and a
  new row is added here with status **used**.
- When a function is deleted, its row stays and its status becomes **deleted**. An ID is
  **never reused**, not even for a function with the same name.
- When a function moves to another module, it keeps its ID.
- Deleted (`= delete`) and defaulted (`= default`) special members, and lambdas, get no ID.

The initial IDs were assigned on 2026-11-07 for issue #138, as a one-time exception
allowed by Romanr92: only comments were added to production files, and the compiled
library was checked to be byte-identical before and after.

## Next free ID per module

| Module | Last ID | Next free ID |
| --- | --- | --- |
| API | SWU-API-022 | SWU-API-023 |
| COMMON | SWU-COMMON-002 | SWU-COMMON-003 |
| DIAGNOSTICS | SWU-DIAGNOSTICS-015 | SWU-DIAGNOSTICS-016 |
| HOSTED | SWU-HOSTED-005 | SWU-HOSTED-006 |
| INPUT | SWU-INPUT-011 | SWU-INPUT-012 |
| LEXER | SWU-LEXER-051 | SWU-LEXER-052 |
| MEMORY | SWU-MEMORY-075 | SWU-MEMORY-076 |
| PARSER | SWU-PARSER-046 | SWU-PARSER-047 |
| UNICODE | SWU-UNICODE-008 | SWU-UNICODE-009 |
| VALUE | SWU-VALUE-157 | SWU-VALUE-158 |

## API

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-API-001 | used | `error::at_offset(error_code, std::size_t)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-002 | used | `error::at_range(error_code, std::size_t, source_span)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-003 | used | `error::without_location(error_code)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-004 | used | `error::duplicate_member(source_span, source_span)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-005 | used | `error::numeric_failure(numeric_conversion_reason)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-006 | used | `error::syntax_at(std::size_t, syntax_error_reason)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-007 | used | `error::syntax_at_range(std::size_t, source_span, syntax_error_reason)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-008 | used | `describe(const error &, input_view, char (&)[N])` | `includes/adaptive_json5/json5_document.hpp` | — |
| SWU-API-009 | used | `describe(const string_warning &, input_view, char (&)[N])` | `includes/adaptive_json5/json5_document.hpp` | — |
| SWU-API-010 | used | `result::result<T, E>(std::in_place_index_t<c_value_index>, T &&)` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-011 | used | `result::result<T, E>(std::in_place_index_t<c_error_index>, E &&)` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-012 | used | `result::success(T)` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-013 | used | `result::failure(E)` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-014 | used | `result::has_value()` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-015 | used | `result::value_if()` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-016 | used | `result::value_if()` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-017 | used | `result::error_if()` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-018 | used | `result::success()` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-019 | used | `result::failure(E)` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-020 | used | `result::has_value()` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-021 | used | `result::error_if()` | `includes/adaptive_json5/result.hpp` | — |
| SWU-API-022 | used | `result::result<void, E>(E)` | `includes/adaptive_json5/result.hpp` | — |

## COMMON

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-COMMON-001 | used | `checked_add(std::size_t, std::size_t)` | `core/includes/common/checked_size.hpp` | — |
| SWU-COMMON-002 | used | `checked_multiply(std::size_t, std::size_t)` | `core/includes/common/checked_size.hpp` | — |

## DIAGNOSTICS

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-DIAGNOSTICS-001 | used | `(anonymous)::message_writer::message_writer(char *, std::size_t)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-002 | used | `(anonymous)::message_writer::create(char *, std::size_t)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-003 | used | `(anonymous)::message_writer::append(std::string_view)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-004 | used | `(anonymous)::message_writer::append_number(std::size_t)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-005 | used | `(anonymous)::message_writer::finish()` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-006 | used | `(anonymous)::describe(error_code)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-007 | used | `(anonymous)::describe(syntax_error_reason)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-008 | used | `(anonymous)::describe(numeric_conversion_reason)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-009 | used | `(anonymous)::describe(string_warning_code)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-010 | used | `(anonymous)::describe(const error &)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-011 | used | `(anonymous)::append_position(message_writer &, text_position)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-012 | used | `format_error_at(const error &, text_position, std::optional<text_position>, char *, std::size_t)` | `core/src/diagnostics/diagnostic_format.cpp` | `includes/adaptive_json5/diagnostic_format.hpp` |
| SWU-DIAGNOSTICS-013 | used | `format_error(const error &, input_view, char *, std::size_t)` | `core/src/diagnostics/diagnostic_format.cpp` | `includes/adaptive_json5/diagnostic_format.hpp` |
| SWU-DIAGNOSTICS-014 | used | `format_warning_at(const string_warning &, text_position, char *, std::size_t)` | `core/src/diagnostics/diagnostic_format.cpp` | `includes/adaptive_json5/diagnostic_format.hpp` |
| SWU-DIAGNOSTICS-015 | used | `format_warning(const string_warning &, input_view, char *, std::size_t)` | `core/src/diagnostics/diagnostic_format.cpp` | `includes/adaptive_json5/diagnostic_format.hpp` |

## HOSTED

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-HOSTED-001 | used | `(anonymous)::hosted_allocate(void *, std::size_t, std::size_t)` | `core/src/hosted/parse.cpp` | — |
| SWU-HOSTED-002 | used | `(anonymous)::hosted_deallocate(void *, void *, std::size_t, std::size_t)` | `core/src/hosted/parse.cpp` | — |
| SWU-HOSTED-003 | used | `(anonymous)::hosted_storage_block::hosted_storage_block(std::size_t)` | `core/src/hosted/parse.cpp` | — |
| SWU-HOSTED-004 | used | `(anonymous)::release_hosted_block(void *)` | `core/src/hosted/parse.cpp` | — |
| SWU-HOSTED-005 | used | `hosted::parse(input_view, const parse_options &)` | `core/src/hosted/parse.cpp` | `includes/adaptive_json5/hosted/parse.hpp` |

## INPUT

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-INPUT-001 | used | `cursor::cursor(input_view)` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-002 | used | `cursor::is_at_end()` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-003 | used | `cursor::offset()` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-004 | used | `cursor::remaining()` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-005 | used | `cursor::peek()` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-006 | used | `cursor::peek(std::size_t)` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-007 | used | `cursor::next()` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-008 | used | `cursor::advance(std::size_t)` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-009 | used | `cursor::span_from(std::size_t)` | `core/includes/input/cursor.hpp` | — |
| SWU-INPUT-010 | used | `(anonymous)::is_line_break(char32_t)` | `core/src/input/text_position.cpp` | — |
| SWU-INPUT-011 | used | `locate(input_view, std::size_t)` | `core/src/input/text_position.cpp` | `includes/adaptive_json5/text_position.hpp` |

## LEXER

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-LEXER-001 | used | `detail::is_decimal_digit(std::optional<char>)` | `core/includes/lexer/number/decimal_lexer.hpp` | — |
| SWU-LEXER-002 | used | `is_json5_space(char32_t)` | `core/includes/lexer/whitespace.hpp` | — |
| SWU-LEXER-003 | used | `consume_line_comment(cursor &)` | `core/src/lexer/comment.cpp` | `core/includes/lexer/comment.hpp` |
| SWU-LEXER-004 | used | `consume_block_comment(cursor &)` | `core/src/lexer/comment.cpp` | `core/includes/lexer/comment.hpp` |
| SWU-LEXER-005 | used | `detail::identifier_spelling_matches(const decoded_string &, std::u16string_view)` | `core/src/lexer/identifier/identifier_classification.cpp` | `core/includes/lexer/identifier/identifier_classification.hpp` |
| SWU-LEXER-006 | used | `classify_identifier(const identifier_token &, identifier_context, syntax_mode)` | `core/src/lexer/identifier/identifier_classification.cpp` | `core/includes/lexer/identifier/identifier_classification.hpp` |
| SWU-LEXER-007 | used | `(anonymous)::walk_identifier(cursor &, std::size_t, decoded_string_builder *)` | `core/src/lexer/identifier/identifier_lexer.cpp` | — |
| SWU-LEXER-008 | used | `measure_identifier(cursor &, std::size_t)` | `core/src/lexer/identifier/identifier_lexer.cpp` | `core/includes/lexer/identifier/identifier_lexer.hpp` |
| SWU-LEXER-009 | used | `consume_identifier(cursor &, const storage_callbacks &, std::size_t)` | `core/src/lexer/identifier/identifier_lexer.cpp` | `core/includes/lexer/identifier/identifier_lexer.hpp` |
| SWU-LEXER-010 | used | `consume_line_terminator(cursor &)` | `core/src/lexer/line_terminator.cpp` | `core/includes/lexer/line_terminator.hpp` |
| SWU-LEXER-011 | used | `detail::(anonymous)::starts_identifier_name(cursor)` | `core/src/lexer/member_name_lexer.cpp` | — |
| SWU-LEXER-012 | used | `detail::consume_member_name(cursor &, syntax_mode, const storage_callbacks &, std::size_t, const string_warning_sink *)` | `core/src/lexer/member_name_lexer.cpp` | `core/includes/lexer/member_name_lexer.hpp` |
| SWU-LEXER-013 | used | `detail::measure_member_name(cursor &, syntax_mode, std::size_t)` | `core/src/lexer/member_name_lexer.cpp` | `core/includes/lexer/member_name_lexer.hpp` |
| SWU-LEXER-014 | used | `detail::consume_decimal_digits(cursor &)` | `core/src/lexer/number/decimal_lexer.cpp` | `core/includes/lexer/number/decimal_lexer.hpp` |
| SWU-LEXER-015 | used | `detail::decimal_expected_error(cursor)` | `core/src/lexer/number/decimal_lexer.cpp` | `core/includes/lexer/number/decimal_lexer.hpp` |
| SWU-LEXER-016 | used | `consume_decimal(cursor &, syntax_mode, std::size_t)` | `core/src/lexer/number/decimal_lexer.cpp` | `core/includes/lexer/number/decimal_lexer.hpp` |
| SWU-LEXER-017 | used | `consume_hexadecimal(cursor &, syntax_mode)` | `core/src/lexer/number/hexadecimal_lexer.cpp` | `core/includes/lexer/number/hexadecimal_lexer.hpp` |
| SWU-LEXER-018 | used | `consume_non_finite(cursor &, syntax_mode)` | `core/src/lexer/number/non_finite_lexer.cpp` | `core/includes/lexer/number/non_finite_lexer.hpp` |
| SWU-LEXER-019 | used | `detail::consume_number_spelling(cursor &, syntax_mode, std::size_t)` | `core/src/lexer/number/number_lexer.cpp` | `core/includes/lexer/number/number_lexer.hpp` |
| SWU-LEXER-020 | used | `consume_number(cursor &, syntax_mode, std::size_t)` | `core/src/lexer/number/number_lexer.cpp` | `core/includes/lexer/number/number_lexer.hpp` |
| SWU-LEXER-021 | used | `consume_punctuation(cursor &)` | `core/src/lexer/punctuation.cpp` | `core/includes/lexer/punctuation.hpp` |
| SWU-LEXER-022 | used | `(anonymous)::may_continue_identifier(adaptive_json5::cursor)` | `core/src/lexer/scalar_lexer.cpp` | — |
| SWU-LEXER-023 | used | `(anonymous)::match_literal_bytes(const adaptive_json5::cursor &)` | `core/src/lexer/scalar_lexer.cpp` | — |
| SWU-LEXER-024 | used | `detail::finish_scalar_token(cursor &, cursor, scalar_token, syntax_mode)` | `core/src/lexer/scalar_lexer.cpp` | `core/includes/lexer/scalar_lexer.hpp` |
| SWU-LEXER-025 | used | `consume_scalar(cursor &, syntax_mode, const storage_callbacks &, std::size_t, std::size_t, std::size_t, const string_warning_sink *)` | `core/src/lexer/scalar_lexer.cpp` | `core/includes/lexer/scalar_lexer.hpp` |
| SWU-LEXER-026 | used | `consume_scalar(cursor &, syntax_mode, const storage_callbacks &, std::size_t)` | `core/src/lexer/scalar_lexer.cpp` | `core/includes/lexer/scalar_lexer.hpp` |
| SWU-LEXER-027 | used | `(anonymous)::finish_scalar_measure(cursor &, cursor, const scalar_measure &, syntax_mode)` | `core/src/lexer/scalar_lexer.cpp` | — |
| SWU-LEXER-028 | used | `measure_scalar(cursor &, syntax_mode, std::size_t, std::size_t, std::size_t)` | `core/src/lexer/scalar_lexer.cpp` | `core/includes/lexer/scalar_lexer.hpp` |
| SWU-LEXER-029 | used | `hex_digit_value(char32_t)` | `core/src/lexer/string/hex_escape.cpp` | `core/includes/lexer/string/hex_escape.hpp` |
| SWU-LEXER-030 | used | `consume_hex_escape(cursor &, syntax_mode)` | `core/src/lexer/string/hex_escape.cpp` | `core/includes/lexer/string/hex_escape.hpp` |
| SWU-LEXER-031 | used | `consume_line_continuation(cursor &, syntax_mode)` | `core/src/lexer/string/line_continuation.cpp` | `core/includes/lexer/string/line_continuation.hpp` |
| SWU-LEXER-032 | used | `consume_non_escape(cursor &, syntax_mode)` | `core/src/lexer/string/non_escape.cpp` | `core/includes/lexer/string/non_escape.hpp` |
| SWU-LEXER-033 | used | `consume_simple_escape(cursor &, syntax_mode)` | `core/src/lexer/string/simple_escape.cpp` | `core/includes/lexer/string/simple_escape.hpp` |
| SWU-LEXER-034 | used | `consume_string_opening(cursor &, syntax_mode)` | `core/src/lexer/string/string_boundary.cpp` | `core/includes/lexer/string/string_boundary.hpp` |
| SWU-LEXER-035 | used | `consume_string_piece(cursor &, string_quote, syntax_mode)` | `core/src/lexer/string/string_boundary.cpp` | `core/includes/lexer/string/string_boundary.hpp` |
| SWU-LEXER-036 | used | `detail::consume_unit_escape(cursor &, syntax_mode)` | `core/src/lexer/string/string_escape.cpp` | `core/includes/lexer/string/string_escape.hpp` |
| SWU-LEXER-037 | used | `consume_string_escape(cursor &, syntax_mode)` | `core/src/lexer/string/string_escape.cpp` | `core/includes/lexer/string/string_escape.hpp` |
| SWU-LEXER-038 | used | `(anonymous)::walk_string_body(cursor &, std::size_t, string_quote, syntax_mode, std::size_t, decoded_string_builder *, const string_warning_sink *)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-039 | used | `detail::notify_string_warnings(cursor, string_quote, syntax_mode, const string_warning_observer &)` | `core/src/lexer/string/string_lexer.cpp` | `core/includes/lexer/string/string_lexer.hpp` |
| SWU-LEXER-040 | used | `measure_string(cursor &, syntax_mode, std::size_t)` | `core/src/lexer/string/string_lexer.cpp` | `core/includes/lexer/string/string_lexer.hpp` |
| SWU-LEXER-041 | used | `consume_string(cursor &, syntax_mode, const storage_callbacks &, std::size_t, string_warning_observer, const string_warning_sink *)` | `core/src/lexer/string/string_lexer.cpp` | `core/includes/lexer/string/string_lexer.hpp` |
| SWU-LEXER-042 | used | `(anonymous)::add(detail::surrogate_warnings &, const string_warning &)` | `core/src/lexer/string/string_warning.cpp` | — |
| SWU-LEXER-043 | used | `detail::surrogate_pairing::escape(const encoded_utf16 &, std::size_t, std::size_t)` | `core/src/lexer/string/string_warning.cpp` | `core/includes/lexer/string/string_warning.hpp` |
| SWU-LEXER-044 | used | `detail::surrogate_pairing::character()` | `core/src/lexer/string/string_warning.cpp` | `core/includes/lexer/string/string_warning.hpp` |
| SWU-LEXER-045 | used | `detail::surrogate_pairing::finish()` | `core/src/lexer/string/string_warning.cpp` | `core/includes/lexer/string/string_warning.hpp` |
| SWU-LEXER-046 | used | `raw_separator_warning_for(const string_piece &, syntax_mode)` | `core/src/lexer/string/string_warning.cpp` | `core/includes/lexer/string/string_warning.hpp` |
| SWU-LEXER-047 | used | `consume_unicode_escape(cursor &)` | `core/src/lexer/string/unicode_escape.cpp` | `core/includes/lexer/string/unicode_escape.hpp` |
| SWU-LEXER-048 | used | `check_unquoted_token_boundary(cursor, syntax_mode)` | `core/src/lexer/token_boundary.cpp` | `core/includes/lexer/token_boundary.hpp` |
| SWU-LEXER-049 | used | `skip_strict_json_whitespace(cursor &)` | `core/src/lexer/trivia.cpp` | `core/includes/lexer/trivia.hpp` |
| SWU-LEXER-050 | used | `skip_trivia(cursor &, syntax_mode)` | `core/src/lexer/trivia.cpp` | `core/includes/lexer/trivia.hpp` |
| SWU-LEXER-051 | used | `skip_json5_whitespace(cursor &)` | `core/src/lexer/whitespace.cpp` | `core/includes/lexer/whitespace.hpp` |

## MEMORY

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-MEMORY-001 | used | `accounted_storage::allocate_callback(void *, std::size_t, std::size_t)` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-002 | used | `accounted_storage::deallocate_callback(void *, void *, std::size_t, std::size_t)` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-003 | used | `accounted_storage::accounted_storage(const storage_callbacks &, std::size_t)` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-004 | used | `accounted_storage::callbacks()` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-005 | used | `accounted_storage::allocate(std::size_t, std::size_t)` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-006 | used | `accounted_storage::allocate_array(std::size_t, std::size_t, std::size_t)` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-007 | used | `accounted_storage::byte_limit()` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-008 | used | `accounted_storage::current_bytes()` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-009 | used | `accounted_storage::peak_bytes()` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-010 | used | `buffer_arena::allocate_callback(void *, std::size_t, std::size_t)` | `core/src/memory/buffer_arena.cpp` | `includes/adaptive_json5/memory/buffer_arena.hpp` |
| SWU-MEMORY-011 | used | `buffer_arena::deallocate_callback(void *, void *, std::size_t, std::size_t)` | `core/src/memory/buffer_arena.cpp` | `includes/adaptive_json5/memory/buffer_arena.hpp` |
| SWU-MEMORY-012 | used | `buffer_arena::buffer_arena(void *, std::size_t)` | `core/src/memory/buffer_arena.cpp` | `includes/adaptive_json5/memory/buffer_arena.hpp` |
| SWU-MEMORY-013 | used | `buffer_arena::callbacks()` | `core/src/memory/buffer_arena.cpp` | `includes/adaptive_json5/memory/buffer_arena.hpp` |
| SWU-MEMORY-014 | used | `buffer_arena::capacity()` | `core/src/memory/buffer_arena.cpp` | `includes/adaptive_json5/memory/buffer_arena.hpp` |
| SWU-MEMORY-015 | used | `buffer_arena::used()` | `core/src/memory/buffer_arena.cpp` | `includes/adaptive_json5/memory/buffer_arena.hpp` |
| SWU-MEMORY-016 | used | `buffer_arena::live_allocations()` | `core/src/memory/buffer_arena.cpp` | `includes/adaptive_json5/memory/buffer_arena.hpp` |
| SWU-MEMORY-017 | used | `detail::(anonymous)::buffer_document_storage::buffer_document_storage(void *, std::size_t, std::size_t)` | `core/src/memory/buffer_document_storage.cpp` | — |
| SWU-MEMORY-018 | used | `detail::(anonymous)::release_buffer_document_storage(void *)` | `core/src/memory/buffer_document_storage.cpp` | — |
| SWU-MEMORY-019 | used | `detail::make_buffer_document_storage(void *, std::size_t, std::size_t)` | `core/src/memory/buffer_document_storage.cpp` | `core/includes/memory/buffer_document_storage.hpp` |
| SWU-MEMORY-020 | used | `decoded_string_builder::append_units(const char16_t *, std::size_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-021 | used | `decoded_string_builder::decoded_string_builder(const storage_callbacks &, std::size_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-022 | used | `decoded_string_builder::reserve_exact(std::size_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-023 | used | `decoded_string_builder::append_unit(char16_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-024 | used | `decoded_string_builder::append_scalar(char32_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-025 | used | `decoded_string_builder::finish()` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-026 | used | `decoded_string_builder::size()` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-027 | used | `detail::document_storage_owner::document_storage_owner(void *, accounted_storage *, void (*)(void *) noexcept)` | `core/src/memory/document_storage_owner.cpp` | `includes/adaptive_json5/memory/document_storage_owner.hpp` |
| SWU-MEMORY-028 | used | `detail::document_storage_owner::~document_storage_owner()` | `core/src/memory/document_storage_owner.cpp` | `includes/adaptive_json5/memory/document_storage_owner.hpp` |
| SWU-MEMORY-029 | used | `detail::document_storage_owner::document_storage_owner(document_storage_owner &&)` | `core/src/memory/document_storage_owner.cpp` | `includes/adaptive_json5/memory/document_storage_owner.hpp` |
| SWU-MEMORY-030 | used | `detail::document_storage_owner::accounting()` | `core/src/memory/document_storage_owner.cpp` | `includes/adaptive_json5/memory/document_storage_owner.hpp` |
| SWU-MEMORY-031 | used | `(anonymous)::overlaps(input_view, const void *, std::size_t)` | `core/src/memory/json5_document.cpp` | — |
| SWU-MEMORY-032 | used | `(anonymous)::kept_text(const char *, std::size_t, std::size_t)` | `core/src/memory/json5_document.cpp` | — |
| SWU-MEMORY-033 | used | `json5_document::json5_document(detail::document_storage_owner &&, parsed_document &&)` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-034 | used | `json5_document::parse(input_view, void *, std::size_t, const parse_options &)` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-035 | used | `json5_document::root()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-036 | used | `json5_document::warning_count()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-037 | used | `json5_document::warning(std::size_t)` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-038 | used | `json5_document::json5_document(json5_document &&)` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-039 | used | `json5_document::~json5_document()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-040 | used | `json5_document::values()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-041 | used | `json5_document::source()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-042 | used | `json5_document::current_bytes()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-043 | used | `json5_document::peak_bytes()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-044 | used | `describe(const error &, input_view, char *, std::size_t)` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-045 | used | `describe(const string_warning &, input_view, char *, std::size_t)` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-046 | used | `detail::json5_document_access::parse_with_storage(input_view, const parse_options &, document_storage_owner &&)` | `core/src/memory/json5_document.cpp` | `core/includes/memory/json5_document_access.hpp` |
| SWU-MEMORY-047 | used | `owned_block::owned_block(const storage_callbacks &, void *, std::size_t, std::size_t)` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-048 | used | `owned_block::owned_block(owned_block &&)` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-049 | used | `owned_block::operator=(owned_block &&)` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-050 | used | `owned_block::data()` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-051 | used | `owned_block::size()` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-052 | used | `owned_block::alignment()` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-053 | used | `allocate_block(const storage_callbacks &, std::size_t, std::size_t)` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-054 | used | `allocate_array(const storage_callbacks &, std::size_t, std::size_t, std::size_t)` | `core/src/memory/owned_block.cpp` | `includes/adaptive_json5/memory/owned_block.hpp` |
| SWU-MEMORY-055 | used | `detail::align_within(std::size_t, std::size_t, void *&, std::size_t &)` | `core/src/memory/pointer_alignment.cpp` | `core/includes/memory/pointer_alignment.hpp` |
| SWU-MEMORY-056 | used | `resource_budget::check_addition(std::size_t, std::size_t, std::size_t)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-057 | used | `resource_budget::resource_budget(const resource_limits &)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-058 | used | `resource_budget::check_input(std::size_t)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-059 | used | `resource_budget::check_number(std::size_t, std::size_t)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-060 | used | `resource_budget::reserve_value(bool)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-061 | used | `resource_budget::release_value(bool)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-062 | used | `resource_budget::enter_container()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-063 | used | `resource_budget::leave_container()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-064 | used | `resource_budget::reserve_string_units(std::size_t, std::size_t)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-065 | used | `resource_budget::release_string_units(std::size_t)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-066 | used | `resource_budget::string_unit_allowance()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-067 | used | `resource_budget::reserve_diagnostic()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-068 | used | `resource_budget::release_diagnostic()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-069 | used | `resource_budget::reserved_nodes()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-070 | used | `resource_budget::peak_depth()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-071 | used | `work_budget::work_budget(std::size_t)` | `core/src/memory/work_budget.cpp` | `core/includes/memory/work_budget.hpp` |
| SWU-MEMORY-072 | used | `work_budget::charge(std::size_t)` | `core/src/memory/work_budget.cpp` | `core/includes/memory/work_budget.hpp` |
| SWU-MEMORY-073 | used | `work_budget::consumed()` | `core/src/memory/work_budget.cpp` | `core/includes/memory/work_budget.hpp` |
| SWU-MEMORY-074 | used | `work_budget::remaining()` | `core/src/memory/work_budget.cpp` | `core/includes/memory/work_budget.hpp` |
| SWU-MEMORY-075 | used | `owned_block::~owned_block()` | `includes/adaptive_json5/memory/owned_block.hpp` | — |

## PARSER

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-PARSER-001 | used | `parse_document(input_view, const parse_options &, accounted_storage &)` | `core/src/parser/parser.cpp` | `includes/adaptive_json5/parser.hpp` |
| SWU-PARSER-002 | used | `detail::attach_parsed_value(document &, parser_frame &, value_index, work_budget &, std::size_t)` | `core/src/parser/parser_attachment.cpp` | `core/includes/parser/parser_attachment.hpp` |
| SWU-PARSER-003 | used | `detail::check_container_close(parser_state, const punctuation_token &, syntax_mode)` | `core/src/parser/parser_container_close.cpp` | `core/includes/parser/parser_container_close.hpp` |
| SWU-PARSER-004 | used | `detail::close_container(document &, parser_stack &, const punctuation_token &, syntax_mode, std::size_t, work_budget &)` | `core/src/parser/parser_container_close.cpp` | `core/includes/parser/parser_container_close.hpp` |
| SWU-PARSER-005 | used | `detail::open_container(document &, parser_stack &, const punctuation_token &, work_budget &)` | `core/src/parser/parser_container_open.cpp` | `core/includes/parser/parser_container_open.hpp` |
| SWU-PARSER-006 | used | `(anonymous)::separator_reason(char, syntax_error_reason)` | `core/src/parser/parser_dispatch.cpp` | — |
| SWU-PARSER-007 | used | `detail::next_container_action(const parser_frame &, std::optional<char>, std::size_t, syntax_mode)` | `core/src/parser/parser_dispatch.cpp` | `core/includes/parser/parser_dispatch.hpp` |
| SWU-PARSER-008 | used | `detail::reject_duplicate_name(const document &, value_index, const decoded_string &, source_span, work_budget &)` | `core/src/parser/parser_duplicate.cpp` | `core/includes/parser/parser_duplicate.hpp` |
| SWU-PARSER-009 | used | `detail::(anonymous)::starts_with_byte_order_mark(input_view)` | `core/src/parser/parser_engine.cpp` | — |
| SWU-PARSER-010 | used | `detail::parser_engine::read_value()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-011 | used | `detail::parser_engine::read_name()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-012 | used | `detail::parser_engine::consume_separator(parser_action)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-013 | used | `detail::parser_engine::finish_container()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-014 | used | `detail::parser_engine::accept_completed_value(value_index)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-015 | used | `detail::parser_engine::reserve_string(void *, std::size_t, std::size_t)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-016 | used | `detail::parser_engine::record_warning(void *, const string_warning &)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-017 | used | `detail::parser_engine::nesting_kinds()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-018 | used | `detail::parser_engine::count_value()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-019 | used | `detail::parser_engine::count_name()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-020 | used | `detail::parser_engine::count_separator(parser_action)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-021 | used | `detail::parser_engine::count_close()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-022 | used | `detail::parser_engine::count_completed_value()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-023 | used | `detail::parser_engine::count_document(bool)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-024 | used | `detail::parser_engine::parser_engine(input_view, const parse_options &, accounted_storage &)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-025 | used | `detail::parser_engine::parse(bool)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-026 | used | `detail::parser_engine::run()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-027 | used | `detail::parser_engine::run_complete()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-028 | used | `detail::parser_stack::slot_address(const owned_block &, std::size_t)` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-029 | used | `detail::parser_stack::live_frame(std::size_t)` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-030 | used | `detail::parser_stack::live_frame(std::size_t)` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-031 | used | `detail::parser_stack::reserve_exact(std::size_t)` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-032 | used | `detail::parser_stack::parser_stack(const storage_callbacks &, std::size_t)` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-033 | used | `detail::parser_stack::~parser_stack()` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-034 | used | `detail::parser_stack::push(parser_frame &&, work_budget &)` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-035 | used | `detail::parser_stack::top()` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-036 | used | `detail::parser_stack::top()` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-037 | used | `detail::parser_stack::pop()` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-038 | used | `detail::parser_stack::clear()` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-039 | used | `detail::parser_stack::size()` | `core/src/parser/parser_stack.cpp` | `core/includes/parser/parser_stack.hpp` |
| SWU-PARSER-040 | used | `detail::begin_limited_scan(input_view, std::size_t, const work_budget &)` | `core/src/parser/parser_work.cpp` | `core/includes/parser/parser_work.hpp` |
| SWU-PARSER-041 | used | `detail::finish_limited_scan(const limited_scan &, std::size_t, work_budget &)` | `core/src/parser/parser_work.cpp` | `core/includes/parser/parser_work.hpp` |
| SWU-PARSER-042 | used | `detail::limited_scan_error(const limited_scan &, const error &)` | `core/src/parser/parser_work.cpp` | `core/includes/parser/parser_work.hpp` |
| SWU-PARSER-043 | used | `detail::skip_trivia_within_budget(cursor &, input_view, syntax_mode, work_budget &)` | `core/src/parser/parser_work.cpp` | `core/includes/parser/parser_work.hpp` |
| SWU-PARSER-044 | used | `detail::append_primitive_token(document &, scalar_token, input_view, const parse_options &, const storage_callbacks &)` | `core/src/parser/primitive_parser.cpp` | `core/includes/parser/primitive_parser.hpp` |
| SWU-PARSER-045 | used | `consume_primitive(document &, cursor &, input_view, const parse_options &, const storage_callbacks &, work_budget &)` | `core/src/parser/primitive_parser.cpp` | `core/includes/parser/primitive_parser.hpp` |
| SWU-PARSER-046 | used | `parse_primitive_document(input_view, const parse_options &, accounted_storage &)` | `core/src/parser/primitive_parser.cpp` | `core/includes/parser/primitive_parser.hpp` |

## UNICODE

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-UNICODE-001 | used | `detail::is_unicode_scalar(char32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-002 | used | `detail::contains_unicode(char32_t, const unicode_range (&)[RangeCount])` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-003 | used | `is_unicode_space_separator(char32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-004 | used | `is_identifier_start(char32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-005 | used | `is_identifier_part(char32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-006 | used | `encode_utf16(char32_t)` | `core/src/unicode/utf16.cpp` | `core/includes/unicode/utf16.hpp` |
| SWU-UNICODE-007 | used | `decode_utf16(const decoded_string &, std::size_t)` | `core/src/unicode/utf16_decode.cpp` | `core/includes/unicode/utf16_decode.hpp` |
| SWU-UNICODE-008 | used | `decode_utf8(cursor &)` | `core/src/unicode/utf8.cpp` | `core/includes/unicode/utf8.hpp` |

## VALUE

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-VALUE-001 | used | `detail::supports_floating_conversion()` | `core/includes/value/floating_format.hpp` | — |
| SWU-VALUE-002 | used | `detail::convert_special_floating(const number_analysis &, const floating_conversion_options &)` | `core/includes/value/floating_special.hpp` | — |
| SWU-VALUE-003 | used | `detail::(anonymous)::read_limb(const owned_block &, std::size_t)` | `core/src/value/big_unsigned.cpp` | — |
| SWU-VALUE-004 | used | `detail::(anonymous)::write_limb(owned_block &, std::size_t, limb_type)` | `core/src/value/big_unsigned.cpp` | — |
| SWU-VALUE-005 | used | `detail::decimal_digit_bits(std::size_t)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-006 | used | `detail::limbs_for_bits(std::size_t)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-007 | used | `detail::big_unsigned::big_unsigned(const storage_callbacks &)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-008 | used | `detail::big_unsigned::reserve(std::size_t)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-009 | used | `detail::big_unsigned::is_zero()` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-010 | used | `detail::big_unsigned::bit_length()` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-011 | used | `detail::big_unsigned::append_digit(unsigned int, unsigned int, work_budget &)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-012 | used | `detail::big_unsigned::compare(const big_unsigned &, work_budget &)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-013 | used | `detail::big_unsigned::shift_left(std::size_t, work_budget &)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-014 | used | `detail::big_unsigned::subtract(const big_unsigned &, work_budget &)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-015 | used | `detail::big_unsigned::assign(const big_unsigned &, work_budget &)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-016 | used | `decoded_name_equal(const decoded_string &, const decoded_string &)` | `core/src/value/decoded_name_equal.cpp` | `includes/adaptive_json5/value/decoded_name_equal.hpp` |
| SWU-VALUE-017 | used | `decoded_string::decoded_string(owned_block &&, std::size_t)` | `core/src/value/decoded_string.cpp` | `includes/adaptive_json5/value/decoded_string.hpp` |
| SWU-VALUE-018 | used | `decoded_string::decoded_string(decoded_string &&)` | `core/src/value/decoded_string.cpp` | `includes/adaptive_json5/value/decoded_string.hpp` |
| SWU-VALUE-019 | used | `decoded_string::copy_units(const storage_callbacks &, const char16_t *, std::size_t)` | `core/src/value/decoded_string.cpp` | `includes/adaptive_json5/value/decoded_string.hpp` |
| SWU-VALUE-020 | used | `decoded_string::size()` | `core/src/value/decoded_string.cpp` | `includes/adaptive_json5/value/decoded_string.hpp` |
| SWU-VALUE-021 | used | `decoded_string::unit_at(std::size_t)` | `core/src/value/decoded_string.cpp` | `includes/adaptive_json5/value/decoded_string.hpp` |
| SWU-VALUE-022 | used | `(anonymous)::add_payload(detail::value_storage &, detail::value_payload)` | `core/src/value/document.cpp` | — |
| SWU-VALUE-023 | used | `document::document(const storage_callbacks &, std::size_t)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-024 | used | `document::document(document &&)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-025 | used | `document::make_null()` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-026 | used | `document::make_boolean(bool)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-027 | used | `document::make_string(decoded_string)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-028 | used | `document::make_number(owned_number)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-029 | used | `document::make_array()` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-030 | used | `document::make_object()` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-031 | used | `document::append_element(value_index, value_index)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-032 | used | `document::append_member(value_index, decoded_string &&, value_index)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-033 | used | `document::append_member(value_index, decoded_string &&, value_index, adaptive_json5::source_span, std::size_t)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-034 | used | `document::set_source_span(value_index, adaptive_json5::source_span, std::size_t)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-035 | used | `document::set_member_name_source_span(value_index, adaptive_json5::source_span, std::size_t)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-036 | used | `document::set_root(value_index)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-037 | used | `document::view(value_index)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-038 | used | `document::iterate(value_index)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-039 | used | `document::lookup_member(value_index, const decoded_string &)` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-040 | used | `document::root()` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-041 | used | `document::size()` | `core/src/value/document.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-042 | used | `detail::document_access::attach_parsed_element(document &, value_index, value_index)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-043 | used | `detail::document_access::attach_parsed_member(document &, value_index, decoded_string &&, value_index, source_span, std::size_t)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-044 | used | `detail::document_access::reserve_records_exact(document &, std::size_t)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-045 | used | `detail::(anonymous)::reject_ancestor_cycle(const value_storage &, value_index, value_index)` | `core/src/value/document_links.cpp` | — |
| SWU-VALUE-046 | used | `detail::(anonymous)::link_child(value_storage &, value_index, value_index, value_index, decoded_string *, std::optional<source_span>, std::size_t)` | `core/src/value/document_links.cpp` | — |
| SWU-VALUE-047 | used | `detail::attach_child(value_storage &, value_index, value_index, value_index, decoded_string *, std::optional<source_span>, std::size_t)` | `core/src/value/document_links.cpp` | `core/includes/value/document_links.hpp` |
| SWU-VALUE-048 | used | `detail::attach_to_detached_parent(value_storage &, value_index, value_index, value_index, decoded_string *, std::optional<source_span>, std::size_t)` | `core/src/value/document_links.cpp` | `core/includes/value/document_links.hpp` |
| SWU-VALUE-049 | used | `(anonymous)::hexadecimal_digit_value(char)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-050 | used | `(anonymous)::bit_length_of(std::uint_least64_t)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-051 | used | `(anonymous)::load_significand(input_view, number_kind, detail::big_unsigned &, work_budget &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-052 | used | `(anonymous)::finish_underflow(bool, const floating_conversion_options &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-053 | used | `(anonymous)::convert_to_floating(const owned_number &, const storage_callbacks &, const floating_conversion_options &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-054 | used | `to_float(const owned_number &, const storage_callbacks &, const floating_conversion_options &)` | `core/src/value/floating_conversion.cpp` | `includes/adaptive_json5/value/floating_conversion.hpp` |
| SWU-VALUE-055 | used | `to_double(const owned_number &, const storage_callbacks &, const floating_conversion_options &)` | `core/src/value/floating_conversion.cpp` | `includes/adaptive_json5/value/floating_conversion.hpp` |
| SWU-VALUE-056 | used | `detail::append_integer_digit(std::uintmax_t, unsigned int, unsigned int, std::uintmax_t)` | `core/src/value/integer_magnitude.cpp` | `core/includes/value/integer_magnitude.hpp` |
| SWU-VALUE-057 | used | `detail::read_integer_magnitude(const owned_number &, std::uintmax_t, work_budget &)` | `core/src/value/integer_magnitude.cpp` | `core/includes/value/integer_magnitude.hpp` |
| SWU-VALUE-058 | deleted | `(anonymous)::wrong_access()` | `core/src/value/json5_value.cpp` | — |
| SWU-VALUE-059 | used | `json5_value::json5_value(result<value_view>, const storage_callbacks &)` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-060 | used | `json5_value::ok()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-061 | used | `json5_value::error_if()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-062 | used | `json5_value::kind()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-063 | used | `json5_value::is_null()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-064 | used | `json5_value::is_boolean()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-065 | used | `json5_value::is_string()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-066 | used | `json5_value::is_number()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-067 | used | `json5_value::is_array()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-068 | used | `json5_value::is_object()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-069 | used | `json5_value::member(std::string_view)` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-070 | used | `json5_value::at(std::size_t)` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-071 | used | `json5_value::size()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-072 | used | `json5_value::as_bool()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-073 | used | `json5_value::as_int64()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-074 | used | `json5_value::as_uint64()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-075 | used | `json5_value::as_double()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-076 | used | `json5_value::as_string(char *, std::size_t)` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-077 | used | `detail::lookup_member(const value_storage &, value_index, const decoded_string &)` | `core/src/value/member_lookup.cpp` | `core/includes/value/member_lookup.hpp` |
| SWU-VALUE-078 | used | `detail::(anonymous)::leading_kind(input_view)` | `core/src/value/number_access.cpp` | — |
| SWU-VALUE-079 | used | `detail::number_access::copy_validated_spelling(input_view, number_kind, const storage_callbacks &, std::size_t)` | `core/src/value/number_access.cpp` | `core/includes/value/number_access.hpp` |
| SWU-VALUE-080 | used | `detail::analyse_number(const owned_number &, work_budget &)` | `core/src/value/number_analysis.cpp` | `core/includes/value/number_analysis.hpp` |
| SWU-VALUE-081 | used | `(anonymous)::hexadecimal_digit_value(char)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-082 | used | `(anonymous)::operand_limbs(const detail::number_analysis &, const detail::number_analysis &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-083 | used | `(anonymous)::chunk_power(std::size_t)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-084 | used | `(anonymous)::scale_by_power_of_ten(detail::big_unsigned &, std::size_t, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-085 | used | `(anonymous)::count_decimal_digits(const detail::big_unsigned &, const storage_callbacks &, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-086 | used | `(anonymous)::read_decimal_form(input_view, number_kind, detail::big_unsigned &, const storage_callbacks &, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-087 | used | `(anonymous)::to_number_order(detail::magnitude_order)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-088 | used | `(anonymous)::compare_finite_magnitudes(const owned_number &, const owned_number &, const detail::number_analysis &, const detail::number_analysis &, const storage_callbacks &, const number_comparison_options &, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-089 | used | `(anonymous)::sign_of(const detail::number_analysis &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-090 | used | `(anonymous)::reversed(number_order)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-091 | used | `compare_numbers(const owned_number &, const owned_number &, const storage_callbacks &, const number_comparison_options &)` | `core/src/value/number_comparison.cpp` | `includes/adaptive_json5/value/number_comparison.hpp` |
| SWU-VALUE-092 | used | `to_unsigned_integer(const owned_number &, const numeric_conversion_options &)` | `core/src/value/numeric_conversion.cpp` | `includes/adaptive_json5/value/numeric_conversion.hpp` |
| SWU-VALUE-093 | used | `to_signed_integer(const owned_number &, const numeric_conversion_options &)` | `core/src/value/numeric_conversion.cpp` | `includes/adaptive_json5/value/numeric_conversion.hpp` |
| SWU-VALUE-094 | used | `is_integral_number(const owned_number &, const numeric_conversion_options &)` | `core/src/value/numeric_conversion.cpp` | `includes/adaptive_json5/value/numeric_conversion.hpp` |
| SWU-VALUE-095 | used | `is_integral_number(const value_view &, const numeric_conversion_options &)` | `core/src/value/numeric_conversion.cpp` | `includes/adaptive_json5/value/numeric_conversion.hpp` |
| SWU-VALUE-096 | used | `owned_number::owned_number(owned_block &&, number_kind)` | `core/src/value/owned_number.cpp` | `includes/adaptive_json5/value/owned_number.hpp` |
| SWU-VALUE-097 | used | `owned_number::copy_spelling(input_view, syntax_mode, const storage_callbacks &, std::size_t)` | `core/src/value/owned_number.cpp` | `includes/adaptive_json5/value/owned_number.hpp` |
| SWU-VALUE-098 | used | `owned_number::spelling()` | `core/src/value/owned_number.cpp` | `includes/adaptive_json5/value/owned_number.hpp` |
| SWU-VALUE-099 | used | `owned_number::kind()` | `core/src/value/owned_number.cpp` | `includes/adaptive_json5/value/owned_number.hpp` |
| SWU-VALUE-100 | used | `retained_source::copy(input_view, const storage_callbacks &)` | `core/src/value/retained_source.cpp` | `includes/adaptive_json5/value/retained_source.hpp` |
| SWU-VALUE-101 | used | `retained_source::is_retained()` | `core/src/value/retained_source.cpp` | `includes/adaptive_json5/value/retained_source.hpp` |
| SWU-VALUE-102 | used | `retained_source::text()` | `core/src/value/retained_source.cpp` | `includes/adaptive_json5/value/retained_source.hpp` |
| SWU-VALUE-103 | used | `utf8_size(const decoded_string &)` | `core/src/value/utf8_export.cpp` | `includes/adaptive_json5/value/utf8_export.hpp` |
| SWU-VALUE-104 | used | `export_utf8(const decoded_string &, unsigned char *, std::size_t)` | `core/src/value/utf8_export.cpp` | `includes/adaptive_json5/value/utf8_export.hpp` |
| SWU-VALUE-105 | used | `detail::utf8_name_equal(const decoded_string &, std::string_view)` | `core/src/value/utf8_name_equal.cpp` | `core/includes/value/utf8_name_equal.hpp` |
| SWU-VALUE-106 | used | `value_iterator::value_iterator(const detail::value_storage &, value_index, const container_links &, bool)` | `core/src/value/value_iterator.cpp` | `includes/adaptive_json5/value/value_iterator.hpp` |
| SWU-VALUE-107 | used | `value_iterator::create(const detail::value_storage &, value_index)` | `core/src/value/value_iterator.cpp` | `includes/adaptive_json5/value/value_iterator.hpp` |
| SWU-VALUE-108 | used | `value_iterator::next()` | `core/src/value/value_iterator.cpp` | `includes/adaptive_json5/value/value_iterator.hpp` |
| SWU-VALUE-109 | used | `detail::value_storage::slot_address(const owned_block &, std::size_t)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-110 | used | `detail::value_storage::live_record(std::size_t)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-111 | used | `detail::value_storage::live_record(std::size_t)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-112 | used | `detail::value_storage::destroy_records()` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-113 | used | `detail::value_storage::reserve(std::size_t)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-114 | used | `detail::value_storage::value_storage(const storage_callbacks &, std::size_t)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-115 | used | `detail::value_storage::value_storage(value_storage &&)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-116 | used | `detail::value_storage::reserve_exact(std::size_t)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-117 | used | `detail::value_storage::~value_storage()` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-118 | used | `detail::value_storage::append(value_record &&)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-119 | used | `detail::value_storage::at(value_index)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-120 | used | `detail::value_storage::at(value_index)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-121 | used | `detail::value_storage::size()` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-122 | used | `value_view::record()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-123 | used | `value_view::payload_as()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-124 | used | `value_view::child_at(std::size_t, bool)` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-125 | used | `value_view::value_view(const detail::value_storage &, value_index)` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-126 | used | `value_view::kind()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-127 | used | `value_view::as_boolean()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-128 | used | `value_view::as_string()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-129 | used | `value_view::as_number()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-130 | used | `value_view::source_span()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-131 | used | `value_view::container_size()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-132 | used | `value_view::element_at(std::size_t)` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-133 | used | `value_view::member_at(std::size_t)` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-134 | used | `value_view::member_name()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-135 | used | `value_view::member_name_source_span()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-136 | used | `(anonymous)::read_entry(const owned_block &, std::size_t)` | `core/src/value/warning_list.cpp` | — |
| SWU-VALUE-137 | used | `(anonymous)::write_entry(owned_block &, std::size_t, const string_warning &)` | `core/src/value/warning_list.cpp` | — |
| SWU-VALUE-138 | used | `(anonymous)::next_capacity(warning_storage_policy, std::size_t, std::size_t, std::size_t)` | `core/src/value/warning_list.cpp` | — |
| SWU-VALUE-139 | used | `warning_list::warning_list(const storage_callbacks &, std::size_t, warning_storage_policy)` | `core/src/value/warning_list.cpp` | `includes/adaptive_json5/value/warning_list.hpp` |
| SWU-VALUE-140 | used | `warning_list::warning_list(warning_list &&)` | `core/src/value/warning_list.cpp` | `includes/adaptive_json5/value/warning_list.hpp` |
| SWU-VALUE-141 | used | `warning_list::reserve(std::size_t)` | `core/src/value/warning_list.cpp` | `includes/adaptive_json5/value/warning_list.hpp` |
| SWU-VALUE-142 | used | `warning_list::append_reserved(const string_warning &)` | `core/src/value/warning_list.cpp` | `includes/adaptive_json5/value/warning_list.hpp` |
| SWU-VALUE-143 | used | `warning_list::size()` | `core/src/value/warning_list.cpp` | `includes/adaptive_json5/value/warning_list.hpp` |
| SWU-VALUE-144 | used | `warning_list::at(std::size_t)` | `core/src/value/warning_list.cpp` | `includes/adaptive_json5/value/warning_list.hpp` |
| SWU-VALUE-145 | used | `to_integer(const owned_number &, const numeric_conversion_options &)` | `includes/adaptive_json5/value/integer_conversion.hpp` | — |
| SWU-VALUE-146 | used | `to_integer(const value_view &, const numeric_conversion_options &)` | `includes/adaptive_json5/value/integer_conversion.hpp` | — |
| SWU-VALUE-147 | used | `value_index::value_index(std::size_t)` | `includes/adaptive_json5/value/value_index.hpp` | — |
| SWU-VALUE-148 | used | `value_index::is_set()` | `includes/adaptive_json5/value/value_index.hpp` | — |
| SWU-VALUE-149 | used | `value_index::position()` | `includes/adaptive_json5/value/value_index.hpp` | — |
| SWU-VALUE-150 | used | `detail::validate_utf8_name(std::string_view)` | `core/src/value/utf8_name_equal.cpp` | `core/includes/value/utf8_name_equal.hpp` |
| SWU-VALUE-151 | used | `detail::multiply_64x64_to_128(std::uint64_t, std::uint64_t)` | `core/includes/value/wide_multiply.hpp` | — |
| SWU-VALUE-152 | used | `(anonymous)::try_eisel_lemire(std::uint64_t, std::int32_t, bool)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-153 | used | `(anonymous)::try_fast_path(input_view, const detail::number_analysis &, work_budget &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-154 | used | `json5_value::as_double(const storage_callbacks &)` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-155 | used | `json5_value::as_float()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-156 | used | `json5_value::as_float(const storage_callbacks &)` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-157 | used | `value_view::find_member(std::string_view)` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
