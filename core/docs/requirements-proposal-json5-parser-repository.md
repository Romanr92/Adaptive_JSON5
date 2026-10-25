# Standalone C++ JSON5 parser, schema validation and VS Code requirements

Prepared: 2026-10-11. **All requirements in this file are Draft.**

This file can be copied to the new repository as `docs/requirements.md`. It
defines a reusable, independently developed C++ parser and its supporting tools.
It does not approve or modify AdaptivePi's Release 4 requirements. The user's
choice to develop a parser independently is recorded; individual contracts below
still need review. Repository creation, implementation and publication are not
performed by this document.

## Scope and meaning of full support

The deliverable comprises:

1. A complete JSON5 1.0 reader and a selectable strict JSON reader.
2. Owned values, precise numeric information, explicit errors and controlled
   resource use, usable from C++17 with exceptions disabled.
3. A separately buildable JSON Schema Draft-07 validator and validation CLI.
4. VS Code support for the supported language, schema completion, hover help and
   diagnostics, including incomplete documents being edited.
5. An AdaptivePi manifest schema/profile example and integration guidance.
6. Tests, build/package integration and reproducible release evidence.

**Full JSON5** refers to the entire referenced grammar, not an ASCII-only or
comments-only subset. Documented finite resource limits remain permitted.
**Strict JSON** accepts the standard grammar without JSON5 extensions.
**Draft-07 support** covers its required assertion behavior and reference
semantics for interoperable JSON values. Optional format/content assertions are
annotation-only unless separately implemented and advertised. Neither a few
manifest-specific checks nor a schema file alone constitute a generic validator.

Developing the JSON5 reader yourself is the selected direction. The schema engine
and editor integration may be independently implemented or integrate suitably
licensed components, provided they satisfy these contracts. Their implementation
approach is not selected here. Keep them separable so the parser library does not
require VS Code, Node.js, network access or AdaptivePi to build or run.

A serializer, comment-preserving formatter, mutable document editor, streaming
input API, C ABI and schema drafts other than Draft-07 are outside this baseline.
They may be added later without weakening the reader's contracts. There is no
vehicle certification, bounded real-time execution or production qualification
claim in these requirements.

## Authoritative references

The specifications govern their respective languages; the requirement rows
define additional library behavior and verification. This file is not a substitute
for reading the referenced grammar while implementing it.

| Code | Source and relevant scope |
| --- | --- |
| J5 | [JSON5 Data Interchange Format 1.0.0, March 2018](https://spec.json5.org/), especially §§2–10 and §5.1. |
| J5-R | [Official JSON5 reference implementation and feature summary](https://github.com/json5/json5/blob/main/README.md); supporting tests, not authority over the specification. |
| JSON | [RFC 8259, December 2017](https://www.rfc-editor.org/rfc/rfc8259), §§2–9, including parser and Unicode considerations. |
| ECMA-JSON | [ECMA-404, second edition, December 2017](https://ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf), JSON syntax. |
| ES5 | [ECMAScript 5.1, June 2011](https://262.ecma-international.org/5.1/), §§6, 7.6 and 7.8.3–7.8.4, only as referenced by JSON5. |
| UTF8 | [RFC 3629](https://www.rfc-editor.org/rfc/rfc3629), §§3–4, UTF-8 encoding. |
| SC | [JSON Schema Draft-07 Core](https://json-schema.org/draft-07/draft-handrews-json-schema-01), schema identifiers, references and evaluation. |
| SV | [JSON Schema Draft-07 Validation](https://json-schema.org/draft-07/draft-handrews-json-schema-validation-01), assertion and annotation vocabulary. |
| SM | [Official Draft-07 meta-schema](https://json-schema.org/draft-07/schema). |
| PTR | [RFC 6901](https://www.rfc-editor.org/rfc/rfc6901), JSON Pointer. |
| URI | [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986.html), especially §5, URI-reference resolution used by SC. |
| VS | [VS Code language-extension documentation](https://code.visualstudio.com/api/language-extensions/language-server-extension-guide) and [JSON schema integration](https://code.visualstudio.com/docs/languages/json). |
| ST | [JSON Schema Test Suite](https://github.com/json-schema-org/JSON-Schema-Test-Suite), especially `tests/draft7/` and its documented optional tests. |
| AP | AdaptivePi working-tree requirements, inspected on 2026-10-11: `docs/requirements/release-4-manifests.md`, `release-4-verification.md`, `README.md`, root CMake configuration and `docs/adr/0006-exception-build-policy.md`. These identify consumer needs; most Release 4 contracts are still Draft. |
| P | Standalone project design proposal or user-requested capability, not a language-standard mandate. |

JSON5 and JSON do not prescribe a C++ API, CMake layout, allocator or schema
engine. Their acceptance of syntax must be distinguished from a consumer's
schema/policy rejection. The strictness, ownership and error contracts below are
project decisions. Draft-07 is a proposed schema dialect selected for established
editor compatibility; it is not mandated by JSON5.

## Requirement conventions

Every row has **Status: Draft**, **Owning issue: not yet assigned in the new
repository**, and **Unit Tests: empty**. The verification column is a plan, not
passing evidence. Stable IDs are `J5-001` onward; they belong to the standalone
repository and are not additional AdaptivePi release IDs. Dependencies are earlier
IDs; all prerequisites are themselves Draft. `None` means no preceding contract.
Within a source cell, P/AP distinguishes project choices from standard behavior.

Use `shall` for an obligation. Preserve a reviewed requirement's ID when wording
changes and retain its history. Record explicit agreement before promoting a row
to Agreed; implementation and executed verification have separate states. Review
in the section order below. Planned check names can use `J5_<number>_<purpose>`.

## A. Library and build foundations

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-001 — Standalone C++ library | The library shall expose a documented C++17 API, compile without compiler language extensions, and have no dependency on AdaptivePi headers, types, services or repository structure. | Compile an external minimal consumer using only installed/public headers; inspect production dependencies. | None | P; AP build context |
| J5-002 — Build components | CMake 3.25 shall be sufficient to configure separate parser, schema and host-tool targets; tests/editor tooling shall be optional and excluded from a minimal library build. | Configure minimal, schema-enabled and host-tool builds independently, with tests disabled. | J5-001 | P; AP build context |
| J5-003 — Exception-independent API | Recoverable operations shall return explicit success/error data with the same documented outcomes in both exception modes; the standalone build shall default to exceptions disabled and offer an explicit enabled option; ordinary invalid input shall not throw, abort or assert. | Inspect default/explicit options; compile public declarations with `-fno-exceptions`; exercise success and malformed-input results in both build modes. | J5-001 | P; AP MAN-010/014; ADR 0006 |
| J5-004 — Input boundary | Parsing shall accept an explicit byte buffer and length, shall not require a terminating NUL, and shall not mutate caller input. | Parse exact-length and read-only buffers, including embedded zero bytes; compare input before/after and test truncations. | J5-001, J5-003 | P; AP MAN-006/011 |
| J5-005 — Syntax selection | Callers shall explicitly select JSON5 or strict JSON syntax; JSON5 shall be the documented default, and additional nonstandard syntax shall be disabled. | Inspect default options and run the same mode-discriminating fixtures under both explicit modes and the default. | J5-003, J5-004 | P; J5; JSON |
| J5-006 — Unicode data baseline | Releases shall identify the Unicode property-data version and reproducibly generate or vendor its tables; identifier handling shall retain ES5's Unicode 3.0 minimum and document later-version additions. | Check table provenance, regeneration and representative historical/new property memberships without using locale-dependent character classification. | J5-001 | ES5 §7.6; P |
| J5-007 — License and provenance | The new repository shall use a clearly identified permissive license, with MIT proposed; all incorporated source, data and test fixtures shall retain applicable notices and documented versions. | Audit production/editor/test dependency inventories and packaged notices. An upstream license is not silently replaced by the project's license. | None | P; user licensing constraint |
| J5-008 — Public contracts | Public documentation shall specify syntax defaults, value lifetimes, numeric conversions, duplicate handling, error categories, resource limits and thread-safety before claiming a supported release. | Documentation/API review against the corresponding later requirement rows; retain unresolved items as Draft rather than claiming completion. | J5-001, J5-003, J5-005 | P |

## B. Encoding, tokens and complete grammar

Token-level checks may use internal test harnesses; they do not require a public
lexer API. The complete-parser check J5-027 follows the individual grammar
contracts, so acceptance of the whole language is not asserted prematurely.

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-009 — UTF-8 source | Both syntax modes shall validate every input byte, including comments and trailing trivia, against UTF8; malformed sequences shall produce an encoding error without replacement decoding. | Exercise overlong encodings, encoded surrogates, invalid leading/continuation bytes, truncated sequences and values beyond U+10FFFF at different positions. | J5-004, J5-005 | UTF8 §§3–4; P |
| J5-010 — JSON whitespace | Strict JSON mode shall use only the whitespace admitted by JSON/ECMA-JSON. | Test each allowed character and reject extra Unicode spaces outside strings in strict mode. | J5-005, J5-009 | JSON §2; ECMA-JSON |
| J5-011 — JSON5 whitespace | JSON5 mode shall implement J5 §8 with the documented Unicode data. | Test its named spaces, BOM as whitespace, Unicode Zs characters and boundaries adjacent to every token class. | J5-005, J5-006, J5-009 | J5 §8 |
| J5-012 — Comments | JSON5 mode shall implement J5 §7 comment placement/termination; strict JSON shall reject comments. | Include end-of-file line comments, all line endings, unterminated block comments, delimiters inside quoted strings and attempted nesting. | J5-005, J5-009, J5-011 | J5 §7; J5-R |
| J5-013 — Structural tokens | Both modes shall implement their object/array punctuation and separators, including the JSON5-only single trailing comma. | Token/structure fixtures for empty and nested containers; reject holes, doubled commas, missing separators and misplaced colons. | J5-010, J5-011, J5-012 | ECMA-JSON; J5-R |
| J5-014 — Quoted strings | Quoted strings shall follow the selected grammar's delimiter and unescaped-character rules. | Test delimiter collisions, raw CR/LF, raw tab/control bytes, unterminated strings and escaped quotes in each mode; do not apply JSON's control-character restriction indiscriminately to JSON5. | J5-005, J5-009 | JSON §7; J5 §5 |
| J5-015 — JSON escapes | The decoder shall implement JSON's permitted escapes and four-hex-digit Unicode escapes without accepting malformed or truncated forms. | Check decoded content and error locations for every escape form, invalid hex digits and end-of-buffer truncations. | J5-014 | JSON §7; ECMA-JSON |
| J5-016 — JSON5 escapes | JSON5 decoding shall implement J5 §5.1, including its additional escapes and non-escape-character behavior, without C-specific reinterpretations. | Include `\x41`, `\v`, `\0`, `\a`, `\e`, backslash-space, `\01`, `\8`, malformed `\x`/`\u` and exact decoded lengths. In particular `\a`/`\e` must yield letters. | J5-014, J5-015 | J5 §5.1; ES5 §7.8.4 |
| J5-017 — Continuations and separators | JSON5 shall decode line continuations according to J5 §5.1 and expose a warning for raw U+2028/U+2029 in strings; warnings shall not turn valid parsing into failure. | Check LF, CR, CRLF, U+2028/U+2029 continuations for unchanged joined content; test raw separators, warning spans and strict-mode distinctions. | J5-014, J5-016 | J5 §§5.1–5.2; P warning API |
| J5-018 — Surrogate fidelity | String decoding shall preserve escaped UTF-16 units losslessly, including unpaired units admitted by the grammar, and combine valid pairs consistently when converting to Unicode scalars; it shall never silently substitute or drop units. | Compare raw non-BMP characters with paired escapes; preserve lone high/low units and test pair order. Verify representation separately from checked UTF-8 conversion. | J5-009, J5-015, J5-016 | JSON §8.2; ES5 §§6, 7.8.4; P representation |
| J5-019 — Unquoted member names | JSON5 unquoted names shall follow the referenced IdentifierName grammar, including reserved-word names; strict JSON shall require quoted names. | Category-based name fixtures for start/continuation positions, `$`, `_`, non-ASCII letters, combining marks and ZWNJ/ZWJ; reject digit-first names and unrelated punctuation. | J5-005, J5-006, J5-009 | ES5 §7.6; J5-R |
| J5-020 — Escaped identifiers | JSON5 identifier escapes shall be decoded before positional validity and equality checks. | Test valid escaped letters and escaped illegal starts/continuations; compare quoted, bare and escaped spellings of the same decoded name. | J5-015, J5-019 | ES5 §7.6 |
| J5-021 — JSON numbers | Strict JSON shall accept exactly the numeric productions of JSON/ECMA-JSON, without requiring their value to fit a binary floating-point type during tokenization. | Accept integer/fraction/exponent tokens; reject leading plus, leading zeros, missing fraction/exponent digits, hex and non-finite literals. | J5-005, J5-010 | JSON §6; ECMA-JSON; P raw-token behavior |
| J5-022 — JSON5 numbers | JSON5 shall recognize all numeric forms admitted by J5 §6, with standard spelling/case and token boundaries. | Cover signed decimal/hex, leading/trailing decimal points, exponents, NaN and Infinity; reject octal/binary literals, separators, malformed signs, incomplete hex and wrong case. | J5-005, J5-011, J5-021 | J5 §6; J5-R |
| J5-023 — Primitive literals | Both modes shall implement the primitive literal productions of their specification without case folding or treating unknown identifiers as string values. | Test lowercase literals, lookalikes, case variants and an unquoted name in value position. | J5-005, J5-019, J5-021, J5-022 | JSON §3; J5 §§2, 9 |
| J5-024 — Token boundaries | Lexing shall enforce complete token boundaries and shall not obtain a valid token by silently discarding a suffix or splitting an invalid spelling. | Test `truex`, `1foo`, `0x1z`, incomplete escapes, and comments/spaces inserted inside rather than between tokens. | J5-012, J5-015, J5-020, J5-021, J5-022, J5-023 | ES5 lexical context; P strictness |
| J5-025 — BOM policy | JSON5 shall treat a UTF-8 BOM according to its whitespace grammar; strict JSON shall reject a leading BOM by default and permit ignoring exactly one leading BOM through a documented option. | Compare leading and interior BOMs under both modes/options; distinguish actual BOM bytes from the same character inside a string. | J5-005, J5-009, J5-010, J5-011 | J5 §8; JSON §8.1; P default |
| J5-026 — Complete input | Parsing shall consume exactly one root value, of any admitted type, followed only by permitted trivia; empty input and additional values/tokens shall fail. | Root primitives/containers, whitespace-only and comment-only input, trailing comments, `{} {}` and junk after a valid prefix. | J5-012, J5-013, J5-014, J5-021, J5-022, J5-023, J5-024, J5-025 | JSON §§2–3; J5 §§2, 9; P strict consumption |
| J5-027 — Language completeness | The finished reader shall implement the entire J5 grammar in JSON5 mode and the entire JSON/ECMA-JSON grammar in strict mode within documented resource limits, with a production-to-test coverage matrix. | Audit every grammar production against lexer/parser tests, including Unicode, malformed/truncated inputs and cross-mode cases; unresolved grammar gaps block a full-support claim. | J5-009–026 | J5 §§9–10; JSON §9; ECMA-JSON |

## C. Values, ownership and numeric operations

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-028 — Value types | Parsed values shall distinguish null, boolean, string, number, array and object without coercion; numeric subcategories shall not lose their original spelling or exact value. | Inspect each type, reject boolean-to-number conversion, and distinguish absent members from explicit null. | J5-003, J5-027 | P; AP MAN-001/003/007 |
| J5-029 — Length-aware strings | String/name APIs shall expose length, preserve empty content/embedded NUL internally and provide checked UTF-8 export that fails on unpaired surrogate units rather than substituting text; no parsed content shall depend on C-string termination. | Empty/NUL strings and names, raw JSON5 controls and suffixes after NUL; compare units and exported bytes, including valid pairs and failed unpaired-unit conversion. | J5-003, J5-004, J5-018, J5-028 | P; AP MAN-006 |
| J5-030 — Ordered arrays | Array iteration and indexing shall preserve source element order, including empty arrays and repeated values. | Inspect nested arrays and arguments containing empty strings, whitespace, quotes and metacharacters. | J5-028, J5-029 | P; AP MAN-003/006/015 |
| J5-031 — Observable object members | The owned object representation shall preserve every source member and expose source order, including repeated names, before any consumer policy is applied. | Enumerate duplicate and nonduplicate objects; demonstrate that convenience lookup cannot silently erase or conceal duplicates. | J5-028, J5-029 | P; AP MAN-001 |
| J5-032 — Duplicate policy | Callers shall select preserve-all or reject-duplicates per object; preserve-all shall be the generic default and reject-duplicates shall report both occurrences before returning any successful document. | Bare/quoted/escaped collisions at root and nested levels; equal names in separate objects; compare decoded units rather than hash alone. | J5-020, J5-031 | P; AP MAN-001 |
| J5-033 — Name equality | Key equality shall use exact decoded content without normalization or case folding; any hashing shall confirm equality rather than trusting a hash match. | Compare equivalent escapes, different case, canonically equivalent but differently encoded sequences, and injected hash collisions where applicable. | J5-020, J5-029, J5-031 | ES5 §7.6; P |
| J5-034 — Exact number storage | Numbers shall retain owned original token text and enough information for exact finite-value checks; eager conversion to double shall not determine acceptance or bounded integer results. | Preserve spellings around 2^53, signed/unsigned 64-bit limits, very long integers/fractions, huge exponents and overflowing hex tokens. | J5-021, J5-022, J5-028 | P; AP MAN-001/007 |
| J5-035 — Checked integer conversion | Signed/unsigned integer conversion shall return explicit wrong-type, non-integral or out-of-range outcomes without rounding, wrapping or accepting booleans/non-finite values. | Destination min/max and adjacent values; `0x10000000000000001`; exact fractional/exponent values close to bounds; zero and negative unsigned inputs. | J5-003, J5-028, J5-034 | P; AP MAN-001/007 |
| J5-036 — Value and spelling queries | The API shall distinguish mathematical integrality from lexical form, exposing exact-integral checking and original spelling independently. | Compare `1`, `1.0`, `1e0`, `0x1`, `+1`, `1e-1`, extremely close fractions and huge exponents without converting through floating point. | J5-034, J5-035 | P; AP open integer-policy finding |
| J5-037 — Floating conversion | Floating-point conversion shall be explicit and locale-independent, with documented rounding, overflow and underflow outcomes; it shall not alter the stored token or exact representation. | Representative rounding boundaries, overflow, subnormal/underflow values and repeated conversions under different locales. | J5-003, J5-034, J5-036 | P |
| J5-038 — Special numbers | JSON5 values shall distinguish explicit non-finite tokens from finite tokens that overflow a requested conversion, and retain the sign/spelling of negative zero. | Inspect `-0`, `-0.0`, signed hex zero, signed Infinity/NaN and a large finite exponent; verify that strict mode still rejects non-finite tokens. | J5-022, J5-034, J5-037 | P; J5 numeric context |
| J5-039 — Checked access | Type conversion, lookup and indexing APIs shall document and explicitly report wrong type, absent member, ambiguous preserved duplicate and out-of-range index without ordinary-input termination. | Exercise invalid accesses through the supported API in both exception modes, including preserved duplicate-name lookup. | J5-003, J5-028, J5-030, J5-031, J5-032 | P |
| J5-040 — Document lifetime | A successful document shall own all value/name/number data independently of caller input; moves/destruction and any views shall have documented lifetimes with no references to temporary parse buffers. | Destroy/overwrite input, move the document, then inspect contents; verify documented view invalidation and leak-free destruction. | J5-028–039 | P; AP MAN-011 |
| J5-041 — Source spans | Parsed values, member names and relevant tokens shall retain source byte ranges for diagnostics and editor mapping; source text retained for tooling shall have explicit ownership and a configurable cost. | Verify spans around comments, escapes, repeated members, CRLF and multibyte characters, including moved documents and input destruction. | J5-027, J5-040 | P |
| J5-042 — Pure memory parsing | The core parse/value API shall perform no filesystem, process-control, environment expansion or network operations. | Use in-memory fixtures without filesystem/controller dependencies; inspect production imports and instrument external-operation boundaries. | J5-004, J5-040 | P; AP MAN-012/014 |

## D. Failures, resources and concurrency

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-043 — Structured errors | Public errors shall distinguish syntax, encoding, duplicate-policy violation, wrong type/access, numeric conversion, configured resource limit and allocation failure; schema/tool layers shall add their own categories without disguising operational failures as invalid input. | Compare stable error codes for representative failures and ensure callers need not parse human-readable text. | J5-003, J5-032, J5-035, J5-039 | P; AP MAN-010/014 |
| J5-044 — Diagnostic locations | Input diagnostics shall carry an original zero-based byte offset plus a documented line/column convention and relevant source range; end-of-input failures shall use a valid end offset. | Exact locations for LF/CR/CRLF and Unicode separators, multibyte characters, invalid UTF-8, both duplicate occurrences and truncated input. | J5-009, J5-041, J5-043 | P |
| J5-045 — Failure-safe reporting | An allocation/resource failure shall be reportable without a further successful allocation; a minimal stable code/location shall remain available if rich text or path construction cannot be completed. | Exhaust the allocator while constructing the document and error details; verify a valid minimal result without a second failure or termination. | J5-003, J5-043, J5-044 | P; AP resource-policy need |
| J5-046 — Fallible allocation | All library-controlled fallible allocation, including owned results and schema evaluation, shall use a recoverable mechanism in both exception modes; unsupported throwing/abort-on-OOM dependency paths shall not be advertised as recoverable. | Inject failures at each allocation point, audit containers/dependencies and show how result storage and allocator lifetimes are handled without `bad_alloc` in the default build. | J5-003, J5-040, J5-045 | P; AP ADR 0006 |
| J5-047 — Configurable limits | Options shall bound input bytes, nesting, node/member counts, decoded string units, numeric token/exponent lengths, owned allocation bytes and diagnostic count; defaults and accounting rules shall be documented and validated before processing. | Each limit at below/equal/above boundary, invalid option combinations and checked arithmetic overflow; do not silently truncate input or values. | J5-008, J5-034, J5-043, J5-046 | P; J5/JSON permit implementation limits |
| J5-048 — Bounded traversal | Parsing, conversion, comparison, cleanup and later schema traversal shall check configured budgets before exhausting stack or work storage, including error cleanup of deeply nested partially built data. | Deep valid/invalid structures at the depth boundary, failed construction/destruction, huge exponents and adversarial duplicate sets; inspect recursion and size arithmetic. | J5-035, J5-040, J5-046, J5-047 | P |
| J5-049 — Work budgets | Long-running validation/conversion and tooling requests shall support documented cancellation or deterministic work budgets; budget exhaustion shall be distinguishable from syntax/schema rejection. | Cancel at multiple work boundaries and limit pathological comparisons/reference chains/regex work; verify bounded cleanup and stable error results. | J5-043, J5-047, J5-048 | P; editor responsiveness |
| J5-050 — Concurrency | Separate parser contexts/documents shall be usable concurrently without shared mutable global state; immutable compiled schemas shall document safe concurrent validation with separate work contexts. | Concurrent independent success/failure cases, repeated runs and race checks; sharing mutable contexts need not be supported. | J5-040, J5-046 | P |
| J5-051 — Atomic results and reuse | A failed operation shall publish no successful partial document/configuration, release temporary resources and leave reusable contexts in a documented valid state. | Alternate valid/malformed/resource-failing inputs in one context; track allocations and inspect success/error alternatives. | J5-003, J5-040, J5-043, J5-046, J5-048 | P; AP MAN-014 |
| J5-052 — Deterministic semantics | Parsing and validation shall not depend on process locale, time, inherited environment or hidden global options; the same input/options/version shall produce equivalent values and stable primary error categories. | Repeat under changed locales/environment and concurrent contexts; verify that numeric parsing still uses the grammar's decimal separator. | J5-006, J5-037, J5-042, J5-050 | P |

## E. Generic JSON Schema validation

This is a separate component and a significant deliverable. Its build is optional
for parser-only users; these rows are required before claiming the complete
parser-plus-schema product. Using a third-party schema engine is permitted, but
does not waive exception, ownership, precision, resource or license obligations.

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-053 — Schema dialect | The schema component shall implement Draft-07 required validation/reference behavior, expose its dialect explicitly and reject unsupported dialect requests rather than interpreting another draft as Draft-07. | Full required-keyword coverage review, dialect-selection cases and the official Draft-07 suite; distinguish optional capabilities. | J5-027, J5-028, J5-043 | SC; SV; SM; P dialect choice |
| J5-054 — Schema compilation | Schema compilation shall accept strict-JSON boolean/object schemas, check them against the Draft-07 meta-schema and return distinct schema-compilation errors for invalid keyword shapes/patterns. | True/false schemas, invalid roots, wrong keyword types, malformed patterns and the locally available meta-schema; no live download. | J5-005, J5-040, J5-046, J5-053 | SC; SM; P |
| J5-055 — Identifiers and references | Schema compilation shall resolve Draft-07 identifiers, URI scopes and references using an explicit caller-provided registry/resolver, including local fragments, JSON Pointers and registered external schemas. | Nested `$id`, relative/absolute references, pointer escapes, fragments and unresolved references; registry fixtures replace network retrieval. | J5-043, J5-054 | SC §8; PTR; URI §5; P resolver boundary |
| J5-056 — Reference semantics | Evaluation shall follow Draft-07 `$ref` sibling semantics and support recursive schemas that make progress through the instance, while detecting nonprogressing cycles/budget exhaustion without hanging. | Ignored assertion siblings, recursive lists/trees, reference-only cycles and reused instance/schema pairs; confirm cycle/resource errors are not fabricated invalid-instance results. | J5-048, J5-049, J5-055 | SC §8.3; P cycle reporting |
| J5-057 — Assertion coverage | Every required assertion listed in the keyword matrix below shall implement SV's semantics, including nested application, interactions and nonapplicable instance types. | Dedicated positive/negative/boundary checks per keyword and interaction, plus required Draft-07 suite tests; silently skipped assertions fail acceptance. | J5-028, J5-053, J5-054, J5-056 | SV §§6, 9 |
| J5-058 — Annotations and defaults | Standard annotations shall remain available to tooling without changing instance values; `default` and `examples` shall not insert missing fields or convert invalid data during validation. | Validate before/after instance equality; inspect metadata through valid references/branches and ensure annotations from inapplicable branches do not become misleading active defaults. | J5-053, J5-056, J5-057 | SC annotation context; SV §§8, 10; P tooling API |
| J5-059 — Exact numeric validation | Numeric schema assertions and equality shall use exact finite mathematical values, with integer determined by value rather than spelling; explicit NaN/Infinity shall be rejected as non-JSON schema instances before assertions. | `1`/`1.0`/`1e0`/`0x1`, `1e-1`, huge integers, close fractions, exact decimal `multipleOf`, negative zero and non-finite JSON5 values. | J5-034–038, J5-053, J5-057 | SV numeric semantics; P JSON5-instance boundary |
| J5-060 — String and pattern semantics | Schema string length shall count Unicode characters rather than UTF-8 bytes; pattern/property-name matching shall use a documented Draft-07-compatible regex dialect and explicitly report invalid/unsupported patterns. | Astral/combining characters, decoded escapes and NUL; anchored/unanchored patterns, invalid regex and required interoperable regex fixtures. Reject unsupported Unicode instances explicitly, without replacement decoding. | J5-018, J5-029, J5-043, J5-049, J5-057 | SV string/regex semantics; P implementation contract |
| J5-061 — Structural equality | `const`, `enum` and `uniqueItems` shall use type-aware deep equality with exact numeric equivalence, order-sensitive arrays and order-independent uniquely named objects. | Reordered objects, reordered arrays, nested values, booleans versus numbers and equivalent numeric spellings; reject duplicate-member instances at the schema boundary. | J5-028, J5-030–036, J5-057, J5-059 | SV equality context; P duplicate-instance policy |
| J5-062 — Extension keywords | Unrecognized schema keywords shall follow Draft-07 extension handling; they shall not be confused with unknown instance properties. Tooling may warn about likely misspellings without inventing a validation assertion. | Unknown annotation/custom keywords, misspelled known keywords and `additionalProperties:false` in the same schema; verify their different effects. | J5-053, J5-054, J5-057, J5-058 | SC keyword context; P tooling warning |
| J5-063 — Validation outcomes | The API shall distinguish valid instance, schema assertion failure, invalid schema, unresolved reference, unsupported instance/dialect and operational failure, with a documented fail-fast or bounded collect-all option. | Separate each outcome, diagnostic-count boundaries and an assertion failure followed by allocation/cancellation failure; no ambiguous bool-only result. | J5-043, J5-045, J5-049, J5-054–062 | P |
| J5-064 — Schema diagnostics | Assertion diagnostics shall identify the instance pointer, schema location/reference URI and failed keyword, with original source spans when available; pointer escaping shall follow PTR. | Nested arrays and property names containing `/`, `~`, empty names and Unicode; externally referenced schema failures and source-byte mapping. | J5-041, J5-044, J5-055, J5-063 | PTR; P |
| J5-065 — Evaluation budgets | Schema work shall account for references, branches, deep equality, regex and collected errors against explicit evaluation budgets; pathological input/schema pairs shall return operational errors and release work storage. | Branch-heavy schemas, repeated references, expensive patterns, deep `uniqueItems`, cancellation and failure injection; record which regex mechanism enforces its budget. | J5-046–049, J5-056, J5-060, J5-061, J5-063 | P |
| J5-066 — Compiled-schema lifetime | Compiled schemas shall own their data and resolved reference graph independently of temporary schema input/registry buffers, and support repeated validation with separate bounded work contexts. | Destroy input/registry temporaries, move/reuse the compiled schema and run concurrent validations; document any explicitly retained resolver lifetime. | J5-040, J5-046, J5-050, J5-055, J5-065 | P |
| J5-067 — Pure evaluation | Validating a compiled schema against a parsed document shall perform no network, filesystem, application launch or environment-expansion operations; any external schema loading shall occur through the explicit compilation/resolution boundary. | Validate using instrumentation/fakes proving no external calls, including reference-bearing schemas and failure paths. | J5-042, J5-055, J5-066 | P; AP MAN-012 |
| J5-068 — Component independence | Parser-only builds shall omit schema-engine dependencies; schema-enabled builds shall identify all transitive dependencies and preserve the parser's supported compiler/exception/resource contracts. | Minimal and schema-enabled builds, dependency/license audit, header consumer checks and allocation-error verification of any integrated engine. | J5-001–003, J5-007, J5-046, J5-053 | P |
| J5-069 — Optional assertions | Baseline `format`, `contentEncoding` and `contentMediaType` shall be annotations; requested assertion modes shall either implement a documented supported set with checks or return unsupported-option errors. | Confirm annotations do not fail normal validation; test capability reporting and every optional assertion advertised, including disabled mode. | J5-053, J5-058, J5-063, J5-068 | SV §§7–8; P baseline option |

### Draft-07 keyword coverage matrix

This table specifies the coverage obligation in J5-053/057/058/069. Refer to SC/SV
for semantics; do not implement a keyword by its name alone. The meta-schema's
URI and its local copy must be recorded with the release.

| Area | Required coverage |
| --- | --- |
| Core/reference | Boolean and object schemas; `$schema`, `$id`, `$ref`; local/registered references and `definitions`. |
| Types and values | `type` including type arrays and value-based `integer`; `enum`, `const`. |
| Numbers | `multipleOf`, `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum`. |
| Strings | `minLength`, `maxLength`, `pattern`. |
| Arrays | Schema/tuple `items`, `additionalItems`, `minItems`, `maxItems`, `uniqueItems`, `contains`. |
| Objects | `properties`, `patternProperties`, `additionalProperties`, `required`, `propertyNames`, `minProperties`, `maxProperties`, property/schema `dependencies`. |
| Composition/conditions | `allOf`, `anyOf`, `oneOf`, `not`, `if`, `then`, `else`. |
| Metadata | `title`, `description`, `default`, `examples`, `readOnly`, `writeOnly`, `$comment`; retain standard metadata for tooling without mutating instances. |
| Optional assertion vocabulary | `format`, `contentEncoding`, `contentMediaType`: annotation handling required; assertion capability follows J5-069. |

Unknown keywords remain governed by Draft-07 extension rules. Keywords from later
drafts, such as `unevaluatedProperties`, are not silently implemented with a
different dialect's semantics. JSON5 non-finite numbers have no interoperable JSON
Schema value; rejecting them here does not make their syntax invalid to the parser.
Likewise, losslessly parsed unpaired surrogate units may be explicitly outside the
schema component's documented Unicode instance domain and must never be silently
replaced to obtain a passing result.

## F. Host tools and VS Code

Host tooling is developed and packaged separately from the ARM64 library. An
existing extension may be adapted, but syntax highlighting alone does not satisfy
these requirements. A custom language server is an implementation option, not a
requirement to introduce another runtime into the ECU image.

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-070 — Validation CLI | A host CLI shall validate file/stdin input using an explicit syntax mode and optional schema/profile/limits; exit 0 shall mean valid, 1 invalid input/assertion/profile, and 2 configuration/I/O/operational failure. | Exercise valid, malformed, invalid-schema, unreadable-file, unsupported-option, resource-limit and cancelled requests; test bounded stdin reading. | J5-043, J5-047, J5-051, J5-063, J5-067 | P |
| J5-071 — Machine diagnostics | Host tools shall provide a versioned strict-JSON diagnostic/capability format as well as readable messages; arbitrary input content shall be escaped safely and no caller shall need to scrape prose. | Parse machine output with strict JSON; test Unicode/control characters, schema pointers, warning severity, version/capability fields and minimal OOM reporting. | J5-044, J5-045, J5-064, J5-070 | P |
| J5-072 — Editor packaging | VS Code support shall ship as a reproducibly packaged extension or a documented compatible extension setup, with an explicit language ID, supported VS Code versions and Remote SSH installation instructions. | Install the package/setup in a clean local and Remote SSH workspace; open `.json5` documents without changing unrelated language associations. | J5-002, J5-007, J5-070 | P; VS; AP development workflow |
| J5-073 — External schema association | Editor support shall associate workspace file patterns with repository-local schemas without requiring `$schema` inside instance documents; multiple workspace roots shall resolve their own relative associations. | Open two workspaces with different schemas, test local references and verify no schema metadata is inserted into a manifest. | J5-055, J5-072 | P; VS; AP MAN-001 |
| J5-074 — Grammar agreement | Editor tokenization/highlighting and complete-document syntax diagnostics shall support the full selected grammar, including its Unicode names/whitespace, escapes, continuations and numeric forms, without treating valid syntax as invalid. | Reuse the grammar fixture corpus in editor acceptance checks; include `\x`, `\v`, `\0`, non-escape letters, raw control characters and mode-specific behavior. | J5-027, J5-071, J5-072 | P; J5/JSON syntax baseline |
| J5-075 — Location mapping | Editor ranges shall correctly map original byte offsets to the editor protocol's documented character-position encoding, including UTF-16 positions where used. | Place errors before/after astral characters, combining marks, CRLF and continued strings; verify selected ranges and end-of-buffer locations. | J5-041, J5-044, J5-071, J5-072 | P; VS |
| J5-076 — Live document versions | Diagnostics/completions shall reflect the current unsaved document version; cancelled or outdated requests shall not replace results for newer text. | Rapid edits, file reopen, schema changes, cancellation and out-of-order backend responses; no stale result application. | J5-049, J5-072, J5-075 | P; VS |
| J5-077 — Incomplete-input assistance | Completion and hover shall remain usable while input is incomplete or malformed; any recovered editor tree shall be marked as provisional and shall never be a successful runtime parse/validation result. | Request suggestions inside incomplete objects, arrays, strings and escapes; assert runtime parsing rejects the same unfinished document. | J5-026, J5-039, J5-074, J5-076 | P |
| J5-078 — Schema completion | Completion shall suggest applicable field names and schema-provided enum/const/example/default values, identify required fields and avoid ordinary duplicate-property insertion while respecting document quoting/style. | Root/nested and referenced schemas, existing properties, type unions, conditional branches and incomplete text; inserted suggestions must be syntactically valid in their context. | J5-055, J5-057, J5-058, J5-073, J5-077 | P; VS |
| J5-079 — Hover and schema diagnostics | Hover shall show relevant schema descriptions/types/constraints; editor schema diagnostics shall agree with the runtime validator's mathematical-number and assertion semantics for supported instances. | Hover fields and enum values; compare `1`, `1.0`, `1e0`, `1e-1`, large integers, unknown fields and conditional failures against the native validator. | J5-059–064, J5-073–078 | P; VS |
| J5-080 — Consumer-policy diagnostics | Editor profile settings shall expose duplicate, Unicode-instance, root-shape and numeric-spelling restrictions separately from generic JSON5 syntax; prohibited manifest duplicates shall be errors rather than merely ignored or overwritten. | Generic versus manifest profiles on the same input; decoded-key duplicates, non-finite numbers and schema-valid values rejected only by an explicit lexical policy. | J5-032, J5-036, J5-063, J5-074, J5-079 | P; AP MAN-001/006/007 |
| J5-081 — Offline operation | Local-schema completion/validation shall work without network access; any optional remote loading shall require an explicit user configuration and use the same reference identity rules as runtime schema resolution. | Block network access, use local/registered schemas and relative references, and confirm no background fetch is needed to activate the language. | J5-055, J5-067, J5-073, J5-079 | P |
| J5-082 — Editor acceptance | A release claiming VS Code support shall have executed editor tests for highlighting, live diagnostics, completion, hover, position mapping and runtime parity on the supported host/Remote SSH configuration. | Record extension/VS Code/backend versions and actual acceptance results; screenshots or README claims alone do not establish grammar/validator parity. | J5-072–081 | P |

## G. AdaptivePi consumer profile and integration

These requirements describe compatibility with the current consumer proposal,
not a mandate to embed application-specific rules in the generic library. The
manifest contract must be rechecked against the finally agreed AdaptivePi text
before integration. The library should remain useful to other consumers.

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-083 — Manifest profile boundary | An AdaptivePi example profile shall select JSON5, object root and duplicate rejection, then apply the external schema/value rules; its generic syntax parser shall remain independent of application fields and filesystem layout. | Apply the profile to valid JSON5 primitives and duplicate objects: generic parsing succeeds where allowed, while the profile reports its separate rejection. | J5-026, J5-032, J5-042, J5-063, J5-080 | P; AP MAN-001/012 |
| J5-084 — Versioned schema artifact | The example shall provide a strict-JSON Draft-07 schema for manifest version 1 with descriptions/examples and a documented local editor association; release notes shall identify the AdaptivePi contract revision it models. | Validate the schema with the local meta-schema; compile/use it and check a valid complete manifest with editor completion. | J5-054, J5-058, J5-073, J5-078, J5-083 | P; AP MAN-001/003/009 |
| J5-085 — Field and policy coverage | That schema/profile shall cover every current field/type/value rule in the manifest contract snapshot below, without adding parser-specific metadata as an application field. | Reuse one valid example and individually invalid fixtures for each missing field, wrong type, null, unknown field, pattern, numeric bound and conditional policy. | J5-057, J5-059–061, J5-084 | AP MAN-001/003/004/006/007/008/009 |
| J5-086 — Unknown versus dynamic names | Example schema objects with fixed fields shall reject additional properties, while `environment` shall permit dynamic names matching its name rule and enforce string values. | Unknown root/restart members fail; valid arbitrary environment names succeed; invalid names/types fail after decoding. | J5-020, J5-057, J5-085 | P clarification proposal; AP MAN-001/003/006 |
| J5-087 — Numeric-policy configuration | The profile shall document mathematical-integral versus lexical-integer validation and allow an explicit spelling policy to be applied without rounding; its final default shall follow AdaptivePi's agreed wording. | Configure both policies on integral exponent/fraction/hex/plus spellings; booleans, true fractions and non-finite values always fail manifest integer fields. | J5-035, J5-036, J5-059, J5-085 | P; AP MAN-001/007 unresolved spelling |
| J5-088 — String and path boundaries | The profile shall reject manifest NUL and invalid Unicode-to-UTF-8 conversion without changing otherwise valid string bytes; path syntax checks shall not claim filesystem existence, permissions or symlink containment. | Empty/special arguments, escaped/raw NUL, unpaired surrogate units and lexical traversal fixtures; document the separate filesystem checks required in AdaptivePi. | J5-018, J5-029, J5-040, J5-060, J5-067, J5-085 | AP MAN-005/006/013; P Unicode policy |
| J5-089 — Defaults and arguments | Integration guidance shall preserve ordered configured arguments including empty strings, leave derived `argv[0]` to AdaptivePi, and distinguish runtime optional-field defaults from schema annotations. | Map a fixture with empty arguments/environment and omitted service-unit field to an owned consumer model; verify that parser/schema data is not silently mutated. | J5-029, J5-030, J5-040, J5-058, J5-085 | AP MAN-003/008/011/015 |
| J5-090 — Result/error mapping | Integration guidance shall show how parser/schema/profile outcomes map to project-owned `Result`/error categories without requiring `ara::core` in the library; unsupported version shall remain distinguishable from a generic field error. | Standalone fake consumer mapping tests for malformed/encoding/duplicate, version, field, conversion and resource failures; reconcile exact categories with agreed MAN-010/014 before integration. | J5-003, J5-043, J5-051, J5-063, J5-083–089 | AP MAN-010/011/014; P adapter boundary |
| J5-091 — Existing ARM64 platform | The production parser and selected schema component shall cross-build/link with AdaptivePi's existing Yocto ARM64 SDK and runtime, without requiring Rust, Node.js, VS Code or a host compiler on the target. | Cross-build a standalone consumer, inspect its AArch64 ELF/dependencies and run target fixtures using the existing deployment path; separately document any genuinely new runtime dependency. | J5-001–003, J5-068, J5-090 | P; AP SDK/platform constraints |
| J5-092 — Reusable package | Both parser-only and schema-enabled packages shall support namespaced exported CMake targets, `add_subdirectory` and installed `find_package` use; integration shall not alter parent compiler flags, enable tests unexpectedly or download dependencies implicitly. | Clean source-tree and installed-package consumers with offline vendored dependencies, including a cross-build; inspect transitive include/link requirements. | J5-001, J5-002, J5-007, J5-068, J5-091 | P; AP CMake integration |
| J5-093 — Pinned integration | Release artifacts shall identify immutable source/dependency versions and compatible build options; AdaptivePi integration shall use a reviewed release/commit and document any ABI-impacting option. | Reproduce a packaged consumer from the recorded revision/options; verify consistent exception/allocator definitions across library and consumer. | J5-007, J5-008, J5-091, J5-092 | P |

### Current manifest contract snapshot to support

This is a consumer-compatibility checklist, **not newly agreed AdaptivePi
wording**. It makes the file useful when copied to the new repository. Reconcile
it with the current consumer file before publishing the example schema.

| Area | Current requirement-derived contract | Where enforced |
| --- | --- | --- |
| Root/version | Object root; required `schema_version` with integer value 1. Numeric spelling is unresolved. | Profile plus schema; a spelling rule needs an additional token check. |
| Fixed root members | Required `schema_version`, `application_id`, `executable`, `arguments`, `working_directory`, `restart_policy`; optional `environment`, `service_unit`. | Required/types/additional-property schema rules. |
| Identity | String matching `[A-Za-z][A-Za-z0-9_]*`; schema regex must be anchored to check the whole value. | Schema. |
| Arguments | Ordered array of strings; empty array and empty strings valid; embedded NUL invalid; excludes executable argument zero. | Schema/value checks; launch-vector assembly in AdaptivePi. |
| Environment | Optional object of string values, defaulting to empty at the consumer; names match `[A-Za-z_][A-Za-z0-9_]*`; values can be empty but cannot contain NUL. | Schema `propertyNames`/value rules plus consumer default. |
| Restart | Object containing exactly `mode` and `max_attempts`; mode `never` or `on_failure`; count integral in 0–100 and never a boolean; `never` requires 0. | Schema types/ranges/conditional rule plus any agreed spelling policy. |
| Service unit | Optional string matching `[A-Za-z][A-Za-z0-9_-]*\.service`; omitted value becomes `adaptive-pi-<application_id>.service`; no NUL. | Anchored schema pattern and consumer-derived default. |
| Executable/work paths | Nonempty relative strings under `bin/` and `work/`, no NUL or `.`/`..` components. Exact allowed path spellings remain consumer-owned. | Schema/profile for lexical rules. |
| Filesystem | Canonical/symlink-resolved paths stay within package root; executable is an executable regular file; work path is an existing directory. | AdaptivePi package validation, not the generic parser or editor schema. |
| Duplicate keys | Reject within each object after decoding; dynamic environment names are also checked. | Parser/profile; JSON Schema alone cannot recover duplicate members. |
| Unknown members | Fixed schema objects reject unknown fields; environment uses dynamic-name rules. This interpretation requires MAN-001 review. | Schema/profile; clarification remains proposed. |
| Ownership/errors | Complete owned result, no temporary-buffer references, explicit errors and no published partial configuration. | Library contracts and consumer adapter. |

The current AdaptivePi requirements specify JSON5 1.0.0 manifests named
`manifest.json5`. MAN-001–015 remain In Review; the format and filename edits
do not establish agreement to the full manifest contracts.
The generic library does not supervise services, install packages or implement
filesystem security. Final C++ consumer-model allocation also needs an agreed
failure strategy; the parser's fallible allocator does not make `std::string` or
other unrelated consumer allocations recoverable automatically.

## H. Verification and release acceptance

| ID / title | Requirement | Planned verification | Dependencies | Basis |
| --- | --- | --- | --- | --- |
| J5-094 — Grammar regression coverage | Tests shall trace to grammar requirements and cover valid, invalid and truncated cases for each production and syntax mode, asserting decoded values and locations rather than acceptance alone. | Production coverage matrix, truncation at every byte of representative tokens, mode-pair cases and the minimum regression checklist below. | J5-009–044 | P |
| J5-095 — Independent syntax evidence | Releases shall run pinned external syntax/reference corpora and differential checks, documenting duplicate/precision/Unicode/limit differences rather than adopting another implementation's result as the standard. | Use [official JSON5 tests](https://github.com/json5/json5/tree/main/test) and [JSONTestSuite](https://github.com/nst/JSONTestSuite); classify optional `i_` cases and compare exact values only where reference semantics are compatible. | J5-007, J5-027, J5-032–038, J5-094 | J5-R; JSONTestSuite is supporting community evidence; P |
| J5-096 — Schema conformance evidence | The schema component shall pass pinned required Draft-07 Test Suite cases, with registered remote fixtures available offline; optional tests shall be reported according to actual advertised capabilities. | Record suite revision, test selection, counts and failures; required assertion/reference tests cannot be silently skipped or relabelled optional. Add numeric/Unicode/parity regressions absent from the suite. | J5-053–069, J5-094 | ST; P |
| J5-097 — Robustness evidence | Parser/schema tests shall include sanitizer-enabled fuzzing and metamorphic cases, preserving minimized regressions; no supported malformed-input case shall cause a crash, invalid memory access or uncontrolled loop. | Fuzz both modes, duplicate policies, limits, schemas/references and Unicode boundaries; transform legal whitespace/comments/escapes without changing expected values. Record tools, seeds, durations and findings. | J5-027, J5-046–052, J5-065, J5-094, J5-096 | P |
| J5-098 — Fault-injection evidence | A release shall demonstrate allocation/limit/cancellation failure propagation and cleanup for parser, conversion and schema operations in both supported exception configurations. | Fail each reachable allocation in representative success/failure paths, account for retained results and verify zero leaked temporary blocks; exercise the minimal diagnostic fallback. | J5-003, J5-045–051, J5-063, J5-065, J5-097 | P; AP resource expectations |
| J5-099 — Supported build matrix | Release evidence shall cover native GCC/Clang C++17, both exception modes, installed consumers, ARM64 SDK cross-build and representative target execution; unsupported combinations shall be identified. | The build matrix below records actual commands/toolchain versions and results; native exception-enabled execution may be confined to CI to match AdaptivePi practice. | J5-003, J5-068, J5-082, J5-091–098 | P; AP build/deployment context |
| J5-100 — Release completeness | A full-support release shall document its API, standards/Unicode versions, completed requirement/grammar/keyword matrices, limits, licenses, build/editor instructions and known deviations, backed by executed evidence. | Audit requirements versus shipped capabilities and test artifacts; record measured memory/build footprint and representative throughput without inventing a real-time or universal performance guarantee. | J5-008, J5-027, J5-053, J5-082, J5-093–099 | P |

### Minimum regression checklist

These are test intentions, not implemented tests. Binary fixtures are needed for
raw UTF-8 errors/control bytes; Markdown spellings alone are insufficient.

| Case | Required observation |
| --- | --- |
| JSON5 `{key: 'value',}` and JSON equivalent | Expected owned values; the JSON5-only form fails strict mode. |
| Primitive/array roots and empty/comment-only input | Generic roots supported; no value is a syntax failure; manifest profile rejects non-object roots separately. |
| Comment delimiters inside strings and comments next to tokens | Correct token boundaries; string content is not stripped as a comment. |
| Bare `true`/`null` as keys, escaped Unicode names and invalid digit-first names | IdentifierName behavior and correct decoded names, not arbitrary bare-key acceptance. |
| Non-ASCII IdentifierName categories and Unicode Zs spaces | Grammar support agrees with the documented data version; no ASCII-only claim disguised as full support. |
| `\x41`, `\v`, `\0`, `\a`, `\e`, backslash-space and escaped single quote | Correct decoded units/length; invalid decimal-digit escape forms fail; manifest NUL policy is a separate rejection. |
| Raw tab/NUL and raw CR/LF in strings | Respect the selected grammar and explicit buffer length; do not import a blanket JSON control-byte rule into JSON5. |
| All five line-continuation forms and raw U+2028/U+2029 | Joined content excludes the continuation; raw separators preserve content with the prescribed JSON5 warning. |
| Paired and unpaired escaped surrogate units | Generic parse preserves exact units; checked UTF-8/schema/profile operations report their own unsupported-conversion/domain outcome without substitution. |
| `{a:1, "a":2, "\u0061":3}` | Three members retained generically; decoded duplicate policy reports the colliding occurrences. |
| Equal-looking normalized names and deliberately colliding hashes | Exact decoded equality, no normalization or hash-only comparison. |
| `1`, `1.0`, `1e0`, `0x1`, `+1`, `1e-1`, `1.0000000000000001` | Independent mathematical-integral and spelling queries; schema integer/value semantics correct; no double-rounding acceptance. |
| `9007199254740993`, signed/unsigned bounds and `0x10000000000000001` | No rounding/wrapping; raw token survives; checked destination overflow reported. |
| Huge finite exponent versus explicit Infinity/NaN; negative zero | Grammar acceptance does not imply conversion success; non-finite schema-instance rejection is distinct. |
| Very deep/wide structures, large strings/numbers and cancellation | Named budget/limit failures and bounded, leak-free cleanup. |
| Allocation failure during parse, compiled schema, validation and diagnostic construction | Recoverable error, no published success/partial candidate, no abort-dependent path. |
| Default/unknown instance fields versus unknown schema keywords | Validation does not insert defaults; instance rejection and schema extension handling remain distinct. |
| Draft-07 `$ref` siblings, nested base IDs and recursive schemas | Correct dialect behavior; no hangs or fabricated assertion failure for resolver/resource errors. |
| Decimal `multipleOf`, equivalent numeric enum/const and unordered-object `uniqueItems` | Exact schema number/deep-equality behavior. |
| Editor `1e-1`/`1.0`, extra escapes, decoded duplicates and astral character positions | Runtime/editor agreement in values, diagnostics and selected source ranges. |
| Valid/invalid manifest field and path fixtures | Schema/profile versus filesystem rejection is identified explicitly; no process-control calls. |

### Verification matrix

| Configuration | Required evidence |
| --- | --- |
| Native Linux, GCC C++17, exceptions OFF | Parser/schema unit and corpus tests, standalone/installed consumer compile/link. |
| Native Linux, Clang C++17, exceptions OFF | Same semantic checks plus applicable sanitizer/fuzzer runs. |
| Native exception-enabled configuration, normally CI | Same success/failure semantics and allocation-cleanup checks; enabling exceptions does not change the normal error API. |
| Existing Yocto ARM64 SDK, exceptions OFF | Parser and selected schema component cross-build, public-header consumer checks, ELF/link dependency inspection. |
| ARM64 QEMU using existing deployment | Representative parsing, schema/profile and error cases, including ownership after input destruction. |
| VS Code on supported host/Remote SSH setup | Installed-editor checks and parity corpus with recorded extension/backend versions. |
| Offline installed package | Build and local-schema validation without fetching sources or schemas at configure/runtime. |

## Suggested implementation milestones

These milestones organize the work; they do not authorize implementation or
change any requirement's Draft status. Internal lexer tests can finish before a
public complete-parser API. Later target/editor scenarios provide integration
evidence rather than becoming prerequisites for early lexical checks.

1. **Foundation:** review API/error/allocation contracts; prepare CMake and native
   consumer builds. Decide the lossless string and exact-number representations.
2. **Grammar:** implement strict JSON token/structure behavior, then complete
   JSON5 features with isolated lexer/decoder tests and the coverage matrix.
3. **Library correctness:** owned document model, duplicate policy, conversions,
   limits and failure injection; publish no premature full-support claim.
4. **Schema:** complete the independent Draft-07 component and its suite evidence.
   A learning release containing only the parser should state that scope clearly.
5. **Tooling:** CLI and VS Code integration, including incomplete input and parity.
6. **Consumer:** reviewed AdaptivePi profile/schema, CMake consumption and ARM64
   target evidence, followed by a reproducible standalone release.

## Review decisions and handoff

The following are proposed design choices, not unresolved language grammar:

- MIT for new original code; dependency notices remain separate.
- C++17, JSON5 default and selectable strict JSON.
- Preserve-all duplicate policy for the generic parser; reject duplicates for
  schema instances and the AdaptivePi profile.
- Lossless handling of escaped UTF-16 units, with checked Unicode/UTF-8 boundaries.
- Draft-07 required assertions/references; optional format/content assertions
  disabled unless specifically implemented and advertised.
- Independently buildable schema/editor components; using compatible components
  there remains allowed while the JSON5 reader is developed in this repository.
- Named configurable budgets with numeric defaults/calibration documented before
  release, rather than invented universal time or memory guarantees here.

AdaptivePi-specific findings remain open: integer spelling/value policy, fixed
versus dynamic unknown keys, profile Unicode rejection
and the scope of consumer-model allocation recovery. The current JSON5 requirements and
these JSON5 integration proposals must be reconciled during Release 4 review.
Do not mark MAN-001 Agreed merely because this standalone direction was selected.

Recommended first review entry: **J5-001**. Review the foundations before settling
storage/API choices; implementable acceptance checks are provided for J5-001–100.
This document contains requirements and plans only: no parser/schema/editor code,
executed conformance results or repository publication.
