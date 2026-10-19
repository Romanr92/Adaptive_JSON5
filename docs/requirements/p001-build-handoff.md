# P001: Independent C++17 build

Historical preparation-stage handoff. For current implementation placement and
the compiled target, follow [the architecture](../architecture.md),
[source layout](../source-layout.md) and [ADR 0002](../adr/0002-public-private-source-layout.md).
The starting-point paths/status below record the earlier scaffold and do not
authorize new header-only parser modules.

P001 is the first step in the [implementation sequence](json5-implementation-order.md).
J5-001 is the broader standalone-library contract: a documented public C++17 API,
native Windows/Linux/macOS portability and independence from application
frameworks or repository layout. Passing P001 configuration units alone cannot
complete J5-001. P001 has no prerequisites.

## Current starting point

- `CMakeLists.txt` provides `AdaptiveJSON5::build_options`, an interface target
  carrying the C++17 baseline, and a target-local compiler-policy helper.
- `tests/CMakeLists.txt` builds a separate GoogleTest executable. GoogleTest is
  pinned, host-only and acquired only when tests are enabled.
- The existing units check language level, exception mode and RTTI mode.
  Both exception configurations passed locally; see [execution evidence](../verification.md).
- `include/` and `src/` contain documentation only. There is no production
  library target, public header or parser to compile yet.

## Production handoff

The human developer owns public headers under `include/adaptive_json5/` and
implementation files under `src/`, as required by [AGENTS.md](../../AGENTS.md).
No placeholder parsing behavior is needed to establish the build foundation.

As the first real production component is introduced, connect its explicit
source list to the planned `AdaptiveJSON5::parser` target. CMake work can be
handled by the agent. Apply the existing target-local configuration helper,
publish the C++17 requirement and public include directory, and keep GoogleTest
linked only to the unit executable. The production target must also be available
when tests need it; the current test presets turn the library export/install
switch OFF. Preserve tests-disabled and subproject consumption.

Keep compiler extensions disabled and exception/RTTI policy target-local.
Compiler exceptions ON must not introduce a throwing library error API.
The core must not acquire OS services, optional schema/CLI/editor dependencies
or an implicit global heap; follow the [embedded boundary](../embedded.md).

The first C++ behavior to design is **P002, the non-throwing result foundation**:
success/error alternatives, a stable minimal error code and byte offset, with
allocation-free failure reporting. Choose its declarations before writing units
against them. P002 does not need a parser, storage allocator or syntax grammar.
Keep those later steps in their documented order.

See the [P002 result handoff](p002-result-handoff.md) for the first production
design and concrete unit acceptance cases.

## Evidence still needed

Once real headers/sources exist, compile their GoogleTest units with exceptions
OFF and ON and RTTI OFF. Public-header and production compilation evidence is
still pending, as is execution of the relevant native compiler matrix for those
files. Keep requirement status, test inventory and executed results separate.
