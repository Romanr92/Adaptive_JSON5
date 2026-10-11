# Repository agent scope

## Source of truth

Read `README.md`, `docs/requirements.md`, `docs/traceability.md` and the relevant
ADR/skill before requirement, design or unit-test work. Use active J5 contracts,
their status, dependencies and documented deviations. The proposal under
`docs/requirements/proposals/` is preserved Draft source material; later user
instructions and the adapted requirements supersede conflicting proposal text.
Do not silently change a requirement or invent implementation evidence.

## Authorized work and constraints

The user permits CMake/build setup, GoogleTest unit tests, documentation,
requirements and repository/issue organization. **Do not create or modify
production C++ implementation or public C++ headers.** Production implementation
belongs to a human developer unless the user explicitly changes this instruction.

Test-only C++ under `tests/` is permitted. Do not disguise production logic as
test helpers or generated code to bypass the restriction.

- Prepare or edit CMake, GoogleTest units, formatting, documentation, requirements,
  ADRs, repository skills and the authorized issue backlog.
- When asked to implement or fix production C++, explain the approach and guide
  the developer. Test failures do not authorize an agent to fix production files.
- Design the core as a generic C++17 library for compatible hosted projects and
  embedded devices after compilation into a compatible project. The core is
  OS-agnostic; MICROSAR is only an example, not a required vendor port. Read `docs/embedded.md` and J5-101 for runtime/storage boundaries.
- Do not require WSL, AdaptivePi, an OS service, SDK, Node.js, VS Code or host test
  dependencies in the parser. Schema, CLI, editor and examples remain optional.
- Unit tests and formatting are the only automated quality checks. Do not add
  integration, end-to-end, fuzz, sanitizer, benchmark, coverage or static-analysis
  gates unless the user changes this scope.
- Run applicable GoogleTest units with exceptions OFF and ON; keep common
  behavior tests active in both and compile-check the no-RTTI build policy.
- Preserve Apache-2.0 and the notices/immutable versions of incorporated material.
- Keep requirement review, implementation, test existence and executed results
  distinct. The initial build-setting units do not verify a parser or embedded runtime.

## Repository skills

For explanations, design guidance or review of a requirement, use
`.agents/skills/teach-requirement/SKILL.md`. For GoogleTest edits, use
`.agents/skills/write-unit-tests/SKILL.md`. These are adaptations of AdaptivePi's
workflows, rewritten for this repository; its release-wide approval, `ara::*`
APIs, deployment and platform policies do not apply here.

Use the adapted requirements and roadmap. Automated quality checks are unit
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

For core parser work, follow the [official-specification implementation sequence](docs/requirements/json5-implementation-order.md).
P001–P041 are ordered so all prerequisites come earlier; existing J5 IDs remain
unchanged for issue and requirement traceability. Optional schema/editor/CLI work
does not block this core parser sequence.
