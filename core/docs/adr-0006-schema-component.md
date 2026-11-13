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
| 5 | Budget struct and defaults | Decided: per-category caps plus overall `max_work_steps`; defaults in `docs/memory-and-limits.md` |
| 6 | Instance domain | Open |
| 7 | Cancellation | Open |
| 8 | URI handling | Open |
| 9 | Registry API | Open |
| 10 | Compiled graph shape | Open |
| 11 | Exact equality | Open |
| 12 | Evaluation engine | Open |

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
- Reason names reuse the parser's `error_code` names where the meaning is the same
  (`resource_limit`, `allocation_failure`, `internal_error`), so integrators see the
  same words in both layers (J5-043).

`schema_error` reasons and the phase each can occur in:

| Reason | Compile | Validate | Notes |
| --- | --- | --- | --- |
| `invalid_schema` | yes | no | Sub-reason: keyword shape, invalid pattern, unsupported pattern (J5-054, J5-060) |
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
`resource_limit` with a sub-reason naming the field.

| Field | Default | Counts |
| --- | --- | --- |
| `max_depth` | the parser's default nesting limit (`resource_limits::max_depth`, 512) | Nested schema and instance evaluation depth |
| `max_reference_steps` | 10,000 | `$ref` traversals per validation |
| `max_branch_evaluations` | 100,000 | Subschema evaluations under `anyOf`, `oneOf`, `allOf`, `not`, `if` |
| `max_equality_steps` | 100,000 | Nodes visited by `const`, `enum`, `uniqueItems` |
| `max_errors` | 100 | Diagnostics kept in collect-all mode |
| `max_work_steps` | 1,000,000 | Overall cap on all evaluation steps per validation |
| regex budget | placeholder, unset | Defined with the regex engine in P048 |

Defaults and accounting rules are documented in `docs/memory-and-limits.md`, next to
the parser's limits, when the budget struct is implemented; this ADR records the
reasoning.

### Rejected alternatives

- Per-category caps only: a schema could spread work across categories;
  `max_work_steps` closes that.
- Unlimited defaults: contrary to the bounded-work intent of SSR-02 and J5-049, and
  pathological schemas would hang tools.

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
