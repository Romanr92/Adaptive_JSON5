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
| API | SWU-API-043 | SWU-API-044 |
| COMMON | SWU-COMMON-003 | SWU-COMMON-004 |
| DIAGNOSTICS | SWU-DIAGNOSTICS-018 | SWU-DIAGNOSTICS-019 |
| HOSTED | SWU-HOSTED-005 | SWU-HOSTED-006 |
| INPUT | SWU-INPUT-011 | SWU-INPUT-012 |
| LEXER | SWU-LEXER-074 | SWU-LEXER-075 |
| MEMORY | SWU-MEMORY-081 | SWU-MEMORY-082 |
| PARSER | SWU-PARSER-073 | SWU-PARSER-074 |
| SCHEMA | SWU-SCHEMA-311 | SWU-SCHEMA-312 |
| UNICODE | SWU-UNICODE-010 | SWU-UNICODE-011 |
| VALUE | SWU-VALUE-221 | SWU-VALUE-222 |

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
| SWU-API-023 | used | `error::error(error_code, std::size_t, std::uint32_t, source_span, std::uint8_t, source_span)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-024 | used | `error::has(std::uint32_t)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-025 | used | `error::reason_for(error_code)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-026 | used | `error::code()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-027 | used | `error::byte_offset()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-028 | used | `error::has_byte_offset()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-029 | used | `error::source_range()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-030 | used | `error::duplicate_names()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-031 | used | `error::syntax_reason()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-032 | used | `error::numeric_reason()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-033 | used | `error::limit_reason()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-034 | used | `error::access_reason()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-035 | used | `error::limit_at(std::size_t, resource_limit_reason)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-036 | used | `error::limit_without_location(resource_limit_reason)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-037 | used | `error::access_failure(access_error_reason)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-038 | used | `error::internal_fault()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-039 | used | `error::relocated(std::size_t)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-040 | used | `error::unlocated()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-041 | used | `error::security_reason()` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-042 | used | `error::security_at_range(source_span, security_reason)` | `includes/adaptive_json5/error.hpp` | — |
| SWU-API-043 | used | `secure_parse_options()` | `includes/adaptive_json5/options.hpp` | — |

## COMMON

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-COMMON-001 | used | `checked_add(std::size_t, std::size_t, resource_limit_reason)` | `core/includes/common/checked_size.hpp` | — |
| SWU-COMMON-002 | used | `checked_multiply(std::size_t, std::size_t, resource_limit_reason)` | `core/includes/common/checked_size.hpp` | — |
| SWU-COMMON-003 | used | `detail::as_internal_fault(const error &)` | `core/includes/common/internal_link.hpp` | — |

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
| SWU-DIAGNOSTICS-016 | used | `(anonymous)::describe(resource_limit_reason)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-017 | used | `(anonymous)::describe(access_error_reason)` | `core/src/diagnostics/diagnostic_format.cpp` | — |
| SWU-DIAGNOSTICS-018 | used | `(anonymous)::describe(security_reason)` | `core/src/diagnostics/diagnostic_format.cpp` | — |

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
| SWU-INPUT-010 | used | `(anonymous)::is_line_break(std::uint32_t)` | `core/src/input/text_position.cpp` | — |
| SWU-INPUT-011 | used | `locate(input_view, std::size_t)` | `core/src/input/text_position.cpp` | `includes/adaptive_json5/text_position.hpp` |

## LEXER

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-LEXER-001 | used | `detail::is_decimal_digit(std::optional<char>)` | `core/includes/lexer/number/decimal_lexer.hpp` | — |
| SWU-LEXER-002 | used | `is_json5_space(std::uint32_t)` | `core/includes/lexer/whitespace.hpp` | — |
| SWU-LEXER-003 | used | `consume_line_comment(cursor &)` | `core/src/lexer/comment.cpp` | `core/includes/lexer/comment.hpp` |
| SWU-LEXER-004 | used | `consume_block_comment(cursor &)` | `core/src/lexer/comment.cpp` | `core/includes/lexer/comment.hpp` |
| SWU-LEXER-005 | used | `detail::identifier_spelling_matches(const decoded_string &, const std::uint16_t *, std::size_t)` | `core/src/lexer/identifier/identifier_classification.cpp` | `core/includes/lexer/identifier/identifier_classification.hpp` |
| SWU-LEXER-006 | used | `classify_identifier(const identifier_token &, identifier_context, syntax_mode)` | `core/src/lexer/identifier/identifier_classification.cpp` | `core/includes/lexer/identifier/identifier_classification.hpp` |
| SWU-LEXER-007 | used | `(anonymous)::walk_identifier(cursor &, std::size_t, decoded_string_builder *)` | `core/src/lexer/identifier/identifier_lexer.cpp` | — |
| SWU-LEXER-008 | used | `measure_identifier(cursor &, std::size_t)` | `core/src/lexer/identifier/identifier_lexer.cpp` | `core/includes/lexer/identifier/identifier_lexer.hpp` |
| SWU-LEXER-009 | used | `consume_identifier(cursor &, const storage_callbacks &, std::size_t)` | `core/src/lexer/identifier/identifier_lexer.cpp` | `core/includes/lexer/identifier/identifier_lexer.hpp` |
| SWU-LEXER-010 | used | `consume_line_terminator(cursor &)` | `core/src/lexer/line_terminator.cpp` | `core/includes/lexer/line_terminator.hpp` |
| SWU-LEXER-011 | used | `detail::(anonymous)::starts_identifier_name(cursor)` | `core/src/lexer/member_name_lexer.cpp` | — |
| SWU-LEXER-012 | used | `detail::consume_member_name(cursor &, syntax_mode, const storage_callbacks &, std::size_t, const string_warning_sink *, string_checks)` | `core/src/lexer/member_name_lexer.cpp` | `core/includes/lexer/member_name_lexer.hpp` |
| SWU-LEXER-013 | used | `detail::measure_member_name(cursor &, syntax_mode, std::size_t, std::size_t, string_checks)` | `core/src/lexer/member_name_lexer.cpp` | `core/includes/lexer/member_name_lexer.hpp` |
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
| SWU-LEXER-025 | used | `consume_scalar(cursor &, syntax_mode, const storage_callbacks &, std::size_t, std::size_t, std::size_t, const string_warning_sink *, string_checks)` | `core/src/lexer/scalar_lexer.cpp` | `core/includes/lexer/scalar_lexer.hpp` |
| SWU-LEXER-026 | used | `consume_scalar(cursor &, syntax_mode, const storage_callbacks &, std::size_t)` | `core/src/lexer/scalar_lexer.cpp` | `core/includes/lexer/scalar_lexer.hpp` |
| SWU-LEXER-027 | used | `(anonymous)::finish_scalar_measure(cursor &, cursor, const scalar_measure &, syntax_mode)` | `core/src/lexer/scalar_lexer.cpp` | — |
| SWU-LEXER-028 | used | `measure_scalar(cursor &, syntax_mode, std::size_t, std::size_t, std::size_t, std::size_t, string_checks)` | `core/src/lexer/scalar_lexer.cpp` | `core/includes/lexer/scalar_lexer.hpp` |
| SWU-LEXER-029 | used | `hex_digit_value(std::uint32_t)` | `core/src/lexer/string/hex_escape.cpp` | `core/includes/lexer/string/hex_escape.hpp` |
| SWU-LEXER-030 | used | `consume_hex_escape(cursor &, syntax_mode)` | `core/src/lexer/string/hex_escape.cpp` | `core/includes/lexer/string/hex_escape.hpp` |
| SWU-LEXER-031 | used | `consume_line_continuation(cursor &, syntax_mode)` | `core/src/lexer/string/line_continuation.cpp` | `core/includes/lexer/string/line_continuation.hpp` |
| SWU-LEXER-032 | used | `consume_non_escape(cursor &, syntax_mode)` | `core/src/lexer/string/non_escape.cpp` | `core/includes/lexer/string/non_escape.hpp` |
| SWU-LEXER-033 | used | `consume_simple_escape(cursor &, syntax_mode)` | `core/src/lexer/string/simple_escape.cpp` | `core/includes/lexer/string/simple_escape.hpp` |
| SWU-LEXER-034 | used | `consume_string_opening(cursor &, syntax_mode)` | `core/src/lexer/string/string_boundary.cpp` | `core/includes/lexer/string/string_boundary.hpp` |
| SWU-LEXER-035 | used | `consume_string_piece(cursor &, string_quote, syntax_mode)` | `core/src/lexer/string/string_boundary.cpp` | `core/includes/lexer/string/string_boundary.hpp` |
| SWU-LEXER-036 | used | `detail::consume_unit_escape(cursor &, syntax_mode)` | `core/src/lexer/string/string_escape.cpp` | `core/includes/lexer/string/string_escape.hpp` |
| SWU-LEXER-037 | used | `consume_string_escape(cursor &, syntax_mode)` | `core/src/lexer/string/string_escape.cpp` | `core/includes/lexer/string/string_escape.hpp` |
| SWU-LEXER-038 | used | `(anonymous)::walk_string_body(cursor &, std::size_t, string_quote, syntax_mode, std::size_t, std::size_t, decoded_string_builder *, const string_warning_sink *, string_checks)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-039 | used | `detail::notify_string_warnings(cursor, string_quote, syntax_mode, const string_warning_observer &)` | `core/src/lexer/string/string_lexer.cpp` | `core/includes/lexer/string/string_lexer.hpp` |
| SWU-LEXER-040 | used | `measure_string(cursor &, syntax_mode, std::size_t, std::size_t, string_checks)` | `core/src/lexer/string/string_lexer.cpp` | `core/includes/lexer/string/string_lexer.hpp` |
| SWU-LEXER-041 | used | `consume_string(cursor &, syntax_mode, const storage_callbacks &, std::size_t, string_warning_observer, const string_warning_sink *, string_checks)` | `core/src/lexer/string/string_lexer.cpp` | `core/includes/lexer/string/string_lexer.hpp` |
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
| SWU-LEXER-052 | used | `value_string_checks(const parse_options &)` | `core/includes/lexer/string/string_checks.hpp` | — |
| SWU-LEXER-053 | used | `name_string_checks(const parse_options &)` | `core/includes/lexer/string/string_checks.hpp` | — |
| SWU-LEXER-055 | used | `(anonymous)::starts_signed_name(const cursor &)` | `core/src/lexer/number/number_lexer.cpp` | — |
| SWU-LEXER-056 | used | `(anonymous)::measure_unknown_word(const cursor &, std::size_t)` | `core/src/lexer/scalar_lexer.cpp` | — |
| SWU-LEXER-057 | used | `(anonymous)::next_identifier_scalar(cursor &, bool, std::size_t)` | `core/src/lexer/identifier/identifier_lexer.cpp` | — |
| SWU-LEXER-058 | used | `scan_decimal_exponent(cursor &, std::size_t)` | `core/src/lexer/number/decimal_lexer.cpp` | — |
| SWU-LEXER-059 | used | `scan_decimal_significand(cursor &, syntax_mode)` | `core/src/lexer/number/decimal_lexer.cpp` | — |
| SWU-LEXER-060 | used | `has_other_number_prefix(cursor)` | `core/src/lexer/number/decimal_lexer.cpp` | — |
| SWU-LEXER-061 | used | `scan_hex_digits(cursor &)` | `core/src/lexer/number/hexadecimal_lexer.cpp` | — |
| SWU-LEXER-062 | used | `finish_non_finite(cursor &, cursor, syntax_mode, number_kind, std::size_t)` | `core/src/lexer/number/non_finite_lexer.cpp` | — |
| SWU-LEXER-063 | used | `match_non_finite(cursor &, const std::array<std::, 8U> &, std::size_t, bool)` | `core/src/lexer/number/non_finite_lexer.cpp` | — |
| SWU-LEXER-064 | used | `is_recognized_escape(std::uint32_t)` | `core/src/lexer/string/non_escape.cpp` | — |
| SWU-LEXER-065 | used | `match_simple_escape(char, std::uint16_t &, bool &)` | `core/src/lexer/string/simple_escape.cpp` | — |
| SWU-LEXER-066 | used | `string_body_state::record(const string_warning &)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-067 | used | `string_body_state::reject(security_reason, source_span)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-068 | used | `string_body_state::handle_pairing(const detail::surrogate_warnings &)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-069 | used | `string_body_state::check_units(const encoded_utf16 &, source_span)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-070 | used | `string_body_state::emit(const encoded_utf16 &, std::size_t)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-071 | used | `string_body_state::read_escape(cursor &, syntax_mode, std::size_t)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-072 | used | `string_body_state::read_character(const string_piece &, syntax_mode)` | `core/src/lexer/string/string_lexer.cpp` | — |
| SWU-LEXER-073 | used | `is_boundary_space(std::uint32_t, syntax_mode)` | `core/src/lexer/token_boundary.cpp` | — |
| SWU-LEXER-074 | used | `complete_line_terminator(std::uint32_t, cursor &)` | `core/src/lexer/line_terminator.cpp` | `core/includes/lexer/line_terminator.hpp` |

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
| SWU-MEMORY-020 | used | `decoded_string_builder::append_units(const std::uint16_t *, std::size_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-021 | used | `decoded_string_builder::decoded_string_builder(const storage_callbacks &, std::size_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-022 | used | `decoded_string_builder::reserve_exact(std::size_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-023 | used | `decoded_string_builder::append_unit(std::uint16_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
| SWU-MEMORY-024 | used | `decoded_string_builder::append_scalar(std::uint32_t)` | `core/src/memory/decoded_string_builder.cpp` | `core/includes/memory/decoded_string_builder.hpp` |
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
| SWU-MEMORY-056 | used | `resource_budget::check_addition(std::size_t, std::size_t, std::size_t, resource_limit_reason)` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
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
| SWU-MEMORY-076 | used | `accounted_storage::faulted()` | `core/src/memory/accounted_storage.cpp` | `includes/adaptive_json5/memory/accounted_storage.hpp` |
| SWU-MEMORY-077 | used | `resource_budget::diagnostic_allowance()` | `core/src/memory/resource_budget.cpp` | `core/includes/memory/resource_budget.hpp` |
| SWU-MEMORY-078 | used | `detail::store_utf16_units(std::byte *, const std::uint16_t *, std::size_t)` | `core/includes/memory/utf16_storage.hpp` | — |
| SWU-MEMORY-079 | used | `detail::load_utf16_unit(const std::byte *)` | `core/includes/memory/utf16_storage.hpp` | — |
| SWU-MEMORY-080 | used | `json5_document::release()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |
| SWU-MEMORY-081 | used | `json5_document::storage_faulted()` | `core/src/memory/json5_document.cpp` | `includes/adaptive_json5/json5_document.hpp` |

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
| SWU-PARSER-047 | used | `detail::locate_limit(const error &, std::size_t, bool)` | `core/src/parser/parser_work.cpp` | `core/includes/parser/parser_work.hpp` |
| SWU-PARSER-048 | used | `detail::parser_engine::located_failure(const error &)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-049 | used | `detail::attach_child(document &, parser_frame &, value_index, bool, source_span, std::size_t)` | `core/src/parser/parser_attachment.cpp` | — |
| SWU-PARSER-050 | used | `select_name_after_comma(char, syntax_mode, std::size_t)` | `core/src/parser/parser_dispatch.cpp` | — |
| SWU-PARSER-051 | used | `select_separator(char, char, std::size_t)` | `core/src/parser/parser_dispatch.cpp` | — |
| SWU-PARSER-052 | used | `select_value_action(parser_state, char, std::size_t)` | `core/src/parser/parser_dispatch.cpp` | — |
| SWU-PARSER-053 | used | `matches_member_name(const value_view &, const decoded_string &, work_budget &)` | `core/src/parser/parser_duplicate.cpp` | — |
| SWU-PARSER-054 | used | `check_duplicate_parent(const document &, value_index, work_budget &)` | `core/src/parser/parser_duplicate.cpp` | — |
| SWU-PARSER-055 | used | `parser_engine::read_scalar_value()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-056 | used | `parser_engine::append_scanned_scalar(scalar_token &, source_span)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-057 | used | `parser_engine::open_value_container(bool)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-058 | used | `parser_engine::accept_scanned_name(parser_frame &, member_name_token &, std::size_t)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-059 | used | `parser_engine::count_scalar_value()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-060 | used | `parser_engine::reserve_measured_scalar(const scalar_measure &)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-061 | used | `parser_engine::count_open_container(char)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-062 | used | `parser_engine::counting_policy::complete(const parser_engine &)` | `core/src/parser/parser_engine.cpp` | — |
| SWU-PARSER-063 | used | `parser_engine::counting_policy::top(parser_engine &)` | `core/src/parser/parser_engine.cpp` | — |
| SWU-PARSER-064 | used | `parser_engine::building_policy::complete(const parser_engine &)` | `core/src/parser/parser_engine.cpp` | — |
| SWU-PARSER-065 | used | `parser_engine::building_policy::top(parser_engine &)` | `core/src/parser/parser_engine.cpp` | — |
| SWU-PARSER-066 | used | `parser_engine::apply_action(parser_action)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-067 | used | `parser_engine::walk_document()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-068 | used | `parser_engine::finish_input(bool)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-069 | used | `locate_resource_failure(resource_limit_reason, const error &, std::size_t)` | `core/src/parser/parser_work.cpp` | — |
| SWU-PARSER-070 | used | `append_scanned_primitive(document &, cursor &, input_view, const parse_options &, const storage_callbacks &, work_budget &, scalar_token &, primitive_value)` | `core/src/parser/primitive_parser.cpp` | — |
| SWU-PARSER-071 | used | `establish_primitive_root(document &, primitive_value, std::size_t)` | `core/src/parser/primitive_parser.cpp` | — |
| SWU-PARSER-072 | used | `detail::parser_engine::walk_value()` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |
| SWU-PARSER-073 | used | `detail::parser_engine::advance_separator(parser_state&, parser_action)` | `core/src/parser/parser_engine.cpp` | `core/includes/parser/parser_engine.hpp` |

## SCHEMA

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-SCHEMA-001 | used | `matches_dialect_uri(schema_dialect, std::string_view)` | `includes/adaptive_json5/schema/schema_dialect.hpp` | — |
| SWU-SCHEMA-002 | used | `occurs_in(schema_error_code, schema_phase)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-003 | used | `schema_error::reason_for(schema_error_code)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-004 | used | `schema_error::code()` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-005 | used | `schema_error::invalid_schema_reason()` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-006 | used | `schema_error::instance_reason()` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-007 | used | `schema_error::limit_reason()` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-008 | used | `schema_error::source_range()` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-009 | used | `schema_error::parser_error()` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-010 | used | `schema_error::of(schema_error_code, std::optional<source_span>)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-011 | used | `schema_error::invalid(invalid_schema_reason, std::optional<source_span>)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-012 | used | `schema_error::syntax(const error &)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-013 | used | `schema_error::unsupported(unsupported_instance_reason, std::optional<source_span>)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-014 | used | `schema_error::limit(schema_limit_reason)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-015 | used | `schema_error::from_storage(const error &)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-016 | used | `detail::schema_work::schema_work(const validation_budget &, const cancellation_callback &)` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-017 | used | `detail::schema_work::step()` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-018 | used | `detail::schema_work::work_steps()` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-019 | used | `detail::schema_work::budget()` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-020 | used | `detail::check_instance_domain(const document &, schema_work &, const storage_callbacks &)` | `core/src/schema/instance_domain.cpp` | `core/includes/schema/instance_domain.hpp` |
| SWU-SCHEMA-021 | used | `(anonymous)::access_failure(const error *)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-022 | used | `(anonymous)::has_unpaired_surrogate(const decoded_string &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-023 | used | `(anonymous)::value_span(const value_view &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-024 | used | `(anonymous)::name_span(const value_view &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-025 | used | `(anonymous)::check_text(const decoded_string &, std::optional<source_span>)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-026 | used | `(anonymous)::check_value(const value_view &, bool, schema_work &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-027 | used | `(anonymous)::check_unique_names(const value_view &, schema_work &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-028 | used | `(anonymous)::frame_stack::reserve(const storage_callbacks &, std::size_t)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-029 | used | `(anonymous)::frame_stack::push(const stack_frame &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-030 | used | `(anonymous)::frame_stack::top()` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-031 | used | `(anonymous)::frame_stack::pop()` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-032 | used | `(anonymous)::frame_stack::size()` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-033 | used | `(anonymous)::enter(const value_view &, frame_stack &, schema_work &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-034 | used | `(anonymous)::is_container(const value_view &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-035 | used | `schema_error::registration_reason()` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-036 | used | `schema_error::registration(registration_reason)` | `includes/adaptive_json5/schema/schema_error.hpp` | — |
| SWU-SCHEMA-037 | used | `detail::pod_vector<T>::grow()` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-038 | used | `detail::pod_vector<T>::pod_vector(const storage_callbacks&)` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-039 | used | `detail::pod_vector<T>::push_back(const T&)` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-040 | used | `detail::pod_vector<T>::get(std::size_t)` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-041 | used | `detail::pod_vector<T>::set(std::size_t, const T&)` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-042 | used | `detail::pod_vector<T>::pop_back()` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-043 | used | `detail::pod_vector<T>::truncate(std::size_t)` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-044 | used | `detail::pod_vector<T>::size()` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-045 | used | `detail::pod_vector<T>::data()` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-046 | used | `detail::pod_vector<T>::release()` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-047 | used | `detail::split_uri(std::string_view)` | `core/src/schema/uri.cpp` | `core/includes/schema/uri.hpp` |
| SWU-SCHEMA-048 | used | `detail::resolve_uri(std::string_view, std::string_view, pod_vector<char>&, const storage_callbacks&)` | `core/src/schema/uri.cpp` | `core/includes/schema/uri.hpp` |
| SWU-SCHEMA-049 | used | `detail::without_fragment(std::string_view)` | `core/src/schema/uri.cpp` | `core/includes/schema/uri.hpp` |
| SWU-SCHEMA-050 | used | `detail::text_of(const pod_vector<char>&, std::size_t, std::size_t)` | `core/src/schema/uri.cpp` | `core/includes/schema/uri.hpp` |
| SWU-SCHEMA-051 | used | `detail::append_text(pod_vector<char>&, std::string_view)` | `core/src/schema/uri.cpp` | `core/includes/schema/uri.hpp` |
| SWU-SCHEMA-052 | used | `(anonymous)::is_scheme_char(char)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-053 | used | `(anonymous)::lower(char)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-054 | used | `(anonymous)::append_lower(pod_vector<char>&, std::string_view)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-055 | used | `(anonymous)::append_authority(pod_vector<char>&, std::string_view)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-056 | used | `(anonymous)::pop_segment(pod_vector<char>&, std::size_t)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-057 | used | `(anonymous)::append_without_dots(pod_vector<char>&, std::string_view)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-058 | used | `(anonymous)::merge_paths(const uri_parts&, std::string_view, pod_vector<char>&)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-059 | used | `(anonymous)::compose(pod_vector<char>&, const uri_parts&, std::string_view)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-060 | used | `detail::next_pointer_token(std::string_view&, pod_vector<char>&)` | `core/src/schema/json_pointer.cpp` | `core/includes/schema/json_pointer.hpp` |
| SWU-SCHEMA-061 | used | `detail::pointer_index(std::string_view)` | `core/src/schema/json_pointer.cpp` | `core/includes/schema/json_pointer.hpp` |
| SWU-SCHEMA-062 | used | `detail::escape_pointer_token(std::string_view, char*, std::size_t)` | `core/src/schema/json_pointer.cpp` | `core/includes/schema/json_pointer.hpp` |
| SWU-SCHEMA-063 | used | `(anonymous)::hex_value(char)` | `core/src/schema/json_pointer.cpp` | — |
| SWU-SCHEMA-064 | used | `(anonymous)::malformed()` | `core/src/schema/json_pointer.cpp` | — |
| SWU-SCHEMA-065 | used | `keyword_name(schema_keyword)` | `includes/adaptive_json5/schema/schema_keyword.hpp` | — |
| SWU-SCHEMA-066 | used | `schema_registry::create(accounted_storage&, std::size_t)` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-067 | used | `schema_registry::schema_registry(schema_registry&&)` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-068 | used | `schema_registry::~schema_registry()` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-069 | used | `schema_registry::add(std::string_view, input_view)` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-070 | used | `schema_registry::add(input_view)` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-071 | used | `schema_registry::size()` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-072 | used | `schema_registry::capacity()` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-073 | used | `detail::schema_registry_access::entry(const schema_registry&, std::size_t)` | `core/src/schema/schema_registry.cpp` | `core/includes/schema/schema_registry_access.hpp` |
| SWU-SCHEMA-074 | used | `detail::schema_registry_access::uri(const registry_entry&)` | `core/src/schema/schema_registry.cpp` | `core/includes/schema/schema_registry_access.hpp` |
| SWU-SCHEMA-075 | used | `(anonymous)::slot(const owned_block&, std::size_t)` | `core/src/schema/schema_registry.cpp` | — |
| SWU-SCHEMA-076 | used | `(anonymous)::parse_failure(const error&)` | `core/src/schema/schema_registry.cpp` | — |
| SWU-SCHEMA-077 | used | `(anonymous)::check_schema_domain(const document&, accounted_storage&)` | `core/src/schema/schema_registry.cpp` | — |
| SWU-SCHEMA-078 | used | `compiled_schema::node_count()` | `core/src/schema/schema_compiler.cpp` | `includes/adaptive_json5/schema/compiled_schema.hpp` |
| SWU-SCHEMA-079 | used | `compiled_schema::resource_count()` | `core/src/schema/schema_compiler.cpp` | `includes/adaptive_json5/schema/compiled_schema.hpp` |
| SWU-SCHEMA-080 | used | `compiled_schema::unknown_keyword_count()` | `core/src/schema/schema_compiler.cpp` | `includes/adaptive_json5/schema/compiled_schema.hpp` |
| SWU-SCHEMA-081 | used | `detail::compile_schema(const schema_registry&, std::string_view, schema_dialect, const compile_options&)` | `core/src/schema/schema_compiler.cpp` | `includes/adaptive_json5/schema/compiled_schema.hpp` |
| SWU-SCHEMA-082 | used | `detail::compiled_schema_access::make(owned_block&&, std::size_t, owned_block&&, std::size_t, owned_block&&, std::size_t, owned_block&&, std::size_t, std::size_t, std::size_t)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-083 | used | `detail::compiled_schema_access::node(const compiled_schema&, std::size_t)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-084 | used | `detail::compiled_schema_access::edge(const compiled_schema&, std::size_t)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-085 | used | `detail::compiled_schema_access::property(const compiled_schema&, std::size_t)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-086 | used | `detail::compiled_schema_access::unit(const compiled_schema&, std::size_t)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-087 | used | `detail::compiled_schema_access::root(const compiled_schema&)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-088 | used | `detail::pod_vector<T>::writable()` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-089 | used | `(anonymous)::charge(compile_state&, std::size_t)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-090 | used | `(anonymous)::span_of(const value_view&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-091 | used | `(anonymous)::shape_error(const value_view&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-092 | used | `(anonymous)::access_error(const error*)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-093 | used | `(anonymous)::kind_of(const value_view&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-094 | used | `(anonymous)::spells(const decoded_string&, std::string_view)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-095 | used | `(anonymous)::classify(const decoded_string&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-096 | used | `(anonymous)::copy_units(compile_state&, const decoded_string&, node_range&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-097 | used | `(anonymous)::string_utf8(const value_view&, pod_vector<char>&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-098 | used | `(anonymous)::store_text(pod_vector<char>&, std::string_view, node_range&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-099 | used | `(anonymous)::add_key(compile_state&, std::string_view, std::size_t)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-100 | used | `(anonymous)::add_node(compile_state&, const schema_node&, std::size_t&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-101 | used | `(anonymous)::add_child(compile_state&, const pending_item&, schema_keyword, step_kind, node_range, std::size_t, const value_view&, std::size_t&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-102 | used | `(anonymous)::apply_id(compile_state&, const value_view&, std::size_t, node_range&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-103 | used | `(anonymous)::add_array_children(compile_state&, const pending_item&, schema_keyword, const value_view&, bool, node_range&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-104 | used | `(anonymous)::add_map_children(compile_state&, const pending_item&, schema_keyword, const value_view&, bool, bool, node_range&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-105 | used | `(anonymous)::collect_keywords(compile_state&, const value_view&, std::array<std::optional<value_view>, c_keyword_count>&, std::size_t&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-106 | used | `(anonymous)::check_dialect(compile_state&, const value_view&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-107 | used | `(anonymous)::add_single_children(compile_state&, const pending_item&, const std::array<std::optional<value_view>, c_keyword_count>&, bool, schema_node&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-108 | used | `(anonymous)::add_collection_children(compile_state&, const pending_item&, const std::array<std::optional<value_view>, c_keyword_count>&, bool, schema_node&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-109 | used | `(anonymous)::process(compile_state&, const pending_item&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-110 | used | `(anonymous)::find_key(compile_state&, std::string_view)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-111 | used | `(anonymous)::next_code_point(std::string_view, std::size_t&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-112 | used | `(anonymous)::utf8_equals_units(const compile_state&, std::string_view, node_range)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-113 | used | `(anonymous)::find_child(compile_state&, std::size_t, std::string_view, std::optional<std::string_view>, step_kind&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-114 | used | `(anonymous)::walk_pointer(compile_state&, std::size_t, std::string_view)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-115 | used | `(anonymous)::resolve_target(compile_state&, std::string_view)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-116 | used | `(anonymous)::located(schema_error_code, const ref_entry&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-117 | used | `(anonymous)::resolve_refs(compile_state&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-118 | used | `(anonymous)::reject_pure_cycles(compile_state&)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-119 | used | `(anonymous)::add_resource(compile_state&, const schema_registry&, std::size_t)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-120 | used | `(anonymous)::find_root(compile_state&, std::string_view)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-121 | used | `schema_registry::schema_registry(accounted_storage&, owned_block&&, std::size_t)` | `core/src/schema/schema_registry.cpp` | `includes/adaptive_json5/schema/schema_registry.hpp` |
| SWU-SCHEMA-122 | used | `(anonymous)::compile_state::compile_state(const compile_options&, schema_dialect)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-123 | used | `detail::schema_work::charge(std::size_t &, std::size_t, schema_limit_reason)` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-124 | used | `detail::schema_work::equality_step()` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-125 | used | `detail::schema_work::reference_step()` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-126 | used | `detail::schema_work::branch_step()` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-127 | used | `detail::values_equal(const value_view &, const value_view &, schema_work &, const storage_callbacks &)` | `core/src/schema/value_equality.cpp` | `core/includes/schema/value_equality.hpp` |
| SWU-SCHEMA-128 | used | `(anonymous)::failed(const error *)` | `core/src/schema/value_equality.cpp` | — |
| SWU-SCHEMA-129 | used | `(anonymous)::strings_equal(const decoded_string &, const decoded_string &)` | `core/src/schema/value_equality.cpp` | — |
| SWU-SCHEMA-130 | used | `(anonymous)::scalars_equal(const value_pair &, value_kind, const storage_callbacks &)` | `core/src/schema/value_equality.cpp` | — |
| SWU-SCHEMA-131 | used | `(anonymous)::find_by_name(const value_view &, const decoded_string &, schema_work &)` | `core/src/schema/value_equality.cpp` | — |
| SWU-SCHEMA-132 | used | `(anonymous)::push_elements(const value_pair &, pod_vector<value_pair> &)` | `core/src/schema/value_equality.cpp` | — |
| SWU-SCHEMA-133 | used | `(anonymous)::push_members(const value_pair &, pod_vector<value_pair> &, schema_work &)` | `core/src/schema/value_equality.cpp` | — |
| SWU-SCHEMA-134 | used | `(anonymous)::compare_pair(const value_pair &, pod_vector<value_pair> &, schema_work &, const storage_callbacks &)` | `core/src/schema/value_equality.cpp` | — |
| SWU-SCHEMA-135 | used | `validation_report::valid()` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validation_report.hpp` |
| SWU-SCHEMA-136 | used | `validation_report::truncated()` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validation_report.hpp` |
| SWU-SCHEMA-137 | used | `validation_report::diagnostic_count()` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validation_report.hpp` |
| SWU-SCHEMA-138 | used | `validation_report::diagnostic(std::size_t)` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validation_report.hpp` |
| SWU-SCHEMA-139 | used | `validate(const compiled_schema &, const document &, const validation_options &)` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validate.hpp` |
| SWU-SCHEMA-140 | used | `format_schema_location(const compiled_schema &, const schema_diagnostic &, char *, std::size_t)` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validate.hpp` |
| SWU-SCHEMA-141 | used | `detail::validation_report_access::make(bool, bool, owned_block &&, std::size_t)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-142 | used | `(anonymous)::evaluator::evaluator(const compiled_schema &, const validation_options &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-143 | used | `(anonymous)::access_failure(const error *)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-144 | used | `(anonymous)::kind_of(const value_view &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-145 | used | `(anonymous)::record(evaluator &, const frame &, schema_keyword)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-146 | used | `(anonymous)::may_stop(const evaluator &, const frame &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-147 | used | `(anonymous)::fail(evaluator &, frame &, schema_keyword)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-148 | used | `(anonymous)::name_matches(const evaluator &, const decoded_string &, node_range)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-149 | used | `(anonymous)::request(std::size_t, const value_view &, bool, bool, bool)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-150 | used | `(anonymous)::next_child(frame &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-151 | used | `(anonymous)::property_for(evaluator &, const schema_node &, const value_view &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-152 | used | `(anonymous)::absorb(frame &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-153 | used | `(anonymous)::advance_combinators(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-154 | used | `(anonymous)::advance_descending(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-155 | used | `(anonymous)::advance(evaluator &, frame &, std::optional<bool>)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-156 | used | `(anonymous)::push_child(evaluator &, const child_request &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-157 | used | `(anonymous)::run_frames(evaluator &, const value_view &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-158 | used | `(anonymous)::put(std::string_view, char *, std::size_t, std::size_t)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-159 | used | `(anonymous)::put_name(const compiled_schema &, node_range, char *, std::size_t, std::size_t)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-160 | used | `(anonymous)::put_index(std::size_t, char *, std::size_t, std::size_t)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-161 | used | `compiled_schema::compiled_schema(compiled_schema &&)` | `core/src/schema/schema_compiler.cpp` | `includes/adaptive_json5/schema/compiled_schema.hpp` |
| SWU-SCHEMA-162 | used | `validation_report::validation_report(validation_report &&)` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validation_report.hpp` |
| SWU-SCHEMA-163 | used | `detail::pod_vector<T>::pod_vector(pod_vector &&)` | `core/includes/schema/pod_vector.hpp` | — |
| SWU-SCHEMA-164 | used | `format_instance_location(const document &, const schema_diagnostic &, char *, std::size_t, const storage_callbacks &)` | `core/src/schema/evaluator.cpp` | `includes/adaptive_json5/schema/validate.hpp` |
| SWU-SCHEMA-165 | used | `(anonymous)::put_step(const path_frame &, pod_vector<char> &, char *, std::size_t, std::size_t)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-166 | used | `(anonymous)::position_of(const value_view &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-167 | used | `(anonymous)::open_container(const value_view &, pod_vector<path_frame> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-168 | used | `(anonymous)::next_char(std::string_view, std::size_t &)` | `core/src/schema/json_pointer.cpp` | — |
| SWU-SCHEMA-169 | used | `(anonymous)::edge_at(const evaluator &, node_range, std::size_t)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-170 | used | `(anonymous)::fail_keyword(evaluator &, frame &, schema_keyword)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-171 | used | `(anonymous)::step_all_of(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-172 | used | `(anonymous)::step_any_of(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-173 | used | `(anonymous)::step_one_of(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-174 | used | `(anonymous)::step_not(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-175 | used | `(anonymous)::step_if(frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-176 | used | `(anonymous)::step_then_else(frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-177 | used | `(anonymous)::target_for(evaluator &, const frame &, const schema_node &, const value_view &, bool)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-178 | used | `(anonymous)::put_path(const pod_vector<path_frame> &, pod_vector<char> &, char *, std::size_t)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-179 | used | `(anonymous)::step_phase(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-180 | used | `(anonymous)::check_member_name(const value_view &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-181 | used | `(anonymous)::check_number(const value_view &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-182 | used | `(anonymous)::walk_containers(frame_stack &, schema_work &)` | `core/src/schema/instance_domain.cpp` | — |
| SWU-SCHEMA-183 | used | `(anonymous)::compile_boolean(compile_state &, const pending_item &, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-184 | used | `(anonymous)::record_ref(compile_state &, const pending_item &, const value_view &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-185 | used | `(anonymous)::add_items_children(compile_state &, const pending_item &, const value_view &, bool, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-186 | used | `(anonymous)::add_combinator_children(compile_state &, const pending_item &, const std::array<std::optional<value_view>, c_keyword_count> &, bool, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-187 | used | `(anonymous)::relative_target(const uri_parts &, const uri_parts &, pod_vector<char> &, uri_parts &, std::string_view &)` | `core/src/schema/uri.cpp` | — |
| SWU-SCHEMA-188 | used | `(anonymous)::apply_identity(compile_state &, const pending_item &, const std::array<std::optional<value_view>, c_keyword_count> &, pending_item &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-189 | used | `detail::next_capacity(std::size_t, std::size_t)` | `core/src/schema/pod_vector.cpp` | `core/includes/schema/pod_vector.hpp` |
| SWU-SCHEMA-190 | used | `detail::copy_bytes(void *, const void *, std::size_t)` | `core/src/schema/pod_vector.cpp` | `core/includes/schema/pod_vector.hpp` |
| SWU-SCHEMA-191 | used | `detail::compiled_schema_access::attach_values(compiled_schema &, document &&)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-192 | used | `detail::compiled_schema_access::values(const compiled_schema &)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-193 | used | `detail::parse_type_keyword(const value_view &)` | `core/src/schema/schema_values.cpp` | `core/includes/schema/schema_values.hpp` |
| SWU-SCHEMA-194 | used | `detail::copy_value(const value_view &, document &, const storage_callbacks &, std::size_t &, std::size_t)` | `core/src/schema/schema_values.cpp` | `core/includes/schema/schema_values.hpp` |
| SWU-SCHEMA-195 | used | `(anonymous)::failure_of(const error *)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-196 | used | `(anonymous)::shape_at(const value_view &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-197 | used | `(anonymous)::type_from_name(const decoded_string &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-198 | used | `(anonymous)::type_of_name_value(const value_view &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-199 | used | `(anonymous)::copy_text(const decoded_string &, const storage_callbacks &, pod_vector<std::uint16_t> &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-200 | used | `(anonymous)::copy_number(const owned_number &, document &, const storage_callbacks &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-201 | used | `(anonymous)::make_copy(const value_view &, document &, const storage_callbacks &, pod_vector<std::uint16_t> &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-202 | used | `(anonymous)::charge_copy(std::size_t &, std::size_t)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-203 | used | `(anonymous)::open_copy(const value_view &, value_index, pod_vector<copy_frame> &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-204 | used | `(anonymous)::attach_copy(const copy_frame &, const value_view &, value_index, document &, const storage_callbacks &, pod_vector<std::uint16_t> &)` | `core/src/schema/schema_values.cpp` | — |
| SWU-SCHEMA-205 | used | `(anonymous)::compile_value_keywords(compile_state &, const std::array<std::optional<value_view>, c_keyword_count> &, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-206 | used | `(anonymous)::keyword_bit(schema_keyword)` | `core/includes/schema/assertions.hpp` | — |
| SWU-SCHEMA-207 | used | `detail::check_assertions(const compiled_schema &, const schema_node &, const value_view &, schema_work &, const storage_callbacks &)` | `core/src/schema/assertions.cpp` | `core/includes/schema/assertions.hpp` |
| SWU-SCHEMA-208 | used | `(anonymous)::failure_of(const error *)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-209 | used | `(anonymous)::is_integer(const value_view &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-210 | used | `(anonymous)::type_matches(const value_view &, std::uint8_t)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-211 | used | `(anonymous)::enum_matches(const value_view &, const value_view &, schema_work &, const storage_callbacks &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-212 | used | `(anonymous)::schema_value(const compiled_schema &, value_index)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-213 | used | `(anonymous)::collect(const bool_result &, schema_keyword, keyword_mask &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-214 | used | `(anonymous)::check_values(const compiled_schema &, const schema_node &, const value_view &, schema_work &, const storage_callbacks &, keyword_mask &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-215 | used | `(anonymous)::run_assertions(evaluator &, frame &, const schema_node &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-216 | used | `detail::is_positive(const owned_number &)` | `core/src/schema/exact_number.cpp` | `core/includes/schema/exact_number.hpp` |
| SWU-SCHEMA-217 | used | `detail::is_multiple_of(const owned_number &, const owned_number &, const storage_callbacks &)` | `core/src/schema/exact_number.cpp` | `core/includes/schema/exact_number.hpp` |
| SWU-SCHEMA-218 | used | `(anonymous)::digit_value(char, std::uint32_t)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-219 | used | `(anonymous)::read_exponent(std::string_view)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-220 | used | `(anonymous)::split_number(const owned_number &)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-221 | used | `(anonymous)::failure_of(const status_result &)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-222 | used | `(anonymous)::append_digits(big_unsigned &, std::string_view, std::uint32_t, work_budget &)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-223 | used | `(anonymous)::build_scaled(big_unsigned &, const decimal_parts &, std::int64_t, work_budget &)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-224 | used | `(anonymous)::reduce_modulo(big_unsigned &, const big_unsigned &, big_unsigned &, work_budget &)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-225 | used | `(anonymous)::compile_numeric_keyword(compile_state &, const value_view &, bool, value_index &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-226 | used | `(anonymous)::compile_numeric_keywords(compile_state &, const std::array<std::optional<value_view>, c_keyword_count> &, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-227 | used | `(anonymous)::order_against(const compiled_schema &, const owned_number &, value_index, const storage_callbacks &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-228 | used | `(anonymous)::check_numeric(const compiled_schema &, const schema_node &, const value_view &, const storage_callbacks &, keyword_mask &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-229 | used | `(anonymous)::scaled_digits(const decimal_parts &, std::int64_t)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-230 | used | `(anonymous)::reserve_digits(big_unsigned &, std::size_t)` | `core/src/schema/exact_number.cpp` | — |
| SWU-SCHEMA-231 | used | `detail::schema_work::regex_steps(std::size_t)` | `core/src/schema/schema_work.cpp` | `core/includes/schema/schema_work.hpp` |
| SWU-SCHEMA-232 | used | `detail::compile_regex(const decoded_string &, pod_vector<regex_inst> &, pod_vector<regex_range> &, const storage_callbacks &, std::size_t &, std::size_t, std::optional<source_span>)` | `core/src/schema/regex.cpp` | `core/includes/schema/regex.hpp` |
| SWU-SCHEMA-233 | used | `detail::regex_search(const regex_tables &, node_range, const decoded_string &, schema_work &, const storage_callbacks &)` | `core/src/schema/regex.cpp` | `core/includes/schema/regex.hpp` |
| SWU-SCHEMA-234 | used | `(anonymous)::regex_builder(pod_vector<regex_inst> &, pod_vector<regex_range> &, const storage_callbacks &, std::size_t &, std::size_t, std::optional<source_span>)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-235 | used | `(anonymous)::pattern_error(const regex_builder &, bool)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-236 | used | `(anonymous)::charge(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-237 | used | `(anonymous)::add_node(regex_builder &, const ast_node &, std::size_t &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-238 | used | `(anonymous)::push_term(regex_builder &, const ast_node &, bool)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-239 | used | `(anonymous)::decode_text(const decoded_string &, pod_vector<std::uint32_t> &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-240 | used | `(anonymous)::append_set(regex_builder &, const std::array<regex_range, N> &, bool)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-241 | used | `(anonymous)::append_shorthand(regex_builder &, std::uint32_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-242 | used | `(anonymous)::hex_digit(std::uint32_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-243 | used | `(anonymous)::read_hex(const regex_builder &, std::size_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-244 | used | `(anonymous)::control_escape(std::uint32_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-245 | used | `(anonymous)::read_escape(regex_builder &, bool)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-246 | used | `(anonymous)::read_class_atom(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-247 | used | `(anonymous)::parse_class(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-248 | used | `(anonymous)::read_count(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-249 | used | `(anonymous)::read_braces(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-250 | used | `(anonymous)::apply_quantifier(regex_builder &, std::size_t, std::size_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-251 | used | `(anonymous)::close_alternative(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-252 | used | `(anonymous)::close_group(regex_builder &, std::size_t &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-253 | used | `(anonymous)::open_group(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-254 | used | `(anonymous)::end_group(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-255 | used | `(anonymous)::parse_escape_term(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-256 | used | `(anonymous)::parse_quantifier(regex_builder &, std::uint32_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-257 | used | `(anonymous)::parse_item(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-258 | used | `(anonymous)::parse_pattern(regex_builder &, std::size_t &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-259 | used | `(anonymous)::emitter(const storage_callbacks & scratch) noexcept : tasks(scratch), patches(scratch)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-260 | used | `(anonymous)::emit(regex_builder &, const regex_inst &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-261 | used | `(anonymous)::push_tasks(emitter &, const emit_task &, const emit_task &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-262 | used | `(anonymous)::patch_exit(regex_builder &, std::size_t, bool)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-263 | used | `(anonymous)::run_repeat(regex_builder &, emitter &, const emit_task &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-264 | used | `(anonymous)::run_alternative(regex_builder &, emitter &, const emit_task &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-265 | used | `(anonymous)::run_emit(regex_builder &, emitter &, const emit_task &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-266 | used | `(anonymous)::run_alternative_end(regex_builder &, emitter &, const emit_task &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-267 | used | `(anonymous)::run_task(regex_builder &, emitter &, const emit_task &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-268 | used | `(anonymous)::emit_program(regex_builder &, std::size_t, const storage_callbacks &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-269 | used | `(anonymous)::matcher(const regex_tables &, node_range, const storage_callbacks &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-270 | used | `(anonymous)::is_word(std::uint32_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-271 | used | `(anonymous)::assertion_holds(const matcher &, regex_op, std::size_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-272 | used | `(anonymous)::add_thread(matcher &, pod_vector<std::size_t> &, std::size_t, std::size_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-273 | used | `(anonymous)::consumes(const regex_tables &, const regex_inst &, std::uint32_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-274 | used | `(anonymous)::step_threads(matcher &, std::size_t, schema_work &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-275 | used | `(anonymous)::prepare(matcher &, const decoded_string &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-276 | used | `detail::compiled_schema_access::attach_regex(compiled_schema &, owned_block &&, owned_block &&)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-277 | used | `detail::compiled_schema_access::regex(const compiled_schema &)` | `core/src/schema/schema_compiler.cpp` | `core/includes/schema/schema_graph.hpp` |
| SWU-SCHEMA-278 | used | `(anonymous)::read_count_keyword(const value_view &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-279 | used | `(anonymous)::compile_string_keywords(compile_state &, const std::array<std::optional<value_view>, c_keyword_count> &, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-280 | used | `(anonymous)::code_points(const decoded_string &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-281 | used | `(anonymous)::check_string(const compiled_schema &, const schema_node &, const value_view &, schema_work &, const storage_callbacks &, keyword_mask &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-282 | used | `(anonymous)::is_unsupported_escape(const regex_builder &, std::uint32_t, bool)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-283 | used | `(anonymous)::escaped_character(regex_builder &, std::uint32_t)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-284 | used | `(anonymous)::add_class_element(regex_builder &)` | `core/src/schema/regex.cpp` | — |
| SWU-SCHEMA-285 | used | `(anonymous)::compile_assertion_keywords(compile_state &, const std::array<std::optional<value_view>, c_keyword_count> &, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-286 | used | `(anonymous)::set_applicator(schema_node &, schema_keyword, std::size_t)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-287 | used | `(anonymous)::compile_array_keywords(const std::array<std::optional<value_view>, c_keyword_count> &, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-288 | used | `(anonymous)::all_unique(const value_view &, schema_work &, const storage_callbacks &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-289 | used | `(anonymous)::check_array(const schema_node &, const value_view &, schema_work &, const storage_callbacks &, keyword_mask &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-290 | used | `(anonymous)::advance_contains(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-291 | used | `(anonymous)::check_by_kind(const compiled_schema &, const schema_node &, const value_view &, schema_work &, const storage_callbacks &, keyword_mask &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-292 | used | `detail::copy_dependency_arrays(const value_view &, document &, const storage_callbacks &, std::size_t &, std::size_t)` | `core/src/schema/schema_values.cpp` | `core/includes/schema/schema_values.hpp` |
| SWU-SCHEMA-293 | used | `detail::check_name_list(const value_view &)` | `core/src/schema/schema_values.cpp` | `core/includes/schema/schema_values.hpp` |
| SWU-SCHEMA-294 | used | `(anonymous)::compile_object_keywords(compile_state &, const std::array<std::optional<value_view>, c_keyword_count> &, schema_node &)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-295 | used | `(anonymous)::compile_name_pattern(compile_state &, const decoded_string &, const value_view &, std::size_t)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-296 | used | `(anonymous)::has_member(const value_view &, const decoded_string &, schema_work &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-297 | used | `(anonymous)::has_all(const value_view &, const value_view &, schema_work &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-298 | used | `(anonymous)::dependencies_hold(const value_view &, const value_view &, schema_work &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-299 | used | `(anonymous)::check_object(const compiled_schema &, const schema_node &, const value_view &, schema_work &, keyword_mask &)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-300 | used | `(anonymous)::instance_has_name(evaluator &, const value_view &, node_range)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-301 | used | `(anonymous)::next_member_target(evaluator &, frame &, const schema_node &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-302 | used | `(anonymous)::advance_members(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-303 | used | `(anonymous)::advance_dependencies(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-304 | used | `(anonymous)::name_value(evaluator &, const value_view &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-305 | used | `(anonymous)::advance_property_names(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-306 | used | `(anonymous)::advance_late_phase(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-307 | used | `(anonymous)::advance_stage(evaluator &, frame &, const schema_node &, std::optional<bool> &)` | `core/src/schema/evaluator.cpp` | — |
| SWU-SCHEMA-308 | used | `(anonymous)::add_map_member(compile_state &, const pending_item &, schema_keyword, const value_view &, bool)` | `core/src/schema/schema_compiler.cpp` | — |
| SWU-SCHEMA-309 | used | `(anonymous)::kind_bit(value_kind)` | `core/src/schema/assertions.cpp` | — |
| SWU-SCHEMA-310 | used | `detail::has_flag<Bit>(std::uint8_t, Bit)` | `core/includes/schema/schema_graph.hpp` | — |
| SWU-SCHEMA-311 | used | `detail::with_flag<Bit>(std::uint8_t, Bit)` | `core/includes/schema/schema_graph.hpp` | — |

## UNICODE

| ID | Status | Function | Defined in | Also documented in |
| --- | --- | --- | --- | --- |
| SWU-UNICODE-001 | used | `detail::is_unicode_scalar(std::uint32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-002 | used | `detail::contains_unicode(std::uint32_t, const unicode_range (&)[RangeCount])` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-003 | used | `is_unicode_space_separator(std::uint32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-004 | used | `is_identifier_start(std::uint32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-005 | used | `is_identifier_part(std::uint32_t)` | `core/includes/unicode/unicode.hpp` | — |
| SWU-UNICODE-006 | used | `encode_utf16(std::uint32_t)` | `core/src/unicode/utf16.cpp` | `core/includes/unicode/utf16.hpp` |
| SWU-UNICODE-007 | used | `decode_utf16(const decoded_string &, std::size_t)` | `core/src/unicode/utf16_decode.cpp` | `core/includes/unicode/utf16_decode.hpp` |
| SWU-UNICODE-008 | used | `decode_utf8(cursor &)` | `core/src/unicode/utf8.cpp` | `core/includes/unicode/utf8.hpp` |
| SWU-UNICODE-009 | used | `invalid_second_byte(std::byte, std::byte)` | `core/src/unicode/utf8.cpp` | — |
| SWU-UNICODE-010 | used | `decode_continuations(cursor &, std::size_t, std::byte, std::size_t, std::uint32_t)` | `core/src/unicode/utf8.cpp` | — |

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
| SWU-VALUE-019 | used | `decoded_string::copy_units(const storage_callbacks &, const std::uint16_t *, std::size_t)` | `core/src/value/decoded_string.cpp` | `includes/adaptive_json5/value/decoded_string.hpp` |
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
| SWU-VALUE-104 | used | `export_utf8(const decoded_string &, char *, std::size_t)` | `core/src/value/utf8_export.cpp` | `includes/adaptive_json5/value/utf8_export.hpp` |
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
| SWU-VALUE-158 | used | `detail::document_access::create(const storage_callbacks &, std::size_t)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-159 | used | `detail::document_access::make_null(document &)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-160 | used | `detail::document_access::make_boolean(document &, bool)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-161 | used | `detail::document_access::make_string(document &, decoded_string)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-162 | used | `detail::document_access::make_number(document &, owned_number)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-163 | used | `detail::document_access::make_array(document &)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-164 | used | `detail::document_access::make_object(document &)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-165 | used | `detail::document_access::append_element(document &, value_index, value_index)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-166 | used | `detail::document_access::append_member(document &, value_index, decoded_string &&, value_index)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-167 | used | `detail::document_access::append_member(document &, value_index, decoded_string &&, value_index, source_span, std::size_t)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-168 | used | `detail::document_access::set_source_span(document &, value_index, source_span, std::size_t)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-169 | used | `detail::document_access::set_member_name_source_span(document &, value_index, source_span, std::size_t)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-170 | used | `detail::document_access::set_root(document &, value_index)` | `core/src/value/document_access.cpp` | `core/includes/value/document_access.hpp` |
| SWU-VALUE-172 | used | `detail::record_link::record_link()` | `core/includes/value/record_link.hpp` | — |
| SWU-VALUE-173 | used | `detail::record_link::record_link(std::size_t)` | `core/includes/value/record_link.hpp` | — |
| SWU-VALUE-174 | used | `detail::record_link::is_set()` | `core/includes/value/record_link.hpp` | — |
| SWU-VALUE-175 | used | `detail::record_link::position()` | `core/includes/value/record_link.hpp` | — |
| SWU-VALUE-176 | used | `detail::value_storage::issue(std::size_t)` | `core/src/value/value_storage.cpp` | `includes/adaptive_json5/value/document.hpp` |
| SWU-VALUE-177 | used | `(anonymous)::exact_power_of_ten<T>(std::size_t)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-178 | used | `(anonymous)::rounds_to_nearest()` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-179 | used | `value_view::children()` | `core/src/value/value_view.cpp` | `includes/adaptive_json5/value/value_view.hpp` |
| SWU-VALUE-180 | used | `json5_iterator::json5_iterator(result<value_iterator>, const storage_callbacks &)` | `core/src/value/json5_iterator.cpp` | `includes/adaptive_json5/json5_iterator.hpp` |
| SWU-VALUE-181 | used | `json5_iterator::next()` | `core/src/value/json5_iterator.cpp` | `includes/adaptive_json5/json5_iterator.hpp` |
| SWU-VALUE-182 | used | `json5_value::children()` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-183 | used | `json5_value::name(char *, std::size_t)` | `core/src/value/json5_value.cpp` | `includes/adaptive_json5/json5_value.hpp` |
| SWU-VALUE-184 | used | `(anonymous)::utf8_char(std::uint32_t)` | `core/src/value/utf8_export.cpp` | — |
| SWU-VALUE-186 | used | `reject_by_scale(const detail::number_analysis &, const floating_conversion_options &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-187 | used | `reserve_operands(const detail::number_analysis &, std::size_t, detail::big_unsigned &, detail::big_unsigned &, work_budget &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-188 | used | `load_ratio(input_view, const detail::number_analysis &, detail::big_unsigned &, detail::big_unsigned &, work_budget &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-189 | used | `align_ratio(std::ptrdiff_t, detail::big_unsigned &, detail::big_unsigned &, work_budget &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-190 | used | `divide_ratio(std::ptrdiff_t, detail::big_unsigned &, const detail::big_unsigned &, work_budget &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-191 | used | `round_quotient(std::uint_least64_t, bool, std::ptrdiff_t, bool, const floating_conversion_options &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-192 | used | `charge_shift(std::size_t, std::size_t, work_budget &)` | `core/src/value/big_unsigned.cpp` | — |
| SWU-VALUE-193 | used | `shifted_capacity(std::size_t, std::uint32_t)` | `core/src/value/big_unsigned.cpp` | — |
| SWU-VALUE-194 | used | `big_unsigned::shift_reserved_limbs(std::size_t, std::uint32_t, std::size_t)` | `core/src/value/big_unsigned.cpp` | `core/includes/value/big_unsigned.hpp` |
| SWU-VALUE-195 | used | `charge_subtraction(std::size_t, work_budget &)` | `core/src/value/big_unsigned.cpp` | — |
| SWU-VALUE-196 | used | `child_links(value_record &, const decoded_string *)` | `core/src/value/document_links.cpp` | — |
| SWU-VALUE-197 | used | `find_last_child(value_storage &, value_index, const container_links &)` | `core/src/value/document_links.cpp` | — |
| SWU-VALUE-198 | used | `publish_child_links(value_record &, value_record *, container_links &, value_index, value_index, decoded_string *, std::optional<source_span>, std::size_t)` | `core/src/value/document_links.cpp` | — |
| SWU-VALUE-199 | used | `validate_member_span(const decoded_string *, std::optional<source_span>, std::size_t)` | `core/src/value/document_links.cpp` | — |
| SWU-VALUE-200 | used | `round_eisel_product(const detail::product_128 &, std::int32_t, std::int32_t, bool)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-201 | used | `read_fast_significand(input_view, bool)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-202 | used | `try_clinger(std::uint64_t, const detail::number_analysis &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-203 | used | `operand_capacity(const detail::number_analysis &, std::size_t)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-204 | used | `convert_exact_ratio(input_view, const detail::number_analysis &, const storage_callbacks &, const floating_conversion_options &, work_budget &)` | `core/src/value/floating_conversion.cpp` | — |
| SWU-VALUE-205 | used | `accumulate_decimal_magnitude(input_view, std::size_t, std::size_t, std::size_t, std::uintmax_t, integer_magnitude, std::uintmax_t, work_budget &)` | `core/src/value/integer_magnitude.cpp` | — |
| SWU-VALUE-206 | used | `scan_integer_exponent(input_view, std::size_t, bool &, std::uintmax_t &)` | `core/src/value/integer_magnitude.cpp` | — |
| SWU-VALUE-207 | used | `scan_integer_significand(input_view, std::size_t &, std::size_t &, std::size_t &, std::size_t &)` | `core/src/value/integer_magnitude.cpp` | — |
| SWU-VALUE-208 | used | `read_hex_magnitude(input_view, std::size_t, integer_magnitude, std::uintmax_t)` | `core/src/value/integer_magnitude.cpp` | — |
| SWU-VALUE-209 | used | `read_decimal_magnitude(input_view, std::size_t, integer_magnitude, std::uintmax_t, work_budget &)` | `core/src/value/integer_magnitude.cpp` | — |
| SWU-VALUE-210 | used | `finish_number_analysis(number_analysis, const decimal_exponent &, std::size_t, bool, std::size_t)` | `core/src/value/number_analysis.cpp` | — |
| SWU-VALUE-211 | used | `scan_scale_exponent(input_view, std::size_t, std::size_t, std::size_t, decimal_exponent &)` | `core/src/value/number_analysis.cpp` | — |
| SWU-VALUE-212 | used | `scan_decimal_digits(input_view, std::size_t &, number_analysis &, std::size_t &, std::size_t &)` | `core/src/value/number_analysis.cpp` | — |
| SWU-VALUE-213 | used | `analyse_hexadecimal(input_view, std::size_t, number_analysis)` | `core/src/value/number_analysis.cpp` | — |
| SWU-VALUE-214 | used | `count_remaining_digits(const detail::big_unsigned &, detail::big_unsigned &, std::size_t, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-215 | used | `read_decimal_scale(input_view, std::size_t, std::size_t, std::size_t)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-216 | used | `read_hex_form(input_view, std::size_t, detail::big_unsigned &, const storage_callbacks &, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-217 | used | `read_decimal_significand(input_view, std::size_t, detail::big_unsigned &, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-218 | used | `compare_decimal_forms(const decimal_form &, const decimal_form &, detail::big_unsigned &, detail::big_unsigned &, work_budget &)` | `core/src/value/number_comparison.cpp` | — |
| SWU-VALUE-219 | used | `write_utf8_scalar(char * &, std::uint32_t)` | `core/src/value/utf8_export.cpp` | — |
| SWU-VALUE-220 | used | `validate_member_links(const container_links &, std::size_t)` | `core/src/value/value_view.cpp` | — |
| SWU-VALUE-221 | used | `validate_member_record(const detail::value_record &, value_index, value_index, const container_links &, std::size_t)` | `core/src/value/value_view.cpp` | — |
