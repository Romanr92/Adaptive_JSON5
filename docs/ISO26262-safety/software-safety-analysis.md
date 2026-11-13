# Software safety analysis

| Item | Value |
| --- | --- |
| Work product | Software safety analysis at the architectural level (ISO 26262-6, clause 7; method from ISO 26262-9, clause 8) and dependent failure analysis (ISO 26262-9, clause 7) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138) |
| Status | Draft. Method, scope, FMEA rows and dependent failure analysis were reviewed with the owner part by part; no independent review yet. |
| Scope | Parser library `AdaptiveJSON5::parser` only. The hosted add-on is QM and is not analysed. The planned serializer is not covered yet. |

This document has two parts. The **software FMEA** looks at each part of the
library and asks how it could fail. The **dependent failure analysis (DFA)** looks
for a single cause that could break two things at the same time.

The architecture these analyses are based on is described in the existing
[parser architecture](../../core/docs/design-architecture.md) and the ADRs. A
safety view of the architecture (interfaces and the errors each part can return)
is still to be done; see the [safety plan](safety-plan.md).

## 1. Software FMEA

### 1.1 Method

I don't use occurrence or detection scores in this FMEA. Software faults are
systematic, not random, so any probability I gave them would be a guess. Instead,
for every failure mode I show which safety measure prevents it and which test
proves that.

A row without a safety measure or without a test is a **gap**. Every gap needs an
action: a new or changed safety requirement, a code change, or a new unit test.

### 1.2 Where the effect stops

The effect of each failure mode is described **at the library boundary**: what the
integrator's code actually sees. For example, `as_int64()` returns 300 when the
input said 3000, and still reports success.

The analysis stops there. What that effect means in the vehicle depends on how the
integrator uses the value, so it belongs in the integrator's own safety analysis,
which takes the effects described here as its starting point.

### 1.3 Parts analysed

| Part | What it does | Code |
| --- | --- | --- |
| Input | Takes the caller's input bytes and tracks positions in them. | `includes/adaptive_json5/input.hpp`, `core/src/input/` |
| Lexer and Unicode handling | Turns bytes into tokens: whitespace, comments, strings, names, numbers and literals. Decodes UTF-8 and UTF-16 escapes. | `core/src/lexer/`, `core/src/unicode/` |
| Parser | Checks the grammar, builds the document and enforces nesting and work limits. | `core/src/parser/` |
| Value model and conversions | Stores values and gives checked access, member lookup and number conversions. | `core/src/value/` |
| Memory | Hands out storage from the caller's buffer or memory functions and enforces memory limits. | `core/src/memory/` |
| Diagnostics | Builds errors, warnings and line and column positions. | `core/src/diagnostics/`, `includes/adaptive_json5/error.hpp` |
| Public API | The functions and types the integrator calls, including misuse such as using a moved-from document. | `includes/adaptive_json5/` |

### 1.4 Failure modes

Each part is checked against these five failure modes:

| ID | Failure mode | Effect at the library boundary |
| --- | --- | --- |
| FM-1 | A wrong value is accepted | The integrator gets a value that does not match the input, and the result reports success. |
| FM-2 | Valid input is rejected | The integrator gets an error for input that is valid, so correct data is lost. |
| FM-3 | The library hangs or runs too long | A call does not return within the time the integrator planned for. |
| FM-4 | Memory is corrupted | The library reads or writes memory it does not own, or its own data becomes inconsistent. |
| FM-5 | The wrong error is reported | A failure is reported, but with the wrong error category or location, so the integrator reacts in the wrong way. |

### 1.5 FMEA table

Each row gives the cause, the effect at the library boundary, the software safety
requirement (SSR), the safety measure, the unit tests that show it, and any gap with
the action taken. Every gap is tracked in a GitHub issue. Review finding IDs such as
LOG-01 refer to the [0.2.0 review](../reviews/2026-11-06-full-repository-review-0.2.0.md).
"Integrator" in the last column means the row becomes an obligation of use in the
Safety Manual (see section 1.6).

Data flow through the parts: integrator input, options and buffer → **Input** →
**Lexer and Unicode** → **Parser** → document in **Memory** → read through the
**Value model** and the **Public API**. **Diagnostics** carries errors and warnings back out.

#### Input

| ID | FM | Cause | Effect | SSR | Safety measure | Tests | Gap and action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| IN-01 | FM-4 | Cursor reads past the end of the input | Crash or garbage read | SSR-01, SSR-08 | Every cursor step checks the remaining length | `core/tests/input/cursor_tests.cpp` | — |
| IN-02 | FM-4 | Input overlaps the document buffer | The parser overwrites its own input | SSR-08 | Overlap check before parsing returns `wrong_type_or_access` | `tests/api/json5_document_tests.cpp` | — |
| IN-03 | FM-3 | Very large input | The parse takes too long | SSR-02 | `max_input_bytes` | `complete_document_tests`, resource guard tests | — |
| IN-04 | FM-1 | The integrator changes the input while it is being parsed, for example from another thread or by DMA | Values that never existed in one consistent input | — | None possible in the library | — | Integrator: the input stays unchanged during the parse |
| IN-05 | FM-5 | Wrong line or column | Misleading diagnostic only | SSR-03 | `locate` rules for LF, CR, CRLF, U+2028 and U+2029 | `tests/api/text_position_tests.cpp` | — |

#### Lexer and Unicode handling

| ID | FM | Cause | Effect | SSR | Safety measure | Tests | Gap and action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| LX-01 | FM-1 | An escape or surrogate is decoded wrongly | Wrong string value, reported as success | SSR-06 | Exact UTF-16 units are kept; unpaired surrogates are warned | `core/tests/lexer/string/`, `core/tests/unicode/`, validation fixtures | — |
| LX-02 | FM-1 | An invalid token is accepted, for example JSON5 syntax in strict mode | Invalid data treated as valid | SSR-03, SSR-06 | Mode-specific lexers; strict mode rejects JSON5-only syntax | `tests/api/strict_json_tests.cpp`, JSONTestSuite validation | — |
| LX-03 | FM-2 | A valid token is rejected | Correct data is lost and the integrator stays in its fallback | J5-027 (QM) | Grammar implemented per specification | json5-tests validation, `grammar_evidence_tests` | Rejecting valid input is not unsafe by itself, because the integrator falls back (A-06), so correct acceptance stays a QM requirement backed by the conformance tests. Integrator: if staying in fallback too long is dangerous for their function, they must handle it, for example with a timeout into a safe state |
| LX-04 | FM-1, FM-2 | A generated Unicode table is wrong (tool fault) | Whitespace or identifiers are misclassified | SSR-06 | Tables are checked against the Unicode data | `core/tests/unicode/unicode_tables_tests.cpp` | Tool confidence, work product 7 |
| LX-05 | FM-3 | A very long string, number or exponent | Long run time or memory use | SSR-02 | `max_string_units`, `max_total_string_units`, `max_number_bytes`, `max_exponent_digits` | lexer string and number tests | — |
| LX-06 | FM-5 | Words starting with `N` or `I` go to the non-finite lexer | Odd error position, no range | SSR-03 | An unsigned word that is not exactly `Infinity` or `NaN` is rejected like any unknown word: `unknown_literal` with a whole-word `source_range`, in both modes | `tests/api/error_reason_tests.cpp` (`J5_043_UnknownWord`), `core/tests/lexer/number/non_finite_tests.cpp` | Done in #125 (LOG-04). Owner decides whether the row is closed. |

#### Parser

| ID | FM | Cause | Effect | SSR | Safety measure | Tests | Gap and action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PA-01 | FM-1 | A grammar error or trailing content is accepted | Invalid document reported as valid | SSR-03 | Complete-input check: one root, permitted trivia, end of input | `complete_document_tests`, `grammar_evidence_tests`, validation fixtures | — |
| PA-02 | FM-1 | The count pass and the build pass disagree | The document is built differently from what was counted | SSR-03 | Run-time agreement checks return `internal_error`, distinct from every caller error; the document is rejected | `core/tests/parser/two_pass_tests.cpp`, `core/tests/parser/parser_engine_tests.cpp`, `core/tests/value/string_storage_tests.cpp` | Done in #125 (LOG-06). Owner decides whether the row is closed. |
| PA-03 | FM-1 | A failure in the middle of the parse | A partial document is used as if complete | SSR-03 | Atomic result: no document is published on failure (J5-051) | `complete_document_tests`, allocation audit tests | — |
| PA-04 | FM-3 | Deep nesting or a huge document | Long run time, large memory or stack use | SSR-02, SSR-07 | `max_depth`, `max_nodes`, `max_members`, `max_work_units`; explicit container stack instead of recursion | `resource_guards_tests`, `work_baseline_tests` | Stack use is not measured yet: #136 |
| PA-05 | FM-4 | The frame stack grows and old frame pointers go stale | Use of moved or freed memory | SSR-08 | Frames are moved on growth | `parser_engine_tests`, `parser_transition_tests` | Evidence to be confirmed by a unit test |
| PA-06 | FM-5 | A limit error has no position and does not name the limit | The integrator cannot tell which limit to raise | SSR-03 | `limit_reason()` names the limit; token-bound limits are located at the offending token, whole-input and memory limits have no location by design | `tests/api/error_reason_tests.cpp` (`J5_047_LimitReason`, `J5_017_ExcessWarning`), `core/tests/parser/error_location_tests.cpp` | Done in #125 (LOG-03). Owner decides whether the row is closed. |

#### Value model and conversions

| ID | FM | Cause | Effect | SSR | Safety measure | Tests | Gap and action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| VM-01 | FM-1 | Integer conversion truncates, wraps or clamps | `as_int64()` returns 300 for 3000, as success | SSR-06 | Exact value or error; never rounds, wraps or clamps | `tests/value/integer_conversion_tests.cpp` | — |
| VM-02 | FM-1 | Floating conversion rounds wrongly | A slightly wrong `double`, as success | SSR-06 | One rounding, to nearest, ties to even, using big-integer arithmetic | `floating_conversion_tests`, `big_unsigned_tests` | `convert_to_floating` is very long and complex: #127 (STD-07) |
| VM-03 | FM-2 | `as_double()` uses up the caller buffer on repeated reads | Later reads fail for valid data | SSR-02, SSR-06 | — | — | **Gap (LOG-01, High):** fix in #124, with a regression test |
| VM-04 | FM-4 | `as_double()` is `const` but changes shared document state | A data race if two threads read one document | SSR-05 | A-02: one thread per document | `tests/api/concurrency_tests.cpp` (separate documents only) | **Gap (LOG-02):** owner decision: reads become free of side effects, in #124. A-02 stays strict even afterwards |
| VM-05 | FM-3 | `member()` walks the member list from the start for every step | Lookup time grows quadratically with object size | SSR-02 | `max_members` bounds the object size | — | **Gap (LOG-11):** owner decision: make the lookup faster, in #124 |
| VM-06 | FM-1 | The integrator modifies a parsed document through exposed mutators | The document no longer matches the input | SSR-03 | — | — | **Gap (ARC-03):** owner decision: both. The mutators move into a `detail` namespace (#128), and the Safety Manual forbids them. Integrator: never modify a parsed document |
| VM-07 | FM-1 | The document in caller memory is corrupted by other software | Wrong values are read | — | None in the library | — | Covered by A-07 (freedom from interference) |
| VM-08 | FM-5 | A missing member, a duplicated member and a non-object all give the same error | The integrator cannot react differently | SSR-03 | `access_reason()` gives `absent_member`, `duplicate_member` or `wrong_type`; a damaged internal link gives `internal_error` | `tests/api/error_reason_tests.cpp` (`J5_039_AccessReason`), `core/tests/value/relationship_validation_tests.cpp` | Done in #125 (LOG-05). Owner decides whether the row is closed. |

#### Memory

| ID | FM | Cause | Effect | SSR | Safety measure | Tests | Gap and action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ME-01 | FM-4 | A block is placed outside the buffer or misaligned | Memory corruption | SSR-08 | Alignment and capacity checks; checked size arithmetic | `buffer_arena_tests`, `storage_tests`, `checked_size_tests` | — |
| ME-02 | FM-3, FM-4 | A limit is not enforced because of a miscount | Memory use beyond the planned budget | SSR-02 | Every allocation is charged against the limits | `accounted_storage_tests`, `allocation_audit_tests` | — |
| ME-03 | FM-4 | A block is released twice, with the wrong size or with a wrong address | Without a check: the byte counter wraps and every later allocation fails; a release that does not wrap leaves the counter too low, so later allocations can pass `max_owned_bytes` (SSR-02); a double release reaches the provider as a double free (SSR-08) | SSR-02, SSR-08 | `accounted_storage` counts outstanding blocks and refuses, before the provider sees it, a null address, zero bytes, a release with no block outstanding, more bytes than the outstanding blocks can hold and a last block of a different size. A refused release leaves the counters unchanged and latches `faulted()`; every later allocation fails with `internal_error`. Inside the library only `owned_block` releases, exactly once | `tests/memory/accounted_storage_tests.cpp` (`J5_046_MismatchedRelease`), also under the sanitizer job | Done in #125 (LOG-07). A wrong size among several outstanding blocks is detected only when it exceeds what the other blocks can hold; per-block headers were not chosen. Integrator: A-09. Owner decides whether the row is closed. |
| ME-04 | FM-1 | An allocation fails in the middle of a parse | A partial document | SSR-03 | All allocations of the call are released; no document is published | `allocation_audit_tests` (fault injection) | — |
| ME-05 | FM-4 | The integrator's memory functions return bad memory | Corruption the library cannot see | — | None possible | — | Integrator (A-05) |
| ME-06 | FM-5 | A bad alignment request is reported as `resource_limit` | Wrong reaction to a caller error | SSR-03 | An alignment that is not a power of two is `wrong_type_or_access` with `invalid_argument`, before any provider call | `tests/memory/storage_tests.cpp`, `tests/memory/accounted_storage_tests.cpp`, `tests/memory/buffer_arena_tests.cpp` | Done in #125 (LOG-08). Owner decides whether the row is closed. |

#### Diagnostics

| ID | FM | Cause | Effect | SSR | Safety measure | Tests | Gap and action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| DG-01 | FM-5 | The wrong error category | The integrator chooses the wrong reaction, but still rejects the document (A-06) | SSR-03 | Fixed error-code table; `internal_error` for library faults; one reason per code, readable only under that code | `result_tests`, `syntax_reason_tests`, `error_range_tests`, `error_reason_tests`, `diagnostic_format_tests` | Category fixes done in #125. Owner decides whether the row is closed. |
| DG-02 | FM-4 | The formatted message is written past the caller buffer | Memory corruption | SSR-08 | Capacity check; always NUL-terminated; truncates | `tests/api/diagnostic_format_tests.cpp` | — |
| DG-03 | FM-2 | Too many warnings | Valid input is rejected | SSR-02 | `max_diagnostics`, on purpose, to keep memory bounded | `parser_warning_tests` | Integrator sizes `max_diagnostics` (A-04) |
| DG-04 | FM-5 | An error is stored without its location when memory is short | Less information only | SSR-03 | Minimal error reporting needs no allocation | allocation failure tests | — |

#### Public API

| ID | FM | Cause | Effect | SSR | Safety measure | Tests | Gap and action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| AP-01 | FM-1 | The integrator ignores a result | An error is treated as success | — | `[[nodiscard]]` on results | — | Integrator (A-06) |
| AP-02 | FM-4 | A moved-from document or value is used | Crash or garbage | SSR-01 | A moved-from document is empty and its accessors return errors | `document_tests`, `access_boundary_tests` | A unit test will show this for every accessor (#138). Integrator: never use a moved-from document |
| AP-03 | FM-4 | A document is used after its buffer is freed or reused | Dangling memory | — | Lifetime contract in `json5_document.hpp` | — | The library only holds a pointer and cannot detect this. Integrator: the buffer stays alive and reserved until the document is destroyed |
| AP-04 | FM-1 | The hosted add-on is linked into a safety build | Heap use in a safety build | SSR-05 | Build switch off by default; not in the release packages | `build_configuration_tests` | Integrator (A-05) |
| AP-05 | FM-1 | The integrator uses document mutators | See VM-06 | SSR-03 | — | — | See VM-06 |

### 1.6 Obligations for integrators found by this analysis

These are collected here and will go into the Safety Manual:

1. The input stays unchanged during the parse (IN-04).
2. If staying in fallback too long is dangerous for the integrator's function, the
   integrator handles it, for example with a timeout into a safe state (LX-03).
3. A parsed document is never modified through the document mutators (VM-06).
4. A moved-from document is never used (AP-02).
5. A document's buffer stays alive and reserved until the document is destroyed (AP-03).
6. Every document has its own buffer (DFA-01).
7. Accounted memory is released only through `owned_block`, and `faulted()` is checked
   after releasing a document (ME-03, A-09).
8. `internal_error` is treated as a library fault, never as bad input (PA-02, DG-01).

## 2. Dependent failure analysis

Each entry names a cause that could break more than one thing at once, what the
library does to prevent it, and what the integrator must do to contain it.

| ID | Dependent failure | What could happen | Prevention in the library | Containment by the integrator | Related |
| --- | --- | --- | --- | --- | --- |
| DFA-01 | Two documents share a buffer | The integrator gives the same or overlapping memory to two documents, or lets the input overlap the buffer. One document then overwrites the other, or the parser overwrites its own input. | The library **detects** input overlapping the buffer and returns `wrong_type_or_access` (IN-02). It **cannot detect** two documents sharing one buffer; the contract that each buffer is reserved for one document is documented. | Give every document its own buffer. This becomes an obligation of use in the Safety Manual. | SSR-08, A-02, A-05 |
| DFA-02 | The E2E check and the parser share code or a common bug | One fault breaks both parts of the ASIL B(D) decomposition at once, so the E2E check misses the error the parser made. | None possible inside the library. | The E2E check must not use this library or code shared with it (A-08), and the integrator documents this in their own DFA. | A-08 |
| DFA-03 | A shared toolchain | The same compiler or standard library bug affects both the library and the E2E check. | Tool confidence evaluation for the library's tools (work product 7). | Can only be noted and passed on: the integrator evaluates their own toolchain, for example with tool qualification or by building the two parts with different tools. | Work product 7 |
| DFA-04 | Buffer overrun | The library writes past its buffer and corrupts whatever is next to it in memory, such as another document or the E2E check's own data. One bug then breaks both parts of the decomposition. | SSR-08: the library never writes outside the memory it was given. Shown by unit tests; further methods are decided in the verification strategy (work product 6). | Memory protection (MPU or MMU) so that an overrun cannot reach the other part (A-07). | SSR-08, A-07 |
| DFA-05 | Stack overflow | Deeply nested or crafted input makes the stack grow into neighbouring memory. This is the same kind of cascade as DFA-04, but through the stack. | SSR-07: stack use has a known upper limit that depends only on the configured limits. The parser keeps open containers in an explicit stack in caller storage instead of using recursion (see the [parser architecture](../../core/docs/design-architecture.md), section 7); the actual stack use is still to be measured (#136). | A stack guard or MPU region below the stack, and a stack size checked against the documented limit (A-07). | SSR-07, A-01, A-07 |
| DFA-06 | Timing interference | A very large or crafted input keeps the CPU busy so long that other tasks, including the E2E check, miss their deadlines. | SSR-02: the work budget (`max_work_units`) and the other limits stop a parse that would take too long. | A watchdog or other timing supervision, and limits chosen to fit the timing budget (A-04, A-07). | SSR-02, A-04, A-07 |
