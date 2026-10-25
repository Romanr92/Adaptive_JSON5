---
name: write-unit-tests
description: Write or update Adaptive_JSON5 GoogleTest units with J5 traceability, boundary/failure coverage and suitable parameterization. Applies to test-only C++ and test build setup; never authorizes production C++ changes.
---

<!-- Adapted from Romanr92/adaptive-pi; changed for Adaptive_JSON5 native development and scope. See NOTICE. SPDX-License-Identifier: Apache-2.0. -->

# Write Adaptive_JSON5 GoogleTest units

Read `AGENTS.md`, relevant J5 entries in `docs/requirements.md`,
`docs/traceability.md`, the applicable ADRs and existing test/CMake conventions.
Tests may target a reviewed Draft contract without claiming it implemented.
Preserve normative wording/status; update only test inventory/evidence unless
the user explicitly requests a requirement change.

AI agents may edit GoogleTest code under `tests/`, `core/tests/` and test CMake. Do not repair
production C++ when a unit exposes a defect; describe the expected behavior,
failure and developer fix. Do not move production algorithms into test helpers.

## Coverage and build modes

- One J5 requirement may need several units; do not force one test per row.
  Cover decoded values, exact numbers, locations, ownership, explicit errors,
  mode distinctions and configured boundaries as the API becomes available.
- Keep common cases active with exceptions OFF and ON. Guard exception-only
  declarations and bodies at compile time; a runtime skip does not make `throw`
  or `try`/`catch` compile with exceptions disabled.
- Exercise no-RTTI builds and caller-owned storage/failure injection for embedded
  contracts. GoogleTest runs on the host and must never enter the parser runtime.
- Pinned syntax/schema corpora are deterministic unit fixture data, with upstream
  notices and explicit expected semantics. No external differential process,
  fuzzing, sanitizer, integration, benchmark or additional quality gate is required.
- Invalid parser input expects a structured error, not a death test. Only use a
  death test if a reviewed contract explicitly specifies termination.

## Parameterization and layout

Follow [ADR 0002](../../../core/docs/adr-0002-public-private-source-layout.md): group
new test files by topic. Internal units belong under `core/tests/`; public-header,
public API and whole-library behavior units belong under top-level `tests/` and
must use only public interfaces without private include access. Link the compiled
core when available, and grant private header access only to internal component
units. Both trees are host-only GoogleTest units, excluded from production sources
and installation. Split mixed internal/public test files as appropriate rather
than copying test bodies. The compiled target and layout now exist; inspect
actual paths and CMake before edits. The user authorized the completed structural
refactor specifically; future production moves/splits retain the normal
human-owned workflow unless separately authorized.

Follow [core/docs/design-coding-style.md](../../../core/docs/design-coding-style.md). Class data members,
including test fixture members, use `m_<varName>` with no trailing underscore,
for example `m_storage` rather than `storage_` or `m_storage_`. Keep existing
public aggregate field naming and the requirement-specific GoogleTest names.
clang-format controls layout, not identifier naming. Production naming fixes
remain the human developer's responsibility.

Use the coding guide's Doxygen blocks for meaningful test-support declarations
and put `//` reasoning comments immediately above non-obvious code. Preserve
the purpose/Arrange/Act/Expect comments required below for GoogleTest declarations
without adding duplicate documentation blocks.

Prefer parameterized units when inputs/expected results vary but setup and
assertions are shared. Keep distinct behaviors separate when branching would
obscure the assertion. Simple build-setting units need no artificial parameters.
Give cases descriptive names and useful printers/diagnostics. Add a short
trailing `//` comment to each parameter-case field describing input versus
expected output and units. Keep test-specific support beside its requirement
block; share helpers only when they represent real lifecycle/support behavior.

Use J5 IDs in suite names, for example `J5_016_StringEscapes.DecodesNonEscape`.
Group units with labeled C-style requirement delimiters, substituting `J5_016`:
`Test_J5_016` and `End Test_J5_016`. Pad each delimiter with `=` to 89 characters
from opening `/` to closing `/`; use an 89-character `-` separator between tests.
Lifecycle fixtures, when needed, appear after includes/namespaces and before
requirement blocks in labeled Fixtures/End Fixtures delimiters. Explain setup
and cleanup immediately above each lifecycle fixture. Omit artificial fixtures.

Immediately above each TEST/TEST_F/TEST_P, use a C-style block comment describing
the purpose and concrete **Arrange**, **Act**, **Expect** steps. Inside the body
use `/* Arrange */`, `/* Act */`, `/* Expect */` in that order, each before its
section's statements. Keep the actual operation/assertions visible in the test.
Requirement-specific compile-time checks belong within that requirement block.

## Traceability and execution

Update the separate **Unit Tests** inventory in `docs/traceability.md` with exact
`SuiteName.TestName` declarations, separated by `; ` and without generated case
suffixes. Leave it empty where no units exist. Keep planned verification in
`docs/requirements.md`; test existence is not a pass or complete coverage.

Record executed configuration/result separately. Run the affected GoogleTest
suite with exceptions OFF and ON when available, and run clang-format 18.
Report a missing compiler/dependency/CI result honestly. Do not infer parser,
schema, editor-host or embedded target support from configuration units.

Adapted from AdaptivePi's unit-test workflow; provenance is in
`docs/dependencies.md`. Its `ara::*` cases, release gates and CI-only enabled-mode
execution are deliberately omitted for this generic native repository.

Library error reporting is non-throwing in every supported build. Test explicit
result categories for failures, including allocation/limits; do not add throwing
error APIs. Embedded guidance is OS-agnostic, with MICROSAR only an example and
no dedicated vendor port or workflow required.
