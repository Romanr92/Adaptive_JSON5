# ADR 0006: Schema component decisions (P043–P055)

Status: Decided, 2026-11-22. Started 2026-11-13; the open points of the 2026-11-14
review (O1–O10) were settled by the user on 2026-11-22 (last section).
Production C++ is written by the human developer; this record does not claim
implementation or verification. Requirement statuses are kept in the requirements registry.

Context: issue #44, P043–P046 of the
[implementation sequence](requirements-json5-implementation-order.md). Related
contracts in [requirements](../../docs/requirements.md): J5-053, J5-055, J5-056,
J5-059, J5-061, J5-063–J5-068.

| # | Topic | Status |
| --- | --- | --- |
| 1 | Layout and build target | Decided: separate `adaptive_json5_schema` target; own package component; ON in test/validation/fuzz builds, OFF for library-only and release builds; QM |
| 2 | Dialect API | Decided: explicit `schema_dialect::draft_07`; Draft-07 URI with or without trailing `#`, exact match; subschema `$schema` ignored |
| 3 | Outcome model | Decided: compile → `result<compiled_schema, schema_error>`, validate → `result<validation_report, schema_error>`; assertion failure is a report, not an error |
| 4 | Storage ownership | Decided: compiled schema owns a copied, immutable, move-only graph in caller storage; separate work storage per validation |
| 5 | Budget struct and defaults | Decided: per-category validation caps plus overall `max_work_steps`; compile limits `max_schema_nodes` and `max_compile_steps`; defaults in `docs/memory-and-limits.md` |
| 6 | Instance domain | Decided: one pre-pass returns `unsupported_instance` for duplicate names, NaN/Infinity and unpaired surrogates |
| 7 | Cancellation | Decided: optional callback polled once per evaluation step; returns `cancelled` |
| 8 | URI handling | Decided: internal RFC 3986 §5 resolver; lowercase scheme/host, dot segments removed, empty fragment = none, no percent-decoding |
| 9 | Registry API | Decided: caller registers (base URI, schema source text) pairs, parsed by the registry as strict JSON with `reject_duplicates`; duplicate URI is an error; no I/O |
| 10 | Compiled graph shape | Decided: flat indexed node array, `$ref` resolved to indices at compile time; standard annotations kept, unknown keywords counted |
| 11 | Exact equality | Decided: type-aware, exact numbers, order-free objects, ordered arrays, explicit stack |
| 12 | Evaluation engine | Decided: explicit stack, `$ref` siblings ignored, loop detection on the active `$ref` chain, fail-fast default and bounded collect-all with a truncated flag |
| 13 | Diagnostic contents | Decided: fixed-size records; JSON Pointers formatted on request into a caller buffer |

## Decision 1: layout and build target

### Decision

A separate library target `adaptive_json5_schema` that links `adaptive_json5`.

- Public headers: `includes/adaptive_json5/schema/`, entry header
  `adaptive_json5/schema.hpp`. The umbrella `adaptive_json5.hpp` stays parser-only.
- Private headers: `core/includes/schema/`; sources: `core/src/schema/`.
- Internal units: `core/tests/schema/`; public-interface units: `tests/schema/`.
- `ADAPTIVE_JSON5_BUILD_SCHEMA` defaults ON when unit, validation or fuzz tests are built,
  so every CI check covers the schema to the repository's standards, and OFF otherwise;
  the release `.a` packages are built with it OFF (owner decision, 2026-11-22). One
  unit-test job builds explicitly with it OFF to check J5-068. When OFF, no schema source,
  header or link dependency is built or installed (J5-068).
- The installed package exposes the schema as its own component
  (`find_package(AdaptiveJSON5 COMPONENTS schema)`) with a separate archive in the
  release bundle.
- CI: schema ON is added to the existing unit-test matrix (Windows MinGW and Linux
  GCC, exceptions on and off), so the same suite runs on every supported
  configuration (J5-091). Parser-only builds stay in the matrix to keep checking
  J5-068.
- Dependencies: `docs/dependencies.md` lists every transitive schema dependency with
  its license (J5-068). Today that is only the parser; the regex engine is added with
  P048.
- Safety scope: the schema target is QM, like the hosted add-on, and outside the
  ASIL B(D) and CAL 4 scope of requirement sections K and L. Its intended use is
  build-time validation of handwritten manifests (for example AdaptivePi execution
  manager manifests) on a host or in CI; runtime validation on the target is not
  planned.
- Deferred until the schema target exists: `CMakeLists.txt`, which currently stops
  with an error when the option is ON, and `docs/release-packages.md`, which says a
  required `schema` component fails, are updated then.

### Rejected alternatives

- Adding schema sources to the existing library when the option is ON: the same
  library name would contain different code depending on the flag, and J5-068's
  transitive-dependency listing becomes ambiguous.
- A separate top-level `schema/` tree: conflicts with
  [ADR 0002](adr-0002-public-private-source-layout.md).
- One extra schema-ON CI job: covers only one of the four configurations J5-091
  requires.
- Placing the schema inside the ASIL B(D) scope now: regex and `$ref` resolution add
  substantial certification work with no runtime use planned.

## Decision 2: dialect API

### Decision

The caller always chooses the dialect explicitly through a `schema_dialect` enum
passed to compile. Only `draft_07` exists for now; there is no default that silently
picks a draft (J5-053).

- Root `$schema` missing: compile as the dialect the caller chose.
- Root `$schema` equal to `http://json-schema.org/draft-07/schema#` or
  `http://json-schema.org/draft-07/schema`: accept as Draft-07. Matching is an exact
  string comparison, with no URI normalization.
- Any other string, including `https` and other drafts: fail with
  `unsupported_dialect`.
- Root `$schema` that is not a string: fail with `invalid_schema`.
- `$schema` in a subschema: ignored, since Draft-07 gives it meaning only at the root.
- Each registered external resource's root `$schema` is checked the same way. Mixing
  dialects is not supported: any other dialect there fails with
  `unsupported_dialect`.

### Rejected alternatives

- Inferring the dialect from `$schema` alone with no caller parameter: a missing
  `$schema` would force a silent assumption, which J5-053 forbids.
- Accepting only the `#` form: follows the meta-schema exactly but rejects common
  real-world schemas.
- Rejecting a subschema-level `$schema` as `invalid_schema`: Draft-07 says it should
  not appear in subschemas, but the Draft-07 meta-schema (J5-054) accepts it and
  bundled real-world schemas contain it, so rejecting it would break valid input.

## Decision 3: outcome model

### Decision

A failed schema assertion is a normal outcome, not an error (J5-063). The API is split
by phase, using the existing non-throwing `result` type:

- Compile returns `result<compiled_schema, schema_error>`.
- Validate returns `result<validation_report, schema_error>`.
- `validation_report` states whether the instance was valid or failed assertions, and
  holds the collected diagnostics.
- `schema_error` is constructible without allocation (J5-045). Compile errors carry a
  schema location as an index or offset, not an owned string (J5-064).
- An operational failure always wins: if memory runs out, a budget is exceeded or the
  run is cancelled after an assertion has already failed, the result is the
  `schema_error` and no partial report is published (J5-063, J5-051).
- Reason names reuse the parser's `error_code` names where the meaning is the same
  (`resource_limit`, `allocation_failure`, `internal_error`), so integrators see the
  same words in both layers (J5-043).

`schema_error` reasons and the phase each can occur in:

| Reason | Compile | Validate | Notes |
| --- | --- | --- | --- |
| `invalid_schema` | yes | no | Sub-reason: syntax (registration, with the parser's error), keyword shape, invalid pattern, unsupported pattern (J5-054, J5-060) |
| `unresolved_reference` | yes | no | |
| `unsupported_dialect` | yes | no | Decision 2 |
| `unsupported_option` | yes | no | A requested assertion mode for `format`, `contentEncoding` or `contentMediaType` that is not supported (J5-069) |
| `unsupported_instance` | no | yes | Decision 6 |
| `nonprogressing_reference` | yes | yes | Compile: pure `$ref` cycles such as `{"$ref": "#"}`; validate: instance-dependent cycles (J5-056) |
| `cancelled` | no | yes | Decision 7 |
| `resource_limit` | yes | yes | Sub-reason names the exceeded limit (decision 5) |
| `allocation_failure` | yes | yes | |
| `internal_error` | yes | yes | A violated library invariant (J5-043) |
| `invalid_registration` | no | no | Registration phase only; sub-reasons duplicate URI, relative URI (O3) |

A third phase, registration, covers adding resources to the registry (O3). It can
return `invalid_registration`, `invalid_schema` (syntax and domain sub-reasons),
`resource_limit`, `allocation_failure` and `internal_error`.

### Rejected alternatives

- A single combined compile-and-validate call: hides which phase failed and prevents
  reusing one compiled schema across validations.
- Reporting assertion failures as `schema_error`: conflates an invalid instance with a
  failure of the validator itself.
- Leaving assertion-mode options out of the API until they are supported: avoids
  `unsupported_option` now but forces a public API change later.
- Separate top-level reasons for invalid and unsupported patterns: a sub-reason keeps
  them distinct without growing the top-level list.

## Decision 4: storage ownership

### Decision

- Compilation copies everything the compiled schema needs into caller-provided
  storage, using the existing `storage` and `accounted_storage` types. Input documents
  and the registry can be destroyed afterwards (J5-066).
- A `compiled_schema` is immutable after compilation and move-only.
- Each validation run uses its own separate work storage, released before the call
  returns (J5-065).
- Concurrent validations of one compiled schema are safe when each uses its own work
  storage; sharing one work context between threads is unsupported and documented as
  such (J5-050).
- A hosted convenience helper using the global heap may be added later for
  build-time tools, following the hosted add-on pattern.

### Rejected alternatives

- Keeping references into the input schema documents or registry: ties the compiled
  schema's lifetime to temporaries, contrary to J5-066.
- Storing per-validation state inside the compiled schema: breaks safe concurrent
  validation.

## Decision 5: budget struct and defaults

### Decision

A budget struct with these fields and defaults, sized generously for build-time
validation (J5-065, J5-049). All limits are inclusive; exceeding one returns
`resource_limit` with a sub-reason naming the field. The one exception is
`max_errors`: reaching it in collect-all mode is not an error (decision 12).

| Field | Default | Counts |
| --- | --- | --- |
| `max_depth` | the parser's default nesting limit (`resource_limits::max_depth`, 512) | Nested schema and instance evaluation depth |
| `max_reference_steps` | 10,000 | `$ref` traversals per validation |
| `max_branch_evaluations` | 100,000 | Subschema evaluations under `anyOf`, `oneOf`, `allOf`, `not`, `if` |
| `max_equality_steps` | 100,000 | Nodes visited by `const`, `enum`, `uniqueItems` |
| `max_errors` | 100 | Diagnostics kept in collect-all mode |
| `max_work_steps` | 1,000,000 | Overall cap on all evaluation steps per validation |
| regex budget | placeholder, unset | Defined with the regex engine in P048 |

Compilation has its own limits, also inclusive and reported as `resource_limit` with
a sub-reason (J5-047, J5-049):

| Field | Default | Counts |
| --- | --- | --- |
| `max_schema_nodes` | 100,000 | Nodes in the compiled graph, across all registered resources |
| `max_compile_steps` | 1,000,000 | Compile work: keyword checks, `$ref` resolution and pure-cycle detection |

Compilation has no cancellation callback, since it runs at build time and these
limits bound it.

Defaults and accounting rules are documented in `docs/memory-and-limits.md`, next to
the parser's limits, when the budget struct is implemented; this ADR records the
reasoning.

### Rejected alternatives

- Per-category caps only: a schema could spread work across categories;
  `max_work_steps` closes that.
- Unlimited defaults: contrary to the bounded-work intent of SSR-02 and J5-049, and
  pathological schemas would hang tools.

## Decision 6: instance domain

### Decision

Before any assertion runs, one pass over the instance checks that it is a
JSON-compatible value (J5-059, J5-061). It returns `unsupported_instance` for:

- duplicate member names in an object;
- NaN or Infinity numbers;
- strings or member names containing unpaired surrogates.

The pass runs once per validation and is charged to the validation budget
(decision 5).

Duplicate names: the parser's `duplicate_name_policy` has two modes, and neither
drops a member, so validation never sees a silently cleaned instance.

- `reject_duplicates` (recommended for manifests): duplicates fail at parse time with
  both locations, before validation is reached.
- `preserve_all` (the parser default): every member is kept, so the pre-pass finds
  the duplicates and returns `unsupported_instance`.

A parsed document does not record which policy was used, so the pre-pass always
checks; with `reject_duplicates` it simply finds none.

### Rejected alternatives

- Checking these cases lazily during evaluation: an assertion could pass or fail
  before the unsupported value is reached, contrary to J5-059's "rejected before
  assertions".
- Treating them as assertion failures: they are not schema verdicts, so they must not
  appear as an invalid instance.

## Decision 7: cancellation

### Decision

- The caller may pass an optional cancellation callback (function pointer plus
  context pointer) with the validation's work context (J5-049).
- The callback is polled once per evaluation step. When it asks to stop, validation
  returns `cancelled` and releases its work storage before returning (J5-065,
  J5-051).
- No callback means no cancellation polling; budgets still bound the work.
- The callback must not touch the compiled schema or the work context; this is
  documented as an integrator obligation.

### Rejected alternatives

- A caller-polled flag: needs atomics for cross-thread use and cannot express other
  stop conditions such as deadlines.

## Decision 8: URI handling

### Decision

A small internal RFC 3986 §5 reference resolver with no external dependency
(J5-055). Normalization:

- Scheme and host are lowercased, as RFC 3986 defines them as case-insensitive.
- `.` and `..` path segments are removed during resolution.
- An empty fragment is treated as no fragment, so a base URI with and without a
  trailing `#` names the same resource.
- Percent-encoding is not decoded or normalized; registry lookups compare the
  normalized URI exactly.

### Rationale

The first three follow RFC 3986's resolution algorithm and common Draft-07
identifiers. Percent-decoding is skipped because it is a frequent source of subtle
bugs and spoofed matches; exact matching means what is registered is exactly what is
found.

### Rejected alternatives

- Full RFC 3986 §6 normalization including percent-decoding: more code and more
  ambiguity for no need in handwritten manifests.
- An external URI library: adds a dependency to J5-068's list.

## Decision 9: registry API

### Decision

- The caller registers pairs of base URI and schema source text (J5-055). The
  registry parses each text itself as strict JSON with `reject_duplicates`, so
  compile knows every schema is valid JSON with no duplicate keywords (J5-054,
  J5-061). An already-parsed document is not accepted, because JSON5-only syntax
  such as comments or unquoted names leaves no trace after parsing.
- A schema text that fails to parse at registration returns `invalid_schema` with a
  syntax sub-reason, carrying the parser's own error and location unchanged, so the
  caller sees exactly where the text broke (J5-043, J5-044).
- The registry parses into its own caller-provided storage, which the caller may
  release once compilation has finished.
- Compilation copies what it needs into the compiled schema, so the source texts,
  their parsed documents and the registry can be destroyed afterwards (J5-066).
- Registering the same normalized URI twice is an error, so a lookup can never
  silently pick one of two schemas.
- The schema code never touches the network or filesystem; all external schemas
  arrive through the registry (J5-067).

### Rejected alternatives

- Accepting already-parsed documents and re-checking them: cannot detect JSON5-only
  syntax or duplicates that were kept, because a parsed document does not record its
  syntax mode or duplicate policy.
- A resolver callback that loads schemas on demand: moves I/O into compilation and
  makes results depend on the environment.
- Last registration wins on a duplicate URI: hides mistakes in which schema is
  actually used.

## Decision 10: compiled graph shape

### Decision

- Nodes are stored in one flat indexed array; children and `$ref` targets are node
  indices, not pointers.
- Every `$ref` is resolved to a node index at compile time. A reference that cannot
  be resolved fails compilation with `unresolved_reference` (J5-055).
- Each node keeps its schema location for diagnostics (J5-064): its parent's node
  index and the step that leads to it (a member name or an array index), copied into
  the compiled schema. The formatter builds the RFC 6901 pointer from these after the
  schema sources are gone (decisions 4 and 13).
- Standard annotations are kept: `title`, `description`, `default`, `examples`,
  `$comment`, `readOnly`, `writeOnly`, and `format`, `contentEncoding`,
  `contentMediaType` as annotations (J5-058, J5-069; needed later by P053).
- Unknown keywords are not stored; their count is kept per node so tooling can warn
  about likely misspellings without inventing an assertion (J5-062).

### Rejected alternatives

- A pointer-linked tree: harder to copy, move and bound, and cycles complicate
  ownership.
- Storing unknown keywords in full: costs storage with no validation meaning; can be
  added later if tooling needs their names.

## Decision 11: exact equality

### Decision

Type-aware deep equality for `const`, `enum` and `uniqueItems` (J5-061):

- Numbers use the existing exact `number_comparison`, so `1`, `1.0` and `1e0` are
  equal (J5-059).
- Objects compare member counts first, then look up each member by decoded name;
  member order does not matter.
- Arrays compare element by element in order.
- Values of different kinds are never equal; booleans are not numbers.
- Traversal uses an explicit stack, not recursion. Every visited node is charged to
  `max_equality_steps` and `max_work_steps` (decision 5).

### Rejected alternatives

- Recursive comparison: unbounded stack use, contrary to the bounded-stack safety
  work.
- Comparing numbers by spelling or as `double`: wrong for equivalent spellings and
  large values.

## Decision 12: evaluation engine

### Decision

- Evaluation uses an explicit stack, not recursion, charged to `max_depth` and
  `max_work_steps` (J5-048).
- When a schema node has `$ref`, its sibling assertions are ignored, as Draft-07
  specifies (J5-056).
- A non-progressing loop is the same (schema node, instance node) pair appearing
  again on the active `$ref` chain. It returns `nonprogressing_reference`, never an
  invalid-instance result (J5-056).
- Two modes (J5-063): fail-fast, the default, stops at the first assertion failure;
  collect-all gathers diagnostics up to `max_errors`. Reaching `max_errors` is not an
  error: the report still says invalid and sets a `truncated` flag, because the
  verdict is known and only the list is incomplete. Fail-fast never reaches it.

### Rejected alternatives

- Collect-all as the default: more work and storage per validation by default;
  callers who want every error opt in.
- Detecting loops by a step count alone: cannot tell a genuine cycle from a large
  valid recursive instance, and would misreport it as a budget failure.

## Decision 13: diagnostic contents

### Decision

Each assertion diagnostic in `validation_report` is a small fixed-size record
(J5-064):

- the failed keyword, as an enum;
- the instance node index and its source byte range in the parsed document (J5-044);
- the schema node index, its source range and the registered resource it came from.

JSON Pointer strings (instance pointer and schema location, escaped per RFC 6901) are
not stored. A formatter writes them into a caller-provided buffer on request, like the
parser's `describe()`. The report therefore needs no allocation per diagnostic;
collect-all mode stores at most `max_errors` records (decision 5).

### Rejected alternatives

- Storing pointer strings in each diagnostic: allocation per error and unbounded size
  for deep instances.
- Diagnostics with keyword only: not enough to locate the failure, contrary to
  J5-064.

## Review against requirements (2026-11-13)

Decisions 1–3 were checked against `docs/requirements.md` on the issue branch. No
requirement was contradicted. The gaps and tidy-ups found were accepted by the user
and are folded into the decisions above:

| ID | Requirement | Resolution |
| --- | --- | --- |
| G1 | J5-043 | `internal_error` added, both phases (decision 3) |
| G2 | J5-069 | `unsupported_option` added, compile only (decision 3) |
| G3 | J5-054, J5-060 | `invalid_schema` sub-reasons (decision 3) |
| G4 | J5-091 | Schema ON in the full CI matrix (decision 1) |
| T1 | J5-043 | Parser `error_code` names reused (decision 3) |
| T2 | J5-045, J5-064 | Allocation-free `schema_error` with schema location (decision 3) |
| T3 | J5-068 | Dependency list in `docs/dependencies.md` (decision 1) |
| T4 | Sections K, L | Schema target is QM, build-time use (decision 1) |
| T5 | — | Decision 2 rationale for subschema `$schema` corrected |
| T6 | J5-056 | `nonprogressing_reference` also at compile time for pure `$ref` cycles (decision 3) |
| T7 | — | `CMakeLists.txt` and `docs/release-packages.md` updated once the schema target exists (decision 1) |

## Review of decisions 4–13 against requirements (2026-11-13)

Decisions 4–13 were checked against `docs/requirements.md` on the issue branch. No
requirement was contradicted. The findings were settled by the user and are folded
into the decisions above:

| ID | Requirement | Finding | Resolution |
| --- | --- | --- | --- |
| R1 | J5-063, J5-065 | Decisions 5 and 12 disagreed on what reaching `max_errors` returns | A report that says invalid with a `truncated` flag, not `resource_limit` (decisions 5, 12) |
| R2 | J5-063, J5-051 | No rule for an assertion failure followed by an operational failure | The operational error wins; no partial report (decision 3) |
| R3 | J5-047, J5-049 | Compilation had no limits | `max_schema_nodes` 100,000 and `max_compile_steps` 1,000,000; no compile cancellation (decision 5) |
| R4 | J5-054, J5-061 | Compile could not verify that schemas were strict JSON | The registry takes source text and parses it strictly with `reject_duplicates` (decision 9) |
| R5 | J5-064 | Schema location for diagnostics was undefined | Each node stores its parent index and the step leading to it (decision 10) |

Registration details for P044 (agreed 2026-11-13) are recorded in decision 9.

## Open points settled (2026-11-22)

The 2026-11-14 review of decisions 1–13 found these gaps. The user adopted every
suggested resolution:

| ID | Topic | Decision |
| --- | --- | --- |
| O1 | Plain-name `$id` (decisions 8, 10) | A `$id` that is only a fragment (`#foo`) registers an extra lookup key, the current base URI with that fragment, for its node; `$ref: "#foo"` finds it. |
| O2 | `$id` beside `$ref` (decisions 8, 12) | A `$id` beside `$ref` is ignored at compile time, like every other sibling; it neither changes the base URI nor registers a key. |
| O3 | Registration errors (decisions 3, 9) | New registration phase and reason `invalid_registration` with sub-reasons `duplicate_uri` and `relative_uri`. |
| O4 | Root base URI (decision 8) | Text registered without a URI gets the fixed, documented default base URI `adaptive-json5:/root.json`. |
| O5 | Unpaired surrogates in schema text (decisions 6, 9) | Registration fails with `invalid_schema` and the `domain` sub-reason. |
| O6 | Diagnostic lifetime (decision 13) | Diagnostic source ranges refer to the registered schema text and the parsed instance document; they stay meaningful only while the caller keeps those texts. Node and value indices need no source. |
| O7 | Compile storage (decision 4) | Compile takes two storages: result storage owned by the compiled schema, and scratch storage released before compile returns. |
| O8 | Pre-pass budget (decision 6) | The instance pre-pass is charged to `max_work_steps` (and `max_depth` for nesting). |
| O9 | ADR status | Status set to Decided. |
| O10 | Issue #44 checklist | The completion checklist names every AGENTS.md gate: unit, validation, formatting, sanitizer, fuzz, Clang and static-analysis jobs. |

`$ref` fragments that are JSON Pointers are decoded per RFC 6901 §6: `%XX` escapes and
then `~1`/`~0` are decoded in pointer tokens only. Decision 8's "no percent-decoding"
applies to URI comparison, not to pointer tokens.

## Assertion keywords (P047–P051, issue #48)

Decided by the owner on 2026-11-22 for issue #48 (J5-057, J5-059, J5-060, J5-061, J5-065):

- **const, enum and other schema values.** Compilation copies `const`, `enum`, the numeric
  bounds, `required` and array-form `dependencies` into an owned value document inside the
  compiled schema (result storage), so decision 4 holds unchanged: the registry and the
  schema texts can be destroyed after compilation. Comparisons use the exact equality of
  decision 11.
- **Regex dialect and engine.** `pattern` and `patternProperties` use an own ECMA-262
  compiler and a Pike VM (NFA simulation without backtracking) on Unicode code points, with
  caller storage only and no third-party dependency. Matching time is bounded by pattern size
  times input length and charged to `validation_budget::max_regex_steps` (default
  1,000,000). Supported: literals and escapes, `.`, classes with ranges and negation,
  `\d \D \w \W \s \S`, `^ $ \b \B`, groups and `(?:...)`, alternation and all greedy and
  lazy quantifiers; Annex B literal `{`, `}` and `]`. Backreferences, lookaround, named
  groups, `\p`, `\c` (both supported since issue #187, see below) and legacy octal escapes return `invalid_schema` with the
  `unsupported_pattern` sub-reason; malformed patterns `invalid_pattern`. `.` matches one
  code point (the behavior of ECMA-262's `u` flag); there are no flags.
- **Lengths** (`minLength`, `maxLength`) count code points; the instance check guarantees
  that every surrogate is paired.
- **propertyNames** validates a copy of each member name as a string and reports one
  `propertyNames` diagnostic for the object, like `contains`, instead of diagnostics inside
  the copies.

### Rejected alternatives

- A backtracking engine: closer to full ECMA-262, but hostile patterns would only be stopped
  by the step budget.
- RE2 or PCRE2: allocate through their own heap, may throw or abort on exhaustion and add a
  third-party dependency (J5-046, J5-068, J5-101).
- Moving the parsed schema documents into the compiled schema: less code, but keeps whole
  documents alive and changes the compile API (decision 4).

## Logic, annotations, compilation and conformance (P052–P055, issue #53)

Decided by the owner on 2026-11-24 for issue #53 (J5-053, J5-054, J5-058, J5-062, J5-065,
J5-069, J5-096):

- **Annotations (D1).** Collection is opt-in (`validation_options::collect_annotations`).
  Compilation copies each node's annotation keywords (`title`, `description`, `default`,
  `examples`, `readOnly`, `writeOnly`, `$comment`, `format`, `contentEncoding`,
  `contentMediaType`) and unknown keywords into the value document. A
  `schema_annotation` uses the diagnostic layout with keyword `none`. Records come only from
  subschemas that pass: a failed frame drops its records, a `not` subschema and
  propertyNames never contribute, `$ref` siblings are ignored, and an invalid report carries
  none. While collecting, anyOf evaluates every branch. Records live in report storage and
  are bounded by the evaluated frames. Instances are never changed.
- **Meta-schema (D2).** The unmodified Draft-07 meta-schema is embedded under its
  BSD-3-Clause option. `compile_schema()` validates every registered resource against it
  after the dialect and built-in checks, so `unsupported_dialect` and pattern errors keep
  their classes; a mismatch is `invalid_schema` / `keyword_shape` at the offending value,
  bounded by `max_compile_steps`, in scratch storage.
- **Optional assertions (D3).** `compile_options::assert_format` asserts date-time, date,
  time (RFC 3339, leap second only at 23:59 UTC), email (RFC 5321: dot-atom or quoted local
  part, host name or address literal), hostname (RFC 1123), ipv4, ipv6, uri and
  uri-reference (RFC 3986 including the authority), json-pointer, relative-json-pointer and
  regex (not malformed for the own engine; `(?` forms outside ECMA-262 are malformed).
  Other names stay annotations. Checks are charged to `max_regex_steps`. Content
  assertion is unsupported: `assert_content` returns `unsupported_option`.
- **Conformance (D4).** JSON-Schema-Test-Suite release `Test-JSON-Schema-Acceptance-1.041`
  (the newest with the `tests/draft7` and `remotes` layout) is pinned and run by its own
  pull-request workflow, which is informative until the owner makes it required. The
  suite led to an exact digit-by-digit `multipleOf` remainder, so huge values such as
  `1e308` no longer reach the work limit.

### Rejected alternatives

- Static per-node metadata without validation-time collection: cannot give valid-branch
  semantics (J5-058).
- Shape checks only, with a test that they agree with the meta-schema: J5-054 asks for the
  meta-schema check itself.
- Running the suite in the unit-test executables: mixes third-party fixtures into the unit
  gate, unlike the existing corpora.

## Optional draft7 conformance (issue #187)

Decided by the owner on 2026-11-27 for issue #187 (J5-060, J5-069, J5-096). This section
supersedes the `\p` and `\c` rule of the assertion-keyword section above:

- **Control escapes.** `\cX` with an ASCII letter X matches the code point `X % 32`. `\c`
  followed by anything else follows Annex B (a literal backslash, then `c`), like the
  accepted `\a` identity escape.
- **Unicode property escapes.** `\p{…}` and `\P{…}` (negated), inside and outside classes,
  accept General_Category values by short or long name and the lone forms (`L`, `Letter`,
  `Nd`, `digit`, …), the `gc=`/`General_Category=` forms, `Script=`/`sc=` script names and
  the binary properties `Any`, `ASCII` and `Assigned`. Other ECMA-262 binary properties and
  `Script_Extensions` return `unsupported_pattern`; an unknown name is `invalid_pattern`.
  Matching uses one pinned Unicode version, and every range a property set adds is charged
  to `max_compile_steps`.
- **IDN host names.** With `assert_format`, `hostname` also validates A-labels (`xn--`):
  RFC 3492 Punycode decoding with overflow checks, then IDNA2008 (RFC 5891 section 5.4,
  RFC 5892 with its Appendix A context rules): the U-label must not be all ASCII, every
  code point must be PVALID or satisfy its CONTEXTJ/CONTEXTO rule, a label must not start
  with a combining mark, and a U-label must not have `-` at either end or `--` in positions
  3–4, and must be in NFC (RFC 5891 section 5.4; checked by normalizing a copy in fixed
  storage: canonical decomposition, ordering and composition, Hangul included). When any
  label of the host holds a right-to-left character (Bidi_Class R, AL or AN), every label
  must satisfy the RFC 5893 section 2 Bidi rule. Plain labels keep the RFC 1123 rules.
- **Unicode data.** Unicode 17.0.0, the version of the parser's identifier tables. A
  separate generator (`tools/generate_schema_unicode_tables.py`) builds constexpr range
  tables for General_Category, Script, Joining_Type, Virama (combining class 9), Bidi_Class,
  Canonical_Combining_Class, canonical decompositions and primary composites, and the
  IDNA2008 derived property from the pinned UCD files in `tools/unicode-data/`. IANA
  publishes derived IDNA tables only up to Unicode 12.0.0, so the derived property is
  computed with the RFC 5892 algorithm and cross-checked against IANA's 12.0.0 table in the
  generator's tests. The tables belong to the schema library only; the parser is unchanged.
- **Conformance.** The runner no longer expects `unsupported_pattern` for the
  ecmascript-regex `\c` and `\p` groups and no longer skips the A-label group of
  `optional/format/hostname.json`. The `\a` deviation (Annex B) stays.

### Rejected alternatives

- Keeping the explicit `unsupported_pattern` errors: conservative, but schemas written for
  ECMA-262 `u`-flag engines use `\p{…}` widely.
- Using IANA's 12.0.0 IDNA table directly: official, but leaves the property tables of the
  regex (17.0.0) and the host name check on different Unicode versions.
- A third-party IDNA or Unicode library: adds a dependency with its own allocation and
  error model (J5-046, J5-101).
