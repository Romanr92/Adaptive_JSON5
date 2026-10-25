# JSON5 1.0.0 support statement (P041 draft)

This page collects what the parser supports, the limits it applies and every
known deviation or project decision, as required before claiming full JSON5
1.0.0 parsing support (P041; J5-008, J5-027, J5-100). It is a **draft**: the
claim is not made until the open items at the end are closed and the
requirement rows are reviewed. Requirement rows remain Draft.

## Versions

| Item | Version |
| --- | --- |
| JSON5 | [JSON5 Data Interchange Format 1.0.0](https://spec.json5.org/), March 2018 |
| Strict JSON | RFC 8259 / ECMA-404 grammar |
| ECMAScript lexical rules referenced by JSON5 | ECMAScript 5.1 |
| Unicode data | 17.0.0, plus the 3.0.0 tables ES5 requires as its minimum ([provenance](dependencies.md)) |
| Input encoding | UTF-8 only |
| C++ | C++17; exceptions ON or OFF, RTTI OFF supported |

## Supported syntax

| Mode | Grammar | Evidence |
| --- | --- | --- |
| `syntax_mode::json5` (default) | Every JSON5 1.0.0 production | [Grammar evidence matrix](grammar-evidence.md); json5-tests corpus |
| `syntax_mode::strict_json` | RFC 8259 JSON text; every JSON5-only feature is rejected | `J5_027_ModePair`; JSONTestSuite `y_`/`n_` |

Any value type may be the root. Empty or trivia-only input is a syntax error.
Errors are returned as `result` values in every build; nothing throws.

## Default limits

Limits are inclusive, finite and configurable per call through
`parse_options::limits`; exceeding one returns `resource_limit`. The defaults are
listed in [architecture § 3](architecture.md#3-storage-ownership-and-budgets).
They accept every syntactically valid file in both pinned corpora, including
500 nested arrays and a 131-digit exponent. They are host defaults, not embedded
recommendations; the final defaults and per-value memory cost are tracked by
[#69](https://github.com/Romanr92/Adaptive_JSON5/issues/69). Parser work grows
linearly with the input and the number of values, except duplicate rejection,
which is quadratic in the members of one object.

## Implementation-defined behaviour

RFC 8259 leaves these choices to each implementation. Outcomes are for default
options and were observed on the pinned JSONTestSuite `i_` files.

| Topic | Files | Strict | JSON5 | Behaviour |
| --- | --- | --- | --- | --- |
| Huge, overflowing or underflowing numbers | `i_number_*` (10) | accept | accept | Parsing keeps the exact spelling and never converts. Range problems are reported only by the explicit conversion functions (`out_of_range`, `underflow`, `non_finite`). |
| Lone, reversed or incomplete escaped surrogates | `i_string_*surrogate*` (8), `i_object_key_lone_2nd_surrogate` | accept | accept | Decoded strings keep the exact UTF-16 units. Checked UTF-8 export rejects unpaired surrogates; nothing is replaced. |
| Invalid UTF-8 (bad bytes, overlong, encoded surrogates, beyond U+10FFFF, truncated, Latin-1) | 10 `i_string_*` files | `encoding` | `encoding` | Rejected at the offset of the invalid sequence. |
| UTF-16 input, with or without BOM | `i_string_UTF-16LE_with_BOM`, `i_string_utf16*_no_BOM` | reject | reject | Only UTF-8 is accepted. |
| 500 nested arrays | `i_structure_500_nested_arrays` | accept | accept | Within the default `max_depth` (512). |
| UTF-8 BOM before the value | `i_structure_UTF-8_BOM_empty_object` | `syntax` at 0 | accept | Strict mode rejects by default; `strict_bom = skip_one` skips exactly one. JSON5 treats U+FEFF as whitespace. |

## Project decisions and deviations

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
| Lone surrogates | spec choice | Preserved in decoded strings; checked UTF-8 export fails rather than substituting. |
| Upstream fixture label | corpus | `json5-tests/comments/irregular-block-comment.json` is labelled valid JSON but contains a comment; strict mode rejects it ([fixture README](../tests/fixtures/third_party/README.md)). |

## Thread safety and runtime

The core uses no filesystem, process, network, clock, locale or OS-thread
services and has no shared mutable global state. Separate documents may be used
concurrently; a shared storage provider needs caller synchronization. See
[architecture § 10](architecture.md#10-runtime-and-concurrency-boundaries).

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
| Paired and unpaired escaped surrogates | `J5_018_UnicodePair`, `J5_018_Unpaired`, `J5_018_UnicodeEscape` |
| `{a:1, "a":2, "a":3}` | `J5_032_DuplicateDefault`, `J5_032_DuplicatePolicy` |
| Normalized-looking names, colliding hashes | `J5_033_DecodedName` |
| `1`, `1.0`, `1e0`, `0x1`, `+1`, `1e-1` | `J5_036_NumberComparison`, `J5_035_IntegerConversion`, `J5_037_FloatingConversion` |
| `9007199254740993`, integer bounds, huge hex | `J5_035_IntegerConversion`, `J5_034_OwnedNumberRange` |
| Huge exponent versus Infinity/NaN; negative zero | `J5_037_FloatingConversion`, `J5_022_NonFinite` |
| Deep/wide structures, large strings/numbers | `J5_047_CompleteLimit`, `J5_048_ContainerDepth`, `J5_049_WorkGrowth` |
| Allocation failure during parse | `J5_098_DocumentFaultSweep`, `J5_041_RetainedSource` |

## Open before the claim

1. Record executed results for the current suites, with CI run references.
2. Add a J5-042 unit (parsing touches no file or OS service) and record the
   J5-002/J5-007 build and licensing review.
3. Review the J5-008 public-contract list against the public headers.
4. Update the README status once the above is done.
