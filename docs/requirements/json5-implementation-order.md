# Full JSON5 parser requirements in implementation order

Copyright 2026 Razvan-Stefan Roman. Project additions are Apache-2.0.
Specification-derived explanations retain the source notices linked below.

Status: **Documented implementation baseline; no user review required.**
Specification checked 2026-10-12. No production parser is implemented.
This is the sequential core-parser checklist. IDs P001–P041 are local project
identifiers, not numbers issued by the JSON5 authors. Each prerequisite has a
smaller number: implement in ascending order; no step requires a later one.
Acceptance at an early step is a unit-level component check, not a finished-parser
claim. Containers' mutual recursion is deliberately implemented together in P033.
Tests, formatting and build setup remain agent-authorized; production C++ remains
reserved for the developer.

## Official sources and authority

- [JSON5 Data Interchange Format 1.0.0, March 2018](https://spec.json5.org/):
  §§2–9 language semantics/grammar; §10 parser acceptance and permitted limits;
  §A normative versus informative text. Its examples are informative.
- [ECMAScript 5.1, June 2011](https://262.ecma-international.org/5.1/):
  only lexical productions referenced by JSON5, notably §§6,7.2–7.4,7.6,7.8.3–7.8.4.
  This does not make JSON5 an executable JavaScript evaluator.

The specification defines behavior, not a C++ implementation order, API, UTF-8
transport, allocator, error type or build system. The order below is this project's
engineering choice. **JSON5** basis means a language rule; **Project** means an
additional contract. A SHOULD warning and permitted duplicate/limit choices are
marked explicitly. JSON5 permits extensions, but this project rejects unsupported
syntax. Strict JSON mode, schema, editor, consumer profiles and the example CLI
are separate requirements in [the complete project requirements](../requirements.md).
They are not prerequisites for this core JSON5 implementation sequence.

Existing J5 IDs are retained for issues/traceability; cross-links below do not
import their old dependency ordering. Read those IDs for additional obligations,
not as future prerequisites for an early P-step. The P dependency column governs
this implementation sequence. [Machine-readable checklist](json5-implementation-order.json)
contains the same entries for dependency checks.

## Numbered implementation requirements

### P001 — Independent C++17 build

Provide the library component and GoogleTest units as separate native C++17 targets; no OS or host-tool dependency enters the core.

- Prerequisites: None.
- Basis: Project.
- Unit acceptance: Compile the configuration units; keep production implementation reserved for the developer.
- Existing contracts: J5-001, J5-002, J5-091, J5-101.

### P002 — Non-throwing result foundation

Define success/error alternatives with stable minimal error code and byte offset; returning any failure shall not require allocation or throw.

- Prerequisites: P001.
- Basis: Project.
- Unit acceptance: Unit-test minimal result construction in exceptions OFF/ON; no parser needed.
- Existing contracts: J5-003, J5-043, J5-045.

### P003 — Fallible storage and accounting

Provide caller-supplied fallible storage, checked size arithmetic, ownership and cleanup primitives before implementing growing parser structures.

- Prerequisites: P002.
- Basis: Project.
- Unit acceptance: Fail each primitive allocation and verify explicit outcomes and balanced release.
- Existing contracts: J5-040, J5-046, J5-051, J5-101.

### P004 — Explicit input cursor

Consume immutable bytes with explicit length and end position; bound every read and preserve original byte locations.

- Prerequisites: P002.
- Basis: Project.
- Unit acceptance: Empty, embedded-zero and truncated buffers; no terminating NUL assumption.
- Existing contracts: J5-004, J5-041, J5-044.

### P005 — Unicode source decoding

Decode the project UTF-8 transport to source characters without replacement; retain byte-to-source mapping.

- Prerequisites: P004.
- Basis: Project transport; JSON5 §9.1.
- Unit acceptance: Malformed, overlong, surrogate-encoded and out-of-range UTF-8; supplementary characters.
- Existing contracts: J5-009.

### P006 — Character classification

Establish documented Unicode data for source classification used by whitespace and IdentifierName; classification has no locale dependency.

- Prerequisites: P005.
- Basis: JSON5 §§1,9; ES5 §§6,7.6; Project version policy.
- Unit acceptance: Category boundaries, non-ASCII and non-BMP handling; document supported Unicode version.
- Existing contracts: J5-006, J5-019, J5-052.

### P007 — Line terminators

Recognize LF, CR, CRLF, U+2028 and U+2029 as line terminator sequences; keep CRLF as one sequence for location/continuation purposes.

- Prerequisites: P005.
- Basis: JSON5 §§5.1,7–9; ES5 §7.3.
- Unit acceptance: Each sequence and EOF after CR; assert source spans.
- Existing contracts: J5-011, J5-017, J5-044.

### P008 — Whitespace

Skip HT, VT, FF, SP, NBSP, BOM and other Unicode Zs whitespace, together with line terminators, between tokens.

- Prerequisites: P006, P007.
- Basis: JSON5 §§8,9.1.
- Unit acceptance: Each admitted space before/after tokens; BOM both leading and between tokens.
- Existing contracts: J5-011, J5-025.

### P009 — Line comments

Skip // comments until a line terminator or EOF; comments are trivia rather than values.

- Prerequisites: P007.
- Basis: JSON5 §§7,9.1.
- Unit acceptance: All terminators, empty comment and EOF comment; validate non-ASCII source.
- Existing contracts: J5-012.

### P010 — Block comments

Skip /* comments through the first */; do not nest comments, and report unterminated comments.

- Prerequisites: P005.
- Basis: JSON5 §§7,9.1.
- Unit acceptance: First closer, embedded opener and EOF at every suffix position.
- Existing contracts: J5-012.

### P011 — Trivia composition

Permit any sequence of admitted spaces, line terminators and comments before/after tokens, without joining split tokens.

- Prerequisites: P008, P009, P010.
- Basis: JSON5 §§7–9.
- Unit acceptance: Mixed trivia; comments between tokens; 1/*x*/2 remains two tokens.
- Existing contracts: J5-012, J5-024.

### P012 — Punctuation tokens

Recognize exactly the structural punctuators { } [ ] : , outside strings/comments.

- Prerequisites: P004, P011.
- Basis: JSON5 §9.1.
- Unit acceptance: Every punctuator, adjacent punctuators and original spans.
- Existing contracts: J5-013.

### P013 — String storage

Provide owned length-aware decoded-unit storage, preserving UTF-16 escape units including lone surrogates; define allocation-failure behavior.

- Prerequisites: P003, P005.
- Basis: Project representation; JSON5 §5.1.
- Unit acceptance: Empty, NUL, supplementary scalar and unpaired unit round-trips without UTF-8 substitution.
- Existing contracts: J5-018, J5-029, J5-040.

### P014 — Quoted string boundaries

Recognize single/double-quoted strings with matching closing delimiters; allow the other quote and ordinary source characters, rejecting raw CR/LF and unterminated strings.

- Prerequisites: P007, P013.
- Basis: JSON5 §§5,9.1.
- Unit acceptance: Both delimiters, opposite quote, raw tab/control characters, raw CR/LF and EOF.
- Existing contracts: J5-014.

### P015 — Simple character escapes

Decode escaped quotes, reverse solidus, b/f/n/r/t/v and zero; zero is valid only when not followed by a decimal digit.

- Prerequisites: P014.
- Basis: JSON5 §5.1.
- Unit acceptance: Each decoded unit; accept zero at quote/EOF boundary only where the complete string is valid; reject zero followed by any digit.
- Existing contracts: J5-015, J5-016.

### P016 — Hexadecimal string escapes

Decode x followed by exactly two hexadecimal digits, in either hex-letter case; reject incomplete or nonhex escapes.

- Prerequisites: P014.
- Basis: JSON5 §5.1.
- Unit acceptance: Lower/upper hex letters, boundary bytes and every truncation.
- Existing contracts: J5-016.

### P017 — Unicode string escapes

Decode u followed by exactly four hexadecimal digits; preserve individual units and support escaped surrogate pairs.

- Prerequisites: P013, P014.
- Basis: JSON5 §5.1.
- Unit acceptance: BMP, paired/lone/reversed surrogate units and malformed/truncated hex.
- Existing contracts: J5-015, J5-018.

### P018 — Non-escape characters

For permitted characters without a special escape production, omit the reverse solidus and retain the character; reject escaped digits 1–9.

- Prerequisites: P015, P016, P017.
- Basis: JSON5 §5.1; ES5 §7.8.4.
- Unit acceptance: a/e/slash/space yield those characters; 8/9 fail; x/u cannot fall back after malformed escape.
- Existing contracts: J5-016.

### P019 — String line continuations

Remove the reverse solidus and the following complete line terminator sequence from the decoded string.

- Prerequisites: P007, P014.
- Basis: JSON5 §5.1.
- Unit acceptance: LF, CR, CRLF, U+2028/U+2029 join unchanged surrounding text.
- Existing contracts: J5-017.

### P020 — Raw separator warnings

Accept unescaped U+2028/U+2029 in strings and produce a warning without changing successful parsing.

- Prerequisites: P002, P014.
- Basis: JSON5 §5.2 SHOULD; Project adopts warning.
- Unit acceptance: Both raw separators, escaped equivalents, original warning spans and successful value.
- Existing contracts: J5-017.

### P021 — Complete string lexer

Compose all string alternatives, validate delimiters and input consumption, and report lexical failures atomically.

- Prerequisites: P015, P016, P017, P018, P019, P020.
- Basis: JSON5 §§5,9.1.
- Unit acceptance: Combine escapes/continuations; truncation at each byte; no partial successful string.
- Existing contracts: J5-014, J5-015, J5-016, J5-017, J5-018.

### P022 — IdentifierName tokens

Recognize ES5 IdentifierName, including $, _, admitted Unicode starts/parts, ZWNJ/ZWJ continuation and reserved-word names.

- Prerequisites: P006.
- Basis: JSON5 §§3,9.1; ES5 §7.6.
- Unit acceptance: Category representatives, digit-first rejection and reserved-word tokens.
- Existing contracts: J5-019.

### P023 — Identifier Unicode escapes

Decode identifier uXXXX escapes before checking start/part legality and member-name equality.

- Prerequisites: P017, P022.
- Basis: JSON5 §9.1; ES5 §7.6.
- Unit acceptance: Escaped valid names, illegal escaped start/part and equal decoded names.
- Existing contracts: J5-020, J5-033.

### P024 — Null and boolean classification

Classify lowercase null, true and false as value literals; preserve their admissibility as object IdentifierName tokens.

- Prerequisites: P022, P023.
- Basis: JSON5 §§2,9.1.
- Unit acceptance: Exact spellings versus case/suffix variants; key-versus-value context classified without parsing containers.
- Existing contracts: J5-023.

### P025 — Decimal numeric tokens

Recognize decimal integer, fraction and exponent alternatives, including leading/trailing decimal points and optional leading sign; reject illegal leading zeros and missing exponent digits.

- Prerequisites: P004.
- Basis: JSON5 §§6,9.1; ES5 §7.8.3.
- Unit acceptance: 0/-0/+1/.5/5./5.e2/1E-2; reject 01, ., 1e and doubled signs.
- Existing contracts: J5-021, J5-022.

### P026 — Hexadecimal numeric tokens

Recognize signed or unsigned 0x/0X with one or more hexadecimal digits; reject incomplete prefix and invalid suffix.

- Prerequisites: P025.
- Basis: JSON5 §6.
- Unit acceptance: Both prefixes/cases/signs; 0x, 0x1z and hex fractions fail.
- Existing contracts: J5-022.

### P027 — Non-finite numeric tokens

Recognize Infinity and NaN with permitted leading signs and exact case.

- Prerequisites: P025.
- Basis: JSON5 §6.
- Unit acceptance: Infinity/+Infinity/-Infinity and NaN/+NaN/-NaN; lowercase and suffix variants fail.
- Existing contracts: J5-022.

### P028 — Token completion

Reject illegal suffixes and unsupported octal/binary/separator forms; never silently discard bytes or split invalid numeric/identifier spellings into accepted prefixes.

- Prerequisites: P012, P021, P023, P024, P025, P026, P027.
- Basis: JSON5 §9; Project no extensions.
- Unit acceptance: truex, 1foo, 0x1z, 0b1, 1_0 and split-token trivia; valid punctuation boundaries.
- Existing contracts: J5-024, J5-027.

### P029 — Owned numeric representation

Retain exact numeric spellings and represent signed decimal/hex and non-finite values without unintended floating-point rounding.

- Prerequisites: P003, P025, P026, P027.
- Basis: Project; JSON5 §10 permits numeric limits.
- Unit acceptance: Integers beyond 2^53, huge exponents, negative zero and failure-safe owned storage.
- Existing contracts: J5-034, J5-038, J5-040.

### P030 — Owned value/container model

Provide distinct null/boolean/string/number/array/object alternatives, ordered array elements and observable ordered duplicate object members.

- Prerequisites: P003, P013, P029.
- Basis: JSON5 §§1–4; Project object/ownership choices.
- Unit acceptance: Construct/inspect/move each variant before recursive parsing exists; explicit wrong-type results.
- Existing contracts: J5-028, J5-030, J5-031, J5-039, J5-040.

### P031 — Resource guards

Validate limits and account for depth, nodes, members, strings, numeric spelling/exponents, bytes and work before parser growth; cleanup remains bounded.

- Prerequisites: P002, P003, P030.
- Basis: Project; JSON5 §10 MAY limits.
- Unit acceptance: Below/equal/above bounds and deterministic allocation/work exhaustion without recursive parser dependency.
- Existing contracts: J5-047, J5-048, J5-049.

### P032 — Primitive value dispatcher

Convert completed string, number, null and boolean tokens into owned values; reject other identifiers in value position.

- Prerequisites: P021, P024, P028, P029, P030, P031.
- Basis: JSON5 §§2,9.2.
- Unit acceptance: Each primitive root, unknown identifiers and allocation failure; containers still explicitly unsupported at this stage.
- Existing contracts: J5-023, J5-028.

### P033 — Recursive container engine

Implement bounded value-dispatch frames for arrays and objects together, with empty containers, nesting and punctuation expectations.

- Prerequisites: P012, P030, P031, P032.
- Basis: JSON5 §§3,4,9.2.
- Unit acceptance: Empty arrays/objects and mixed nesting; allocate/check frames before descent; this step intentionally resolves mutual recursion together.
- Existing contracts: J5-013, J5-027, J5-048.

### P034 — Array element grammar

Complete array element/separator states, heterogeneous ordered elements and one optional trailing comma; reject holes and misplaced separators.

- Prerequisites: P033.
- Basis: JSON5 §§4,9.2.
- Unit acceptance: [], mixed values, nested values, [1,]; reject [,], [1,,2] and missing commas.
- Existing contracts: J5-013, J5-030.

### P035 — Object member grammar

Complete quoted/identifier name, colon, value and separator states; permit one trailing comma and retain decoded names and every member.

- Prerequisites: P021, P023, P024, P033.
- Basis: JSON5 §§3,9.2.
- Unit acceptance: Empty/nested objects, reserved-word/escaped keys and mixed values; reject missing name/colon/value/comma and doubled commas.
- Existing contracts: J5-013, J5-019, J5-020, J5-031.

### P036 — Duplicate-name policy

Keep all decoded duplicate members by default; optional rejection reports both occurrences. Compare exact decoded names without normalization/case folding.

- Prerequisites: P035.
- Basis: Project; JSON5 §3 allows implementation choices.
- Unit acceptance: Quoted/bare/escaped collisions, distinct case and normalization forms, nested object independence.
- Existing contracts: J5-032, J5-033.

### P037 — Complete document entry point

Accept one root value of any supported type surrounded by trivia; reject empty/trivia-only input, extra roots and trailing nontrivia.

- Prerequisites: P011, P032, P034, P035, P036.
- Basis: JSON5 §§2,9.2,10.
- Unit acceptance: Every root type, trailing EOF comment, {} {} and junk; atomic errors and reusable context.
- Existing contracts: J5-026, J5-051.

### P038 — Checked numeric operations

Provide non-throwing signed/unsigned/floating conversion and exact comparison under documented policies without boolean coercion, rounding/wrapping or locale dependence.

- Prerequisites: P029, P030, P037.
- Basis: Project.
- Unit acceptance: All destination bounds, nonintegral values, huge exponent, nonfinite and mathematical/lexical integer distinctions.
- Existing contracts: J5-035, J5-036, J5-037, J5-038, J5-052.

### P039 — Owned document and diagnostics

Expose stable byte spans, warning/error locations and documented views; destruction/moves and input-buffer destruction do not invalidate owned data.

- Prerequisites: P020, P030, P037.
- Basis: Project.
- Unit acceptance: Overwrite input, move/destroy documents, CRLF/non-BMP offsets, failure injection and minimal error fallback.
- Existing contracts: J5-040, J5-041, J5-043, J5-044, J5-045.

### P040 — Whole-grammar unit evidence

Map every JSON5 lexical/syntactic production and referenced ES5 alternative to named GoogleTest positive, negative, boundary and truncation cases; exercise resource failures and both exception modes.

- Prerequisites: P001, P002, P003, P004, P005, P006, P007, P008, P009, P010, P011, P012, P013, P014, P015, P016, P017, P018, P019, P020, P021, P022, P023, P024, P025, P026, P027, P028, P029, P030, P031, P032, P033, P034, P035, P036, P037, P038, P039.
- Basis: Project acceptance of JSON5 §10.
- Unit acceptance: No skipped grammar branches; fresh explicit unit evidence for accepted/rejected input and decoded content.
- Existing contracts: J5-027, J5-094, J5-095, J5-097, J5-098, J5-099.

### P041 — Full-support claim

Document language/Unicode versions, permitted limits and all deviations before claiming full JSON5 1.0.0 parsing support.

- Prerequisites: P040.
- Basis: JSON5 §§10,A; Project release evidence.
- Unit acceptance: Review production-to-unit matrix, all preceding contracts and actual native results.
- Existing contracts: J5-008, J5-027, J5-100.

## Grammar coverage review

P007–P012 cover source/trivia/punctuators. P013–P021 cover both string character
productions, every escape branch and continuations. P022–P024 cover identifiers
and literal classification. P025–P029 cover numeric alternatives and boundaries.
P032–P037 cover JSON5Text, JSON5Value, objects/member lists/member names and
arrays/element lists. P040 verifies the full production matrix before P041.

Checked UTF-8 export must reject lone surrogate units while parsing preserves
admitted escaped units. Ordinary raw control characters inside JSON5 strings
must not inherit strict JSON's stronger restrictions. Raw U+2028/U+2029 remain
valid. Escaped digits, legacy octal, binary literals, numeric separators, array
elisions, semicolons and executable expressions are not JSON5 extensions here.

The official specification's permission to limit number range/string content is
not a reason to silently claim this project's stronger exact-number/surrogate
contracts implemented. Full support is always described with explicit limits.

## Source licensing

This is an implementation-oriented explanation of the cited standards, not a
modified official standard. The JSON5 specification is MIT-licensed; retain
[its notice](../../LICENSES/JSON5-Specification-MIT.txt). The referenced ES5
implementation explanations retain [Ecma's document notice](../../LICENSES/ECMA-262-5.1-Document.txt).
The project license does not replace either source's permissions.

User approval is not a prerequisite for these documented requirements. Implement
specification-derived behavior as cited; preserve MUST/SHOULD/MAY distinctions.
Project additions remain explicitly identified and are authorized by the current
user scope. Baseline status is not evidence of implementation or passing tests.
