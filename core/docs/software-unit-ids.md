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
| SCHEMA | SWU-SCHEMA-034 | SWU-SCHEMA-035 |
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
