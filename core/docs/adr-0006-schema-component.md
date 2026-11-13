# ADR 0006: Schema component decisions (P043–P046)

Status: In progress, started 2026-11-13. Decisions are recorded one at a time as the
user makes them; implementation starts only after the whole list is decided.
Production C++ is written by the human developer; this record does not claim
implementation or verification. All requirement rows stay Draft.

Context: issue #44, P043–P046 of the
[implementation sequence](requirements-json5-implementation-order.md). Related
contracts in [requirements](../../docs/requirements.md): J5-053, J5-055, J5-056,
J5-059, J5-061, J5-063–J5-068.

| # | Topic | Status |
| --- | --- | --- |
| 1 | Layout and build target | Decided: separate `adaptive_json5_schema` target; own package component; option OFF by default; QM |
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
- `ADAPTIVE_JSON5_BUILD_SCHEMA` stays OFF by default. When OFF, no schema source,
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
