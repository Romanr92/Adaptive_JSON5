# AI agent guidelines

## Source of truth

Read `README.md`, `docs/requirements.md`, `docs/traceability.md` and the relevant
ADR/skill before requirement, design or unit-test work. Use active J5 contracts,
their status, dependencies and documented deviations. The proposal under
`docs/requirements/proposals/` is preserved Draft source material; later user
instructions and the adapted requirements supersede conflicting proposal text.
Do not silently change a requirement or invent implementation evidence.

## Authorized work and constraints

AI agents can change build setup, documentation and unit tests, not production
code. This covers CMake, GoogleTest units, documentation, requirements and
repository/issue organization. **Do not create or modify production C++
implementation or public C++ headers.** Production code is written by Romanr92.

Test-only C++ under top-level `tests/` and `core/tests/` is permitted. Internal
component units belong in `core/tests/`; public-header/API and whole-library
behavior units using only public interfaces belong in top-level `tests/`.
Both remain GoogleTest unit tests under the existing quality scope. Do not
disguise production logic as test helpers or generated code to bypass the
restriction, and never include `core/tests/` in production source lists.

- Prepare or edit CMake, GoogleTest units, formatting, documentation, requirements,
  ADRs, repository skills and the authorized issue backlog.
- When asked to implement or fix production C++, explain the approach and guide
  the developer. Test failures do not authorize an agent to fix production files.
- Design the core as a generic C++17 library for compatible hosted projects and
  embedded devices after compilation into a compatible project. The core is
  OS-agnostic; MICROSAR is only an example, not a required vendor port. Read `docs/embedded.md` and J5-101 for runtime/storage boundaries.
- Do not require WSL, AdaptivePi, an OS service, SDK, Node.js, VS Code or host test
  dependencies in the parser. Schema, CLI, editor and examples remain optional.
- Unit tests, validation tests and formatting are the only automated quality checks.
  Validation tests are GoogleTest cases over the pinned third-party fixtures in
  `tests/validation`, built only by `ADAPTIVE_JSON5_BUILD_VALIDATION_TESTS` and run
  as a separate pull-request gate. Do not add
  integration, end-to-end, fuzz, sanitizer, benchmark, coverage or static-analysis
  gates unless the user changes this scope.
- Run applicable GoogleTest units with exceptions OFF and RTTI OFF. Pull requests
  must pass all host unit-test jobs, validation-test jobs and formatting before
  merging into main.
- Preserve Apache-2.0 and the notices/immutable versions of incorporated material.
- Keep requirement review, implementation, test existence and executed results
  distinct. The initial build-setting units do not verify a parser or embedded runtime.

## Repository skills

Follow [ADR 0002: source layout](core/docs/adr-0002-public-private-source-layout.md)
for all new work and guidance: `includes/` contains supported public headers,
`core/includes/` contains private headers, and `.cpp` files under `core/src/` hold
substantial non-template implementations. Group files by topic. Internal units
live in `core/tests/`; public-interface/library units stay in top-level `tests/`
without private include access.
Guidance must follow the compiled architecture: read the corresponding header,
implementation and CMake source list; present declaration/source pairs when a
change needs both. Do not present substantial ordinary implementations as
header-only code for convenience. Templates and required compile-time definitions
remain legitimate header definitions. See the requirement guidance skill for
translation units, linking and dependency boundaries.
Public headers must not depend on the private include tree. The legacy
`include/adaptive_json5/` tree has been migrated; do not recreate it or a parallel
header-only implementation. Future production moves/splits follow the normal
restriction above.

For explanations, design guidance or review of a requirement, use
`.agents/skills/guide-requirement/SKILL.md`. For GoogleTest edits, use
`.agents/skills/write-unit-tests/SKILL.md`. Both are adapted from AdaptivePi (see
`NOTICE`); AdaptivePi's release approval, `ara::*` APIs, deployment and platform
policies do not apply here.

Use the adapted requirements and implementation sequence. Automated quality checks are unit
tests and formatting only. Keep generic/embedded parser dependencies separate
from optional schema, CLI, editor and consumer examples. Preserve Apache-2.0 and
record incorporated material's provenance. Do not mark planned functionality
implemented or verified without actual evidence.

The non-throwing error contract is mandatory: all library failures use explicit
result/error values in every supported compiler exception mode. Do not introduce
throwing error APIs or hidden exception-dependent recovery.

GoogleTest is acquired and built through CMake FetchContent for host unit tests
only. Keep the immutable revision and offline source override; tests-disabled
consumer builds must not acquire or link the host test framework.

For core parser work, follow the [official-specification implementation sequence](core/docs/requirements-json5-implementation-order.md).
P001–P066 are ordered so all prerequisites come earlier; existing J5 IDs remain
unchanged for issue and requirement traceability. Optional schema/editor/CLI work
does not block this core parser sequence.

The P001–P066 parser and optional-component checklist is the documented baseline
and requires no user review or approval before implementation work. Follow official
source semantics and marked project additions. Record implementation and unit
evidence separately; AI agents still do not change production code.

The sequential baseline now includes optional schema P042–P055, CLI/example
P056–P059 and VS Code P060–P066. Core parser P001–P041 remains independent.

## Current delivery and issue workflow

Follow [the delivery scope and issue tracking](core/docs/requirements-json5-implementation-order.md#delivery-scope-and-issue-tracking): retain full
schema P043–P055, minimal complete testing CLI P056–P059 and full advanced memory
accounting. Independently optional build flags do not remove those features from
this selected delivery scope. Editor/consumer acceptance remains in the backlog.
Reuse existing anchors, at most three active issues per subsystem. Verify current
assignment, comments, branch/PR and timeline activity before administrative closures;
redirect consolidated children and never imply implementation completion.

Romanr92 writes all production C++ and public headers. AI agents can change build
setup, documentation and unit tests, not production code. No separate per-issue documentation report, design narrative or
generic Documentation checkbox is required. Preserve essential public API,
ownership, errors, limits/defaults/accounting, cancellation, concurrency, provenance,
known deviations and brief usage contracts. Keep named tests and actual execution
results distinct from implementation and Draft/Verified status.
