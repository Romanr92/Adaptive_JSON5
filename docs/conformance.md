# JSON5 1.0.0 support statement (P041 reviewed)

This page collects what the parser supports, the limits it applies and every
known deviation or project decision, as required before claiming full JSON5
1.0.0 parsing support (P041; J5-008, J5-027, J5-100). The parser-scoped technical
review and required PR checks are complete for `66a425a` in PR #100. The claim
is full JSON5 1.0.0 parsing within the documented encoding, policies and limits.
Requirement rows remain
Draft; this review does not verify optional components or promote shared rows.

## Versions

| Item | Version |
| --- | --- |
| JSON5 | [JSON5 Data Interchange Format 1.0.0](https://spec.json5.org/), March 2018 |
| Strict JSON | RFC 8259 / ECMA-404 grammar |
| ECMAScript lexical rules referenced by JSON5 | ECMAScript 5.1 |
| Unicode data | 17.0.0, plus the 3.0.0 tables ES5 requires as its minimum ([provenance](dependencies.md)) |
| Input encoding | UTF-8 only |
| C++ | C++17. Tested with GCC on Linux x86-64, Linux aarch64 and Windows MinGW, exceptions OFF and ON, RTTI OFF. Other compilers and RTTI ON are untested. |

## Supported syntax

| Mode | Grammar | Evidence |
| --- | --- | --- |
| `syntax_mode::json5` (default) | Every JSON5 1.0.0 production | [Grammar evidence matrix](../core/docs/design-grammar-evidence.md); json5-tests corpus |
| `syntax_mode::strict_json` | RFC 8259 JSON text; every JSON5-only feature is rejected | `J5_027_ModePair`; P042 `tests/api/strict_json_tests.cpp`; JSONTestSuite `y_`/`n_` |

Any value type may be the root. Empty or trivia-only input is a syntax error.
Strict mode accepts no extensions: RFC 8259 §9 permits a parser to accept
non-JSON forms, but this one does not, except the opt-in `strict_bom` option.
Errors are returned as `result` values in every build; nothing throws.

Syntax errors also carry a `syntax_error_reason` (issue #96). `format_error`, `format_warning` and their
`_at` forms in `diagnostic_format.hpp` render `line: L, column: C: <text>` into caller storage, with
1-based lines and code-point columns from `locate`. Unlocated errors print only the text; duplicate names
add `(first at line: L, column: C)`; warnings add `warning: `. Output is truncated with a NUL and the full
length is returned; formatting allocates nothing and ignores the locale. The English text is stable but is
for people only: programs use `error_code` and the reason fields. Unit evidence: `D96_` suites in
`tests/api/syntax_reason_tests.cpp` and `tests/api/diagnostic_format_tests.cpp`.

## Default limits

RFC 8259 §9 permits limits on text size, nesting depth, numbers and strings, and
J5-027 requires the grammar within documented limits. The parser's limits are
inclusive, finite and configurable per call through `parse_options::limits`;
exceeding one returns `resource_limit`, never a wrong result. The defaults are
listed in [architecture § 3](../core/docs/design-architecture.md#3-storage-ownership-and-budgets).

The defaults are conservative, chosen for untrusted input: for example 16 KiB of
input, 1024 values, 1024 members and 4096 UTF-16 units per string. Valid
documents beyond them fail until the caller raises the limits. Accepted pinned
fixtures fit within the defaults, including 500 nested arrays and
a 131-digit exponent. The final defaults and per-value memory cost (about
400-550 owned bytes per value) are tracked by
[#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69). Parser work grows
linearly with the input and the number of values, except duplicate rejection,
which is quadratic in the members of one object.

## Implementation-defined behaviour

RFC 8259 leaves these choices to each implementation. Outcomes are for default
options. All 35 pinned JSONTestSuite `i_` files have explicit project outcomes
in `J5_095_ProjectOutcome.EnforcesDocumentedBehaviour`, including exact rejection
categories/offsets and surrogate-warning spans. The inventory unit rejects missing,
duplicate or stale classifications. Upstream files and labels are unchanged.

| Topic | Files | Strict | JSON5 | Behaviour |
| --- | --- | --- | --- | --- |
| Huge, overflowing or underflowing numbers | `i_number_*` (10) | accept | accept | Parsing keeps the exact spelling and never converts. Range problems are reported only by the explicit conversion functions (`out_of_range`, `underflow`, `non_finite`). |
| Lone, reversed or incomplete escaped surrogates | 9 escaped-surrogate string fixtures plus `i_object_key_lone_2nd_surrogate` | accept with warning | accept with warning | Decoded strings keep the exact UTF-16 units. Each unpaired surrogate escape reports `unpaired_surrogate_escape`. Checked UTF-8 export rejects unpaired surrogates; nothing is replaced. |
| Invalid UTF-8 (bad bytes, overlong, encoded surrogates, beyond U+10FFFF, truncated, Latin-1) | 10 `i_string_*` files | `encoding` | `encoding` | Rejected at the offset of the invalid sequence. |
| UTF-16 input, with or without BOM | `i_string_UTF-16LE_with_BOM`, `i_string_utf16*_no_BOM` | reject | reject | Only UTF-8 is accepted. |
| 500 nested arrays | `i_structure_500_nested_arrays` | accept | accept | Within the default `max_depth` (512). |
| UTF-8 BOM before the value | `i_structure_UTF-8_BOM_empty_object` | `syntax` at 0 | accept | Strict mode rejects by default; `strict_bom = skip_one` skips exactly one. JSON5 treats U+FEFF as whitespace. |

## Project decisions and deviations

The surrogate advisory is a project addition, selected for this claim (option
(b)); [RFC 8259 §8.2](https://www.rfc-editor.org/rfc/rfc8259.html#section-8.2)
describes interoperability problems with unpaired surrogates but does not require
a warning. In both modes, quoted member names and string values report one
`unpaired_surrogate_escape` warning per unpaired escaped unit, covering the six
original bytes of its `\uXXXX` escape. Valid pairs produce no warning, including
pairs joined across JSON5 line continuations, which add no decoded units.
Malformed or truncated `\u` escapes remain syntax errors. Warnings preserve
successful parsing and exact units within configured limits; each retained
warning consumes one `max_diagnostics` entry, so exhausting that limit returns
`resource_limit`. This decision does not change numeric, input-encoding, nesting
or BOM behaviour.

Classified as **spec choice** (the specification permits it), **addition**
(beyond the specification, opt-in or advisory) or **limit**.

| Item | Class | Decision |
| --- | --- | --- |
| U+200B as JSON5 whitespace | spec choice | Zs in Unicode 3.0, which ES5 names as its minimum; accepted deliberately. |
| Raw U+2028/U+2029 in strings | spec choice | Accepted in both modes; JSON5 mode reports `raw_line_separator`/`raw_paragraph_separator` warnings (JSON5 §5.1). |
| Duplicate member names | spec choice | Kept in order by default (`preserve_all`); `reject_duplicates` compares decoded UTF-16 units exactly, without normalization. |
| Strict-mode BOM | addition | `strict_bom_policy::skip_one`, off by default. |
| Retained source text | addition | `source_retention_policy::retain_copy`, off by default. |
| Warnings and line/column | addition | Every warning is retained (bounded by `max_diagnostics`); `locate()` converts byte offsets to 1-based line and code-point column. |
| Strict error offsets | spec choice | Rejected number spellings report the token start, escapes the escaped character, line continuations the line terminator. |
| Number comparison exponent | limit | Exponent magnitudes above `PTRDIFF_MAX / 4` return `resource_limit` in exact comparison. |
| Extreme floating conversion | limit | Some extreme values, e.g. the smallest double subnormal, need more than the default work budget. |
| Lone surrogates | spec choice + addition | Preserved in decoded strings; `unpaired_surrogate_escape` warns in both modes. Checked UTF-8 export fails rather than substituting. |
| Upstream fixture label | corpus | `json5-tests/comments/irregular-block-comment.json` is labelled valid JSON but contains a comment; strict mode rejects it ([fixture README](../tests/fixtures/third_party/README.md)). |

## Thread safety and runtime

The core uses no filesystem, process, network, clock, locale or OS-thread
services and has no shared mutable global state. Separate documents may be used
concurrently; a shared storage provider needs caller synchronization. See
[architecture § 10](../core/docs/design-architecture.md#10-runtime-and-concurrency-boundaries).

## Regression checklist mapping

Parser rows of the [minimum unit regression checklist](requirements.md#minimum-unit-regression-checklist).
Schema, editor and manifest rows are outside P041.

| Checklist row | Covering suites |
| --- | --- |
| `{key: 'value',}` and its JSON equivalent | `J5_097_Equivalence`, `J5_027_ModePair` |
| Primitive/array roots and empty/comment-only input | `J5_026_CompleteRoot`, `J5_026_CompleteRejection`, `J5_026_EmptyRoot` |
| Comment delimiters inside strings, comments next to tokens | `J5_012_Comments`, `J5_024_TriviaBoundary`, json5-tests `comments/` |
| Bare `true`/`null` keys, escaped names, digit-first names | `J5_023_IdentifierClassification`, `J5_020_IdentifierEscapes`, `J5_019_IdentifierBoundary` |
| Non-ASCII identifiers and Unicode Zs | `J5_019_UnicodeClassifier`, `J5_006_UnicodeTableMembership`, `J5_027_ModePair` |
| `\x41`, `\v`, `\0`, `\a`, backslash-space, `\'` | `J5_015_SimpleEscape`, `J5_016_HexEscape`, `J5_016_NonEscape`, `J5_016_ZeroDigit` |
| Raw tab/NUL and raw CR/LF in strings | `J5_014_Controls` |
| Line continuations and raw U+2028/U+2029 | `J5_017_Continuation`, `J5_017_StringWarning`, `J5_017_CompleteWarnings` |
| Paired and unpaired escaped surrogates | `J5_018_UnicodePair`, `J5_018_Unpaired`, `J5_018_UnicodeEscape`, `J5_018_SurrogateWarnings`, `J5_018_CompleteWarnings`, `J5_018_StringWarnings` |
| `{a:1, "a":2, "a":3}` | `J5_032_DuplicateDefault`, `J5_032_DuplicatePolicy` |
| Normalized-looking names, colliding hashes | `J5_033_DecodedName` |
| `1`, `1.0`, `1e0`, `0x1`, `+1`, `1e-1` | `J5_036_NumberComparison`, `J5_035_IntegerConversion`, `J5_037_FloatingConversion` |
| `9007199254740993`, integer bounds, huge hex | `J5_035_IntegerConversion`, `J5_034_OwnedNumberRange` |
| Huge exponent versus Infinity/NaN; negative zero | `J5_037_FloatingConversion`, `J5_022_NonFinite` |
| Deep/wide structures, large strings/numbers | `J5_047_CompleteLimit`, `J5_048_ContainerDepth`, `J5_049_WorkGrowth` |
| Allocation failure during parse | `J5_098_DocumentFaultSweep`, `J5_041_RetainedSource` |

## Contract reviews

Parser-scoped technical review at `803dca3` plus the P041 follow-up, 2026-10-25.
This reviews the documented API/source boundaries, production-to-unit matrix and
executed evidence; it is not a line-by-line proof or MISRA/CERT certification.
The table covers the preceding core contracts and the parser portions of shared
release contracts. Schema, CLI, editor, consumer acceptance and device execution
are outside this claim. CI for the added test evidence is recorded below.

| Contract | Review result (2026-10-25) |
| --- | --- |
| J5-002 build components | Parser part met. CMake 3.25 is the minimum. A library-only configure (`release-library`) fetches no GoogleTest or fixtures and builds only `adaptive_json5_parser`. Unit and validation tests are separate opt-in options. Strict JSON (P042, #81) is part of the parser; schema and tool targets are later steps (P043 onwards); `ADAPTIVE_JSON5_BUILD_SCHEMA` currently stops configuration. |
| J5-007 license and provenance | Met for current material. `LICENSE` is Apache-2.0. `LICENSES/` keeps GoogleTest, JSON5 specification, ECMA-262, Unicode, json5-tests and JSONTestSuite notices; `NOTICE`, [dependencies](dependencies.md) and the [fixture README](../tests/fixtures/third_party/README.md) record versions and provenance. |
| J5-042 pure memory parsing | Met. `J5_042_PureMemory.ProductionIncludesOnlyLanguageSupportHeaders` checks every production include against a language-support allowlist (no files, streams, processes, threads, time, locale or networking). `J5_042_PureMemory.ParseUsesOnlyCallerStorage` shows a complete parse, with retained source and warnings, makes no default-heap allocation. |
| J5-008 public contracts | Met. Syntax defaults, duplicate handling, retention and BOM options are in `options.hpp`; value lifetimes and pointer invalidation in `document.hpp`, `value_view.hpp` and `parser.hpp`; limits in `resource_limits.hpp`; conversions in the conversion headers; each `error_code` value is described in `error.hpp`; thread safety is stated on `parse_document`, `document` and `accounted_storage`. |
| J5-001, J5-003–006 foundation and source model | Reviewed for the parser. CMake requires C++17; `result`/`error`, explicit input lengths and `parse_options` define non-throwing modes and input ownership. Cursor/input/options units cover boundaries. Unicode tables identify 17.0.0 plus the ES5 minimum 3.0.0, with pinned provenance. |
| J5-009–027 grammar and completeness | Reviewed against every row of the P040 matrix and the JSON5 1.0.0 lexical/syntactic productions. UTF-8, trivia, names, strings/escapes, numbers, containers, BOM and complete input have positive, negative, boundary and applicable truncation units. No unresolved grammar branch is listed. Project-defined i_ outcomes now have exact validation expectations. |
| J5-028–034, J5-039–041 values, ownership and access | Reviewed for the public document/view/storage APIs and compiled implementations. Units cover all root kinds, ordered children and duplicate names, exact spellings/UTF-16 units, checked access, source spans, input destruction, moves and failure cleanup. No serializer or streaming API is claimed. |
| J5-035–038 numeric operations | Reviewed for integer/floating conversion and exact comparison. Named numeric units cover bounds, lexical versus mathematical integrality, rounding, non-finite policy, negative zero, scratch/work exhaustion and allocation failure. The exponent-comparison and extreme-conversion limits above are retained as explicit limitations. |
| J5-043–049, J5-051 failures, resources and reuse | Reviewed for parser/value/conversion scope. Structured errors, minimal allocation-free failure reporting, caller storage, inclusive limits, iterative depth handling and deterministic work budgets have named boundary/fault/reuse units. Retention and diagnostics share the owned-byte budget. Cancellation is supplied by deterministic budgets here; no extra cancellation API is claimed. |
| J5-050, J5-052 concurrency and determinism | The host-only `P041/J5_050_ConcurrentParsing.IsolatesSuccessFailuresAndLiveDocuments` runs four workers per syntax mode, sharing immutable input/options and retaining independent documents while repeating syntax, encoding, resource and allocation failures and reuse. Storage/accounting remain separate. Numeric locale units remain separate evidence; a skipped locale case must not be described as an executed locale comparison. Shared mutable accounting/provider access still requires caller synchronization. |
| J5-094–095, J5-097–099 unit and native evidence | Reviewed for core scope. The grammar matrix, equivalence/truncation/fault cases, immutable external fixtures and explicit project outcomes cover P040/P041. Native exception OFF/ON and RTTI OFF results at `66a425a` are recorded below. Linux locale cases skipped; Windows locale cases executed and passed. Schema fixture J5-096 is outside parser scope. |
| J5-100 parser release completeness | Versions, public contracts, grammar/requirement inventories, defaults, provenance, usage and deviations are linked from this statement and README. Stale scaffold/P004/P040 status text is corrected. P041 is tracked by #80. Technical review and required CI are complete at `66a425a`; PR review/merge remain human workflow steps. No schema/editor completeness or embedded target support is inferred. |

PR #100 scope review (2026-10-25): P038 numeric bounds/integrality/rounding,
P039 ownership/spans/diagnostic fallback, P040 grammar/fault evidence and P041
versions/limits/deviations have corresponding implementation and named units.
No additional functional blocker was identified in this acceptance-focused review.
The machine-readable P038–P041 trackers also needed their owner/status and CI
scope synchronized; that documentation correction is in the current working tree.
The review found copied float wording in `to_double()`'s Doxygen text. It is
corrected in this documentation-only follow-up under explicit user authorization;
the `result<double>` signature and implementation are unchanged.

## Executed results

| Where | Commit | Configuration | Result |
| --- | --- | --- | --- |
| GitHub Actions [unit-tests](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40188400393) | `016c7f4` | Linux GCC exceptions OFF and ON; linux-aarch64 g++-13 OFF and ON; Windows MinGW OFF and ON | Passed |
| GitHub Actions [validation tests](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40188400388) | `016c7f4` | Linux, exceptions OFF and ON | Passed |
| GitHub Actions [formatting](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40188400395) | `016c7f4` | clang-format 18.1.8 | Passed |
| GitHub Actions [unit-tests](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192016985) | `803dca3` | Linux x86-64 GCC, Linux aarch64 GCC and Windows MinGW, exceptions OFF and ON, RTTI OFF | All host unit jobs passed |
| GitHub Actions [validation](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192016968) | `803dca3` | Linux, exceptions OFF and ON, RTTI OFF | Both validation jobs passed |
| GitHub Actions [formatting](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192016981) | `803dca3` | clang-format 18.1.8 | Passed |
| GitHub Actions [Linux x86-64 units](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192865734/job/121167408166) | `66a425a` | GCC 13.3.0, exceptions OFF and ON, RTTI OFF | Each mode: 2146 passed, 1 skipped, 0 failed (2147 registered). Locale comparison skipped because no comma-decimal locale was installed. |
| GitHub Actions [Linux aarch64 units](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192865734/job/121167408167) | `66a425a` | GCC 13.3.0, exceptions OFF and ON, RTTI OFF | Each mode: 2146 passed, 1 skipped, 0 failed (2147 registered). Same locale skip. |
| GitHub Actions [Windows MinGW units](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192865734/job/121167408039) | `66a425a` | GCC 16.2.0, exceptions OFF and ON, RTTI OFF | Each mode: 2147 passed, 0 failed; locale comparison executed and passed. |
| GitHub Actions [validation](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192865741) | `66a425a` | Linux GCC 13.3.0, exceptions OFF and ON, RTTI OFF | Each mode: 921/921 passed. |
| GitHub Actions [formatting](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40192865738) | `66a425a` | clang-format 18.1.8 | Passed. |
| Local, Windows GCC 15.2.0, CMake 4.0.1 | `016c7f4` plus the J5-042 units | Unit tests, exceptions OFF and ON, RTTI OFF | 2126/2126 passed in each mode |
| Local, same toolchain | `016c7f4` | Validation tests, exceptions OFF and ON | 850/850 passed in each mode |
| Local, Windows GCC 15.2.0, CMake 4.0.1 (2026-10-25) | Changes subsequently committed as `803dca3` | Unit tests, exceptions OFF and ON, RTTI OFF | 2145/2145 passed in each mode; includes 19 new surrogate-warning cases. A final assertion-safety adjustment was followed by 70/70 affected string/surrogate cases in each mode. |
| Local, same toolchain (2026-10-25) | Changes subsequently committed as `803dca3` | Validation tests, exceptions OFF and ON, RTTI OFF | 850/850 passed in each mode |
| Local (2026-10-25) | Changes subsequently committed as `803dca3` | clang-format 18.1.8 | Formatting passed for all 213 C++ files |
| Local, Windows GCC 15.2.0, CMake 4.0.1 (2026-10-25) | Changes subsequently committed as `66a425a` | Unit tests, exceptions OFF and ON, RTTI OFF | 2147/2147 passed in each mode; includes both concurrent-context cases |
| Local, same toolchain (2026-10-25) | Changes subsequently committed as `66a425a` | Validation tests, exceptions OFF and ON, RTTI OFF | 921/921 passed in each mode; includes 70 exact project outcomes and one inventory case |
| Local (2026-10-25) | Changes subsequently committed as `66a425a` | clang-format 18.1.8 | Formatting passed for all 214 C++ files |

The J5-042 units passed in CI at `6e24e8b` ([unit-tests](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40188954377), [validation](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40188954321), [formatting](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/40188954423)).

## Review and merge status

All required PR checks for `66a425ab2213f8239643101ec7c45fd405413e1b` completed
successfully. These results cover the concurrent-context and project-outcome
tests. PR #100 remains subject to developer review and merge; no merge was
performed. This review's documentation-only corrections are not part of the
tested commit. Requirement rows remain Draft.
