# Full JSON5 parser requirements in implementation order

Copyright 2026 Razvan-Stefan Roman. Project additions are Apache-2.0.
Specification-derived explanations retain the source notices linked below.

Status: **Documented implementation baseline; no user review required.**
Specification checked 2026-10-12. No production parser is implemented.
This is the sequential core-parser checklist. IDs P001–P066 are local project
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

GitHub issues #2–#35 map to P002–P035. PR #36 occupies a GitHub number;
P036–P066 therefore map to issues #37–#67. Build/runtime/consumer/release completion
is tracked in #68–#72. P001 configuration evidence exists; the real library target
remains in #68. P002 is complete, without implying complete J5 error contracts.
Implementation issues use three acceptance items: documentation, implementation,
and unit tests. Required builds use exceptions OFF and RTTI OFF; historical ON
results remain historical evidence. See [the current roadmap](../roadmap.md).

## Numbered implementation requirements

### P001 — Independent C++17 build

Tracking: [#1](https://github.com/Romanr92/Adaptive_JSON5/issues/1).

Provide the library component and GoogleTest units as separate native C++17 targets; no OS or host-tool dependency enters the core.

- Prerequisites: None.
- Basis: Project.
- Unit acceptance: Compile the configuration units; keep production implementation reserved for the developer.
- Existing contracts: J5-001, J5-002, J5-091, J5-101.

### P002 — Non-throwing result foundation

Tracking: [#2](https://github.com/Romanr92/Adaptive_JSON5/issues/2).

Define success/error alternatives with stable minimal error code and byte offset; returning any failure shall not require allocation or throw.

Implementation: developer-authored `error.hpp` and `result.hpp`. Local unit
evidence: 40 result cases passed on Windows/GCC with exceptions OFF/ON and RTTI
OFF on 2026-10-13; see [verification notes](../verification.md). This component
evidence does not complete the broader J5 contracts or establish a remote matrix.

- Prerequisites: P001.
- Basis: Project.
- Unit acceptance: Unit-test minimal result construction in exceptions OFF; no parser needed.
- Existing contracts: J5-003, J5-043, J5-045.

### P003 — Fallible storage and accounting

Tracking: [#3](https://github.com/Romanr92/Adaptive_JSON5/issues/3).

Provide caller-supplied fallible storage, checked size arithmetic, ownership and cleanup primitives before implementing growing parser structures.

Implementation: developer-authored `checked_size.hpp`, `storage.hpp` and `owned_block.hpp`.
Local unit evidence: 39 storage cases passed on Windows/GCC with exceptions OFF and ON and RTTI OFF on 2026-10-13; see [verification notes](../verification.md). This component evidence does not complete broader J5-040, J5-046, J5-051 and J5-101 contracts or establish a remote matrix.

- Prerequisites: P002.
- Basis: Project.
- Unit acceptance: Fail each primitive allocation and verify explicit outcomes and balanced release.
- Existing contracts: J5-040, J5-046, J5-051, J5-101.

### P004 — Explicit input cursor

Tracking: [#4](https://github.com/Romanr92/Adaptive_JSON5/issues/4).

Consume immutable bytes with explicit length and end position; bound every read and preserve original byte locations.

- Prerequisites: P002.
- Basis: Project.
- Unit acceptance: Empty, embedded-zero and truncated buffers; no terminating NUL assumption.
- Existing contracts: J5-004, J5-041, J5-044.

### P005 — Unicode source decoding

Tracking: [#5](https://github.com/Romanr92/Adaptive_JSON5/issues/5).

Decode the project UTF-8 transport to source characters without replacement; retain byte-to-source mapping.

- Prerequisites: P004.
- Basis: Project transport; JSON5 §9.1.
- Unit acceptance: Malformed, overlong, surrogate-encoded and out-of-range UTF-8; supplementary characters.
- Existing contracts: J5-009.

### P006 — Character classification

Tracking: [#6](https://github.com/Romanr92/Adaptive_JSON5/issues/6).

Establish documented Unicode data for source classification used by whitespace and IdentifierName; classification has no locale dependency.

- Prerequisites: P005.
- Basis: JSON5 §§1,9; ES5 §§6,7.6; Project version policy.
- Unit acceptance: Category boundaries, non-ASCII and non-BMP handling; document supported Unicode version.
- Existing contracts: J5-006, J5-019, J5-052.

### P007 — Line terminators

Tracking: [#7](https://github.com/Romanr92/Adaptive_JSON5/issues/7).

Recognize LF, CR, CRLF, U+2028 and U+2029 as line terminator sequences; keep CRLF as one sequence for location/continuation purposes.

- Prerequisites: P005.
- Basis: JSON5 §§5.1,7–9; ES5 §7.3.
- Unit acceptance: Each sequence and EOF after CR; assert source spans.
- Existing contracts: J5-011, J5-017, J5-044.

### P008 — Whitespace

Tracking: [#8](https://github.com/Romanr92/Adaptive_JSON5/issues/8).

Skip HT, VT, FF, SP, NBSP, BOM and other Unicode Zs whitespace, together with line terminators, between tokens.

- Prerequisites: P006, P007.
- Basis: JSON5 §§8,9.1.
- Unit acceptance: Each admitted space before/after tokens; BOM both leading and between tokens.
- Existing contracts: J5-011, J5-025.

### P009 — Line comments

Tracking: [#9](https://github.com/Romanr92/Adaptive_JSON5/issues/9).

Skip // comments until a line terminator or EOF; comments are trivia rather than values.

- Prerequisites: P007.
- Basis: JSON5 §§7,9.1.
- Unit acceptance: All terminators, empty comment and EOF comment; validate non-ASCII source.
- Existing contracts: J5-012.

### P010 — Block comments

Tracking: [#10](https://github.com/Romanr92/Adaptive_JSON5/issues/10).

Skip /* comments through the first */; do not nest comments, and report unterminated comments.

- Prerequisites: P005.
- Basis: JSON5 §§7,9.1.
- Unit acceptance: First closer, embedded opener and EOF at every suffix position.
- Existing contracts: J5-012.

### P011 — Trivia composition

Tracking: [#11](https://github.com/Romanr92/Adaptive_JSON5/issues/11).

Permit any sequence of admitted spaces, line terminators and comments before/after tokens, without joining split tokens.

- Prerequisites: P008, P009, P010.
- Basis: JSON5 §§7–9.
- Unit acceptance: Mixed trivia; comments between tokens; 1/*x*/2 remains two tokens.
- Existing contracts: J5-012, J5-024.

### P012 — Punctuation tokens

Tracking: [#12](https://github.com/Romanr92/Adaptive_JSON5/issues/12).

Recognize exactly the structural punctuators { } [ ] : , outside strings/comments.

- Prerequisites: P004, P011.
- Basis: JSON5 §9.1.
- Unit acceptance: Every punctuator, adjacent punctuators and original spans.
- Existing contracts: J5-013.

### P013 — String storage

Tracking: [#13](https://github.com/Romanr92/Adaptive_JSON5/issues/13).

Provide owned length-aware decoded-unit storage, preserving UTF-16 escape units including lone surrogates; define allocation-failure behavior.

- Prerequisites: P003, P005.
- Basis: Project representation; JSON5 §5.1.
- Unit acceptance: Empty, NUL, supplementary scalar and unpaired unit round-trips without UTF-8 substitution.
- Existing contracts: J5-018, J5-029, J5-040.

### P014 — Quoted string boundaries

Tracking: [#14](https://github.com/Romanr92/Adaptive_JSON5/issues/14).

Recognize single/double-quoted strings with matching closing delimiters; allow the other quote and ordinary source characters, rejecting raw CR/LF and unterminated strings.

- Prerequisites: P007, P013.
- Basis: JSON5 §§5,9.1.
- Unit acceptance: Both delimiters, opposite quote, raw tab/control characters, raw CR/LF and EOF.
- Existing contracts: J5-014.

### P015 — Simple character escapes

Tracking: [#15](https://github.com/Romanr92/Adaptive_JSON5/issues/15).

Decode escaped quotes, reverse solidus, b/f/n/r/t/v and zero; zero is valid only when not followed by a decimal digit.

- Prerequisites: P014.
- Basis: JSON5 §5.1.
- Unit acceptance: Each decoded unit; accept zero at quote/EOF boundary only where the complete string is valid; reject zero followed by any digit.
- Existing contracts: J5-015, J5-016.

### P016 — Hexadecimal string escapes

Tracking: [#16](https://github.com/Romanr92/Adaptive_JSON5/issues/16).

Decode x followed by exactly two hexadecimal digits, in either hex-letter case; reject incomplete or nonhex escapes.

- Prerequisites: P014.
- Basis: JSON5 §5.1.
- Unit acceptance: Lower/upper hex letters, boundary bytes and every truncation.
- Existing contracts: J5-016.

### P017 — Unicode string escapes

Tracking: [#17](https://github.com/Romanr92/Adaptive_JSON5/issues/17).

Decode u followed by exactly four hexadecimal digits; preserve individual units and support escaped surrogate pairs.

- Prerequisites: P013, P014.
- Basis: JSON5 §5.1.
- Unit acceptance: BMP, paired/lone/reversed surrogate units and malformed/truncated hex.
- Existing contracts: J5-015, J5-018.

### P018 — Non-escape characters

Tracking: [#18](https://github.com/Romanr92/Adaptive_JSON5/issues/18).

For permitted characters without a special escape production, omit the reverse solidus and retain the character; reject escaped digits 1–9.

- Prerequisites: P015, P016, P017.
- Basis: JSON5 §5.1; ES5 §7.8.4.
- Unit acceptance: a/e/slash/space yield those characters; 8/9 fail; x/u cannot fall back after malformed escape.
- Existing contracts: J5-016.

### P019 — String line continuations

Tracking: [#19](https://github.com/Romanr92/Adaptive_JSON5/issues/19).

Remove the reverse solidus and the following complete line terminator sequence from the decoded string.

- Prerequisites: P007, P014.
- Basis: JSON5 §5.1.
- Unit acceptance: LF, CR, CRLF, U+2028/U+2029 join unchanged surrounding text.
- Existing contracts: J5-017.

### P020 — Raw separator warnings

Tracking: [#20](https://github.com/Romanr92/Adaptive_JSON5/issues/20).

Accept unescaped U+2028/U+2029 in strings and produce a warning without changing successful parsing.

- Prerequisites: P002, P014.
- Basis: JSON5 §5.2 SHOULD; Project adopts warning.
- Unit acceptance: Both raw separators, escaped equivalents, original warning spans and successful value.
- Existing contracts: J5-017.

### P021 — Complete string lexer

Tracking: [#21](https://github.com/Romanr92/Adaptive_JSON5/issues/21).

Compose all string alternatives, validate delimiters and input consumption, and report lexical failures atomically.

- Prerequisites: P015, P016, P017, P018, P019, P020.
- Basis: JSON5 §§5,9.1.
- Unit acceptance: Combine escapes/continuations; truncation at each byte; no partial successful string.
- Existing contracts: J5-014, J5-015, J5-016, J5-017, J5-018.

### P022 — IdentifierName tokens

Tracking: [#22](https://github.com/Romanr92/Adaptive_JSON5/issues/22).

Recognize ES5 IdentifierName, including $, _, admitted Unicode starts/parts, ZWNJ/ZWJ continuation and reserved-word names.

- Prerequisites: P006.
- Basis: JSON5 §§3,9.1; ES5 §7.6.
- Unit acceptance: Category representatives, digit-first rejection and reserved-word tokens.
- Existing contracts: J5-019.

### P023 — Identifier Unicode escapes

Tracking: [#23](https://github.com/Romanr92/Adaptive_JSON5/issues/23).

Decode identifier uXXXX escapes before checking start/part legality and member-name equality.

- Prerequisites: P017, P022.
- Basis: JSON5 §9.1; ES5 §7.6.
- Unit acceptance: Escaped valid names, illegal escaped start/part and equal decoded names.
- Existing contracts: J5-020, J5-033.

### P024 — Null and boolean classification

Tracking: [#24](https://github.com/Romanr92/Adaptive_JSON5/issues/24).

Classify lowercase null, true and false as value literals; preserve their admissibility as object IdentifierName tokens.

- Prerequisites: P022, P023.
- Basis: JSON5 §§2,9.1.
- Unit acceptance: Exact spellings versus case/suffix variants; key-versus-value context classified without parsing containers.
- Existing contracts: J5-023.

### P025 — Decimal numeric tokens

Tracking: [#25](https://github.com/Romanr92/Adaptive_JSON5/issues/25).

Recognize decimal integer, fraction and exponent alternatives, including leading/trailing decimal points and optional leading sign; reject illegal leading zeros and missing exponent digits.

- Prerequisites: P004.
- Basis: JSON5 §§6,9.1; ES5 §7.8.3.
- Unit acceptance: 0/-0/+1/.5/5./5.e2/1E-2; reject 01, ., 1e and doubled signs.
- Existing contracts: J5-021, J5-022.

### P026 — Hexadecimal numeric tokens

Tracking: [#26](https://github.com/Romanr92/Adaptive_JSON5/issues/26).

Recognize signed or unsigned 0x/0X with one or more hexadecimal digits; reject incomplete prefix and invalid suffix.

- Prerequisites: P025.
- Basis: JSON5 §6.
- Unit acceptance: Both prefixes/cases/signs; 0x, 0x1z and hex fractions fail.
- Existing contracts: J5-022.

### P027 — Non-finite numeric tokens

Tracking: [#27](https://github.com/Romanr92/Adaptive_JSON5/issues/27).

Recognize Infinity and NaN with permitted leading signs and exact case.

- Prerequisites: P025.
- Basis: JSON5 §6.
- Unit acceptance: Infinity/+Infinity/-Infinity and NaN/+NaN/-NaN; lowercase and suffix variants fail.
- Existing contracts: J5-022.

### P028 — Token completion

Tracking: [#28](https://github.com/Romanr92/Adaptive_JSON5/issues/28).

Reject illegal suffixes and unsupported octal/binary/separator forms; never silently discard bytes or split invalid numeric/identifier spellings into accepted prefixes.

- Prerequisites: P012, P021, P023, P024, P025, P026, P027.
- Basis: JSON5 §9; Project no extensions.
- Unit acceptance: truex, 1foo, 0x1z, 0b1, 1_0 and split-token trivia; valid punctuation boundaries.
- Existing contracts: J5-024, J5-027.

### P029 — Owned numeric representation

Tracking: [#29](https://github.com/Romanr92/Adaptive_JSON5/issues/29).

Retain exact numeric spellings and represent signed decimal/hex and non-finite values without unintended floating-point rounding.

- Prerequisites: P003, P025, P026, P027.
- Basis: Project; JSON5 §10 permits numeric limits.
- Unit acceptance: Integers beyond 2^53, huge exponents, negative zero and failure-safe owned storage.
- Existing contracts: J5-034, J5-038, J5-040.

### P030 — Owned value/container model

Tracking: [#30](https://github.com/Romanr92/Adaptive_JSON5/issues/30).

Provide distinct null/boolean/string/number/array/object alternatives, ordered array elements and observable ordered duplicate object members.

- Prerequisites: P003, P013, P029.
- Basis: JSON5 §§1–4; Project object/ownership choices.
- Unit acceptance: Construct/inspect/move each variant before recursive parsing exists; explicit wrong-type results.
- Existing contracts: J5-028, J5-030, J5-031, J5-039, J5-040.

### P031 — Resource guards

Tracking: [#31](https://github.com/Romanr92/Adaptive_JSON5/issues/31).

Validate limits and account for depth, nodes, members, strings, numeric spelling/exponents, bytes and work before parser growth; cleanup remains bounded.

- Prerequisites: P002, P003, P030.
- Basis: Project; JSON5 §10 MAY limits.
- Unit acceptance: Below/equal/above bounds and deterministic allocation/work exhaustion without recursive parser dependency.
- Existing contracts: J5-047, J5-048, J5-049.

### P032 — Primitive value dispatcher

Tracking: [#32](https://github.com/Romanr92/Adaptive_JSON5/issues/32).

Convert completed string, number, null and boolean tokens into owned values; reject other identifiers in value position.

- Prerequisites: P021, P024, P028, P029, P030, P031.
- Basis: JSON5 §§2,9.2.
- Unit acceptance: Each primitive root, unknown identifiers and allocation failure; containers still explicitly unsupported at this stage.
- Existing contracts: J5-023, J5-028.

### P033 — Recursive container engine

Tracking: [#33](https://github.com/Romanr92/Adaptive_JSON5/issues/33).

Implement bounded value-dispatch frames for arrays and objects together, with empty containers, nesting and punctuation expectations.

- Prerequisites: P012, P030, P031, P032.
- Basis: JSON5 §§3,4,9.2.
- Unit acceptance: Empty arrays/objects and mixed nesting; allocate/check frames before descent; this step intentionally resolves mutual recursion together.
- Existing contracts: J5-013, J5-027, J5-048.

### P034 — Array element grammar

Tracking: [#34](https://github.com/Romanr92/Adaptive_JSON5/issues/34).

Complete array element/separator states, heterogeneous ordered elements and one optional trailing comma; reject holes and misplaced separators.

- Prerequisites: P033.
- Basis: JSON5 §§4,9.2.
- Unit acceptance: [], mixed values, nested values, [1,]; reject [,], [1,,2] and missing commas.
- Existing contracts: J5-013, J5-030.

### P035 — Object member grammar

Tracking: [#35](https://github.com/Romanr92/Adaptive_JSON5/issues/35).

Complete quoted/identifier name, colon, value and separator states; permit one trailing comma and retain decoded names and every member.

- Prerequisites: P021, P023, P024, P033.
- Basis: JSON5 §§3,9.2.
- Unit acceptance: Empty/nested objects, reserved-word/escaped keys and mixed values; reject missing name/colon/value/comma and doubled commas.
- Existing contracts: J5-013, J5-019, J5-020, J5-031.

### P036 — Duplicate-name policy

Tracking: [#37](https://github.com/Romanr92/Adaptive_JSON5/issues/37).

Keep all decoded duplicate members by default; optional rejection reports both occurrences. Compare exact decoded names without normalization/case folding.

- Prerequisites: P035.
- Basis: Project; JSON5 §3 allows implementation choices.
- Unit acceptance: Quoted/bare/escaped collisions, distinct case and normalization forms, nested object independence.
- Existing contracts: J5-032, J5-033.

### P037 — Complete document entry point

Tracking: [#38](https://github.com/Romanr92/Adaptive_JSON5/issues/38).

Accept one root value of any supported type surrounded by trivia; reject empty/trivia-only input, extra roots and trailing nontrivia.

- Prerequisites: P011, P032, P034, P035, P036.
- Basis: JSON5 §§2,9.2,10.
- Unit acceptance: Every root type, trailing EOF comment, {} {} and junk; atomic errors and reusable context.
- Existing contracts: J5-026, J5-051.

### P038 — Checked numeric operations

Tracking: [#39](https://github.com/Romanr92/Adaptive_JSON5/issues/39).

Provide non-throwing signed/unsigned/floating conversion and exact comparison under documented policies without boolean coercion, rounding/wrapping or locale dependence.

- Prerequisites: P029, P030, P037.
- Basis: Project.
- Unit acceptance: All destination bounds, nonintegral values, huge exponent, nonfinite and mathematical/lexical integer distinctions.
- Existing contracts: J5-035, J5-036, J5-037, J5-038, J5-052.

### P039 — Owned document and diagnostics

Tracking: [#40](https://github.com/Romanr92/Adaptive_JSON5/issues/40).

Expose stable byte spans, warning/error locations and documented views; destruction/moves and input-buffer destruction do not invalidate owned data.

- Prerequisites: P020, P030, P037.
- Basis: Project.
- Unit acceptance: Overwrite input, move/destroy documents, CRLF/non-BMP offsets, failure injection and minimal error fallback.
- Existing contracts: J5-040, J5-041, J5-043, J5-044, J5-045.

### P040 — Whole-grammar unit evidence

Tracking: [#41](https://github.com/Romanr92/Adaptive_JSON5/issues/41).

Map every JSON5 lexical/syntactic production and referenced ES5 alternative to named GoogleTest positive, negative, boundary and truncation cases; exercise resource failures with exceptions OFF.

- Prerequisites: P001, P002, P003, P004, P005, P006, P007, P008, P009, P010, P011, P012, P013, P014, P015, P016, P017, P018, P019, P020, P021, P022, P023, P024, P025, P026, P027, P028, P029, P030, P031, P032, P033, P034, P035, P036, P037, P038, P039.
- Basis: Project acceptance of JSON5 §10.
- Unit acceptance: No skipped grammar branches; fresh explicit unit evidence for accepted/rejected input and decoded content.
- Existing contracts: J5-027, J5-094, J5-095, J5-097, J5-098, J5-099.

### P041 — Full-support claim

Tracking: [#42](https://github.com/Romanr92/Adaptive_JSON5/issues/42).

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

## Optional schema, CLI and VS Code implementation sequence

P001–P041 remain the independent JSON5 core. P042–P055 add strict JSON and the
optional Draft-07 validator; P056–P059 add CLI/example tools; P060–P066 add the
VS Code extension. No step requires a larger-numbered step. These are documented
baseline requirements and need no user review. Source behavior is governed by
[Draft-07 Core](https://json-schema.org/draft-07/draft-handrews-json-schema-01),
[Draft-07 Validation](https://json-schema.org/draft-07/draft-handrews-json-schema-validation-01)
and [VS Code's extension API](https://code.visualstudio.com/api/language-extensions/language-server-extension-guide).
CLI commands, component architecture and ordering are project choices. Existing
J5 contracts contain detailed semantic acceptance and authority links. No upstream
schema, test corpus or extension code is copied by this documentation change.

Schema compilation is staged deliberately: component shape/reference units first,
complete evaluation next, then public meta-schema-checked compilation. This avoids
requiring a finished validator to implement its first building blocks.
The schema component remains independently selected within library configuration;
these requirements do not introduce additional top-level build switches.
Handwritten consumer validation remains a supported alternative to choosing the
schema component; it is consumer-specific code, not another generic schema engine.

### P042 — Strict JSON syntax mode

Tracking: [#43](https://github.com/Romanr92/Adaptive_JSON5/issues/43).

Add explicit strict JSON mode for schema documents: quoted keys/double-quoted strings, JSON escapes/numbers/whitespace, no comments/trailing commas/non-finite forms; reject BOM by default with documented optional leading-BOM handling.

- Prerequisites: P041.
- Existing contracts: J5-005, J5-010, J5-015, J5-021, J5-025.
- Unit acceptance: Paired JSON/JSON5 fixtures, all JSON grammar branches and mode-specific rejection.

### P043 — Independent schema component and outcomes

Tracking: [#44](https://github.com/Romanr92/Adaptive_JSON5/issues/44).

Define an optional Draft-07 schema target/API with owned compilation/evaluation state, explicit non-throwing outcomes and bounded work storage. Parser-only consumers acquire no schema dependency. Declare instance-domain handling for duplicate names, non-finite values and unpaired surrogates.

- Prerequisites: P042.
- Existing contracts: J5-053, J5-063, J5-065, J5-066, J5-068.
- Unit acceptance: API/storage units and unsupported-instance versus invalid-instance/error categories; no complete validator assumed yet.

### P044 — Schema locations and offline reference registry

Tracking: [#45](https://github.com/Romanr92/Adaptive_JSON5/issues/45).

Implement URI base identities, pointer escaping and caller-registered local/external resources; no implicit network/filesystem resolution. Compile owned reference graphs with unresolved-reference errors.

- Prerequisites: P043.
- Existing contracts: J5-055, J5-064, J5-066, J5-067.
- Unit acceptance: Nested $id, relative/absolute references, fragment and ~0/~1 pointers, input/registry destruction.

### P045 — Exact schema equality

Tracking: [#46](https://github.com/Romanr92/Adaptive_JSON5/issues/46).

Provide type-aware deep equality using exact numeric equivalence, ordered arrays and order-independent unique-name objects.

- Prerequisites: P043.
- Existing contracts: J5-059, J5-061.
- Unit acceptance: Equivalent decimal/hex/exponent values; bool versus number; nested/reordered containers.

### P046 — Bounded schema evaluation engine

Tracking: [#47](https://github.com/Romanr92/Adaptive_JSON5/issues/47).

Evaluate boolean schemas and object schemas with per-type applicability; track branches/references/budgets and recursion progress. Apply Draft-07 $ref behavior, including ignored assertion siblings, without nonprogressing loops.

- Prerequisites: P044, P045.
- Existing contracts: J5-053, J5-056, J5-063, J5-065.
- Unit acceptance: True/false schemas, recursive instance progress, cyclic references, cleanup/cancellation and minimal errors.

### P047 — Type and equality assertions

Tracking: [#48](https://github.com/Romanr92/Adaptive_JSON5/issues/48).

Implement type, enum and const, including type unions and mathematical integer semantics independent of token spelling.

- Prerequisites: P045, P046.
- Existing contracts: J5-057, J5-059, J5-061.
- Unit acceptance: Positive/negative/nonapplicable-type cases and exact numeric parity.

### P048 — Numeric assertions

Tracking: [#49](https://github.com/Romanr92/Adaptive_JSON5/issues/49).

Implement multipleOf, minimum, maximum, exclusiveMinimum and exclusiveMaximum without floating rounding.

- Prerequisites: P047.
- Existing contracts: J5-057, J5-059.
- Unit acceptance: Exact fractional divisibility, huge numbers, every inclusive/exclusive bound.

### P049 — String assertions and regex budget

Tracking: [#50](https://github.com/Romanr92/Adaptive_JSON5/issues/50).

Implement minLength/maxLength in Unicode characters and pattern with the documented compatible regex behavior; reject unsupported conversion/domain and enforce regex work limits.

- Prerequisites: P046.
- Existing contracts: J5-057, J5-060, J5-065.
- Unit acceptance: Supplementary characters, unpaired-unit policy, unanchored patterns, malformed patterns and budget exhaustion.

### P050 — Array assertions

Tracking: [#51](https://github.com/Romanr92/Adaptive_JSON5/issues/51).

Implement items (schema/tuple), additionalItems, minItems, maxItems, uniqueItems and contains.

- Prerequisites: P045, P046.
- Existing contracts: J5-057, J5-061.
- Unit acceptance: Empty/tuple arrays, nested schemas, deep exact uniqueness and matching/nonmatching contains cases.

### P051 — Object assertions

Tracking: [#52](https://github.com/Romanr92/Adaptive_JSON5/issues/52).

Implement minProperties, maxProperties, required, properties, patternProperties, additionalProperties, dependencies (property/schema) and propertyNames.

- Prerequisites: P046, P049.
- Existing contracts: J5-057.
- Unit acceptance: Missing fields, overlapping patterns, dynamic names, dependent schemas, unknown fields and type applicability.

### P052 — Logical and conditional assertions

Tracking: [#53](https://github.com/Romanr92/Adaptive_JSON5/issues/53).

Implement allOf, anyOf, oneOf, not and if/then/else, accounting for all work and distinguishing operational failure from assertion mismatch.

- Prerequisites: P047, P048, P049, P050, P051.
- Existing contracts: J5-057, J5-065.
- Unit acceptance: Zero/one/multiple matching branches, nested conditions and failures during branch evaluation.

### P053 — Annotations and extension handling

Tracking: [#54](https://github.com/Romanr92/Adaptive_JSON5/issues/54).

Expose title/description/default/examples/readOnly/writeOnly and unknown-keyword metadata with valid-branch annotation semantics; never insert defaults or mutate instances. Baseline format/contentEncoding/contentMediaType remain annotations; optional assertions require explicit supported capabilities.

- Prerequisites: P052.
- Existing contracts: J5-058, J5-062, J5-069.
- Unit acceptance: No mutation, failed/not-branch annotation suppression, unknown schema keywords versus unknown instance properties.

### P054 — Public schema compilation and diagnostics

Tracking: [#55](https://github.com/Romanr92/Adaptive_JSON5/issues/55).

Complete strict-JSON boolean/object schema compilation with the Draft-07 meta-schema, keyword shape/pattern checks and dialect errors. Return instance/schema pointers, keyword and available source spans; support bounded fail-fast/collect-all.

- Prerequisites: P044, P052, P053.
- Existing contracts: J5-054, J5-063, J5-064.
- Unit acceptance: Bootstrap shape checks are component units before this step; final public compiler validates the meta-schema with the completed engine and separates every failure class.

### P055 — Schema conformance and packaging

Tracking: [#56](https://github.com/Romanr92/Adaptive_JSON5/issues/56).

Run pinned required Draft-07 unit fixtures with offline registered resources, exact-number/Unicode/failure regressions and owned concurrent work contexts. Document dependencies, capabilities and exported schema targets.

- Prerequisites: P054.
- Existing contracts: J5-053, J5-065, J5-066, J5-067, J5-068, J5-069, J5-096.
- Unit acceptance: Required keyword/reference matrix, exceptions-OFF units, provenance and parser-only independence.

### P056 — CLI dispatch and bounded readers

Tracking: [#57](https://github.com/Romanr92/Adaptive_JSON5/issues/57).

Prepare optional host command dispatch for file/stdin syntax selection and configured limits; read via injectable bounded I/O interfaces. Define generic validation commands without consumer-specific fields.

- Prerequisites: P042.
- Existing contracts: J5-070.
- Unit acceptance: Argument and fake I/O units; file errors, unsupported flags and input budget handling; schema flags initially report unavailable.

### P057 — CLI validation and diagnostic protocol

Tracking: [#58](https://github.com/Romanr92/Adaptive_JSON5/issues/58).

Connect parser and optional compiled-schema validation; provide readable and versioned strict-JSON diagnostics/capabilities. Exit 0 valid, 1 input/assertion/profile invalid, 2 configuration/I/O/operational failure. Encode surrogate/non-finite data safely.

- Prerequisites: P055, P056.
- Existing contracts: J5-070, J5-071.
- Unit acceptance: Dispatch/result mapping units, strict-JSON output round-trip, missing schema component and OOM fallback.

### P058 — File-driven example executable

Tracking: [#59](https://github.com/Romanr92/Adaptive_JSON5/issues/59).

Implement optional adaptive_json5_examples against examples/manifest.json with run/inspect/validate commands, matched expectations and requirement-specific observations; unsupported capabilities never count as passing. Preserve the distinct direct-validation and demonstration-run exit contracts documented in examples/README.md.

- Prerequisites: P057.
- Existing contracts: J5-070, J5-071, J5-094.
- Unit acceptance: Injected readers/reporters, valid/intentional-invalid examples and mismatch/resource outcomes; no e2e CI gate.

### P059 — Host-tool packaging

Tracking: [#60](https://github.com/Romanr92/Adaptive_JSON5/issues/60).

Expose the example executable behind ADAPTIVE_JSON5_BUILD_EXECUTABLE; install/version it independently, documenting library/executable/test presets. Parser and embedded consumers require no CLI/editor runtime.

- Prerequisites: P058.
- Existing contracts: J5-002, J5-007, J5-070, J5-092, J5-093.
- Unit acceptance: CMake target/options review, documented usage and unit capability/version output.

### P060 — VS Code extension package

Tracking: [#61](https://github.com/Romanr92/Adaptive_JSON5/issues/61).

Prepare a separate extension package with JSON5 language identity/file associations, pinned dependencies, native backend selection and supported versions; do not make it a library dependency.

- Prerequisites: P057.
- Existing contracts: J5-072.
- Unit acceptance: Package/contribution configuration units; reproducible packaging instructions, no new host-test CI gate.

### P061 — Editor grammar and position mapping

Tracking: [#62](https://github.com/Romanr92/Adaptive_JSON5/issues/62).

Use grammar fixtures and runtime diagnostics consistently; map original byte spans to documented editor positions, including astral Unicode, combining marks, CRLF and continuations.

- Prerequisites: P060.
- Existing contracts: J5-074, J5-075.
- Unit acceptance: Tokenization and range-adapter units with shared parser fixtures.

### P062 — Versioned live documents and provisional recovery

Tracking: [#63](https://github.com/Romanr92/Adaptive_JSON5/issues/63).

Support unsaved document versions, cancellation and stale-result suppression; provide explicitly provisional recovery for completion/hover without accepting incomplete runtime documents.

- Prerequisites: P061.
- Existing contracts: J5-076, J5-077.
- Unit acceptance: Rapid edits/reopen/cancellation and incomplete token/container unit cases.

### P063 — Offline workspace schema associations

Tracking: [#64](https://github.com/Romanr92/Adaptive_JSON5/issues/64).

Associate file patterns with repository-local schemas per workspace root, without inserting $schema in instances. Resolve registered resources offline; remote access requires explicit configuration.

- Prerequisites: P055, P062.
- Existing contracts: J5-073, J5-081.
- Unit acceptance: Two roots, relative references, changed associations and fake network that fails if called.

### P064 — Schema completions

Tracking: [#65](https://github.com/Romanr92/Adaptive_JSON5/issues/65).

Suggest applicable property names and enum/const/default/example values, required-field hints and quoting-aware insertion, avoiding normal duplicate properties.

- Prerequisites: P053, P063.
- Existing contracts: J5-078.
- Unit acceptance: Nested/referenced/conditional/union schemas and incomplete-input insertion units.

### P065 — Hover and runtime-parity diagnostics

Tracking: [#66](https://github.com/Romanr92/Adaptive_JSON5/issues/66).

Show applicable descriptions/types/constraints and agree with runtime exact-number/assertion semantics; expose duplicate, Unicode, root and lexical numeric profile rules separately from generic grammar.

- Prerequisites: P057, P064.
- Existing contracts: J5-079, J5-080.
- Unit acceptance: Hover metadata and parser/schema/profile parity units including mathematical versus lexical integer policies.

### P066 — Editor release evidence

Tracking: [#67](https://github.com/Romanr92/Adaptive_JSON5/issues/67).

Document extension/backend versions and executed tokenization/live-document/range/completion/hover/parity units. Distinguish unit evidence from unexecuted VS Code host acceptance; do not claim unsupported features.

- Prerequisites: P065.
- Existing contracts: J5-082, J5-100.
- Unit acceptance: Complete editor-feature-to-unit mapping and unit/format checks only.

