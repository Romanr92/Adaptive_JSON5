# Adaptive_JSON5

[![Unit tests](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/unit-tests.yml)
[![Formatting](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/formatting.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/formatting.yml)

A portable C++17 JSON5 reader for compatible C++ projects, including
hosted systems and embedded devices after compilation into a compatible project.
The core is OS-agnostic; MICROSAR is one possible environment, not a required vendor port.
The baseline covers full JSON5 1.0, selectable strict JSON, owned values, exact
numbers, explicit recoverable errors and configurable resource limits. Draft-07
validation, a CLI and VS Code support are independently optional components.

**Status: the compiled C++17 core parses complete JSON5 1.0.0 and strict JSON
documents through `parse_document()`, with exact numeric conversion and
comparison, warnings, line/column lookup and optional source retention
(P001–P040).** The P041 technical review is recorded; final claim status and
required PR evidence are in the
[JSON5 1.0.0 support statement](docs/conformance.md), together with versions,
limits, implementation-defined behaviour, deviations and executed results. Requirement
rows remain Draft.
GoogleTest inventories are in [traceability](docs/traceability.md), and the
per-production view is the [grammar evidence matrix](core/docs/design-grammar-evidence.md).
Production C++ and public headers are reserved for Romanr92; AI creates
unit tests. See [AGENTS.md](AGENTS.md).

The [delivery scope](core/docs/requirements-json5-implementation-order.md#delivery-scope-and-issue-tracking) retains full schema
P043–P055, a minimal testing CLI P056–P059 and full advanced memory accounting.
Existing anchors group consecutive steps with at most three issues per subsystem.
Schema/CLI remain optional for consumers, while required in this delivery scope.
No per-issue documentation report is required; essential public API, limits,
ownership, provenance and usage contracts remain. Ten working days is a checkpoint,
not a credible full-completion promise; the plan states estimate assumptions.

Native development requires no WSL. The parser
requires neither AdaptivePi, an operating-system service, Node.js, VS Code nor
network access. [Embedded requirements](docs/embedded.md) define the bare-metal
boundary. AdaptivePi is inspiration and an optional consumer example.

## Start development

Use CMake 3.25+, a compatible C++17 compiler and GoogleTest 1.17.0 for host units.
CMake downloads and builds the pinned host-only GoogleTest dependency on the first test configuration:

From PowerShell, run these commands in the repository directory:

```powershell
cmake --preset debug-unit-tests
cmake --build --preset debug-unit-tests
ctest --preset debug-unit-tests
```

Run the same units with exceptions ON:

```powershell
cmake --preset debug-unit-tests-exceptions
cmake --build --preset debug-unit-tests-exceptions
ctest --preset debug-unit-tests-exceptions
```

If the selected preset is already configured and built, run only its `ctest`
command. To run just the P002 result suite:

```powershell
ctest --preset debug-unit-tests -L "^result_units$" --output-on-failure
```

Add `--verbose` to a `ctest` command to see each GoogleTest case.
CTest discovers each individual GoogleTest case, including parameterized cases,
so its final total counts cases. Suite labels such as `result_units` select groups.

Exceptions and RTTI default OFF; the second preset enables exceptions. Run
Windows commands in a Visual Studio developer terminal when using its compiler.
See [native development](core/docs/design-development.md) for offline GoogleTest, formatting
and toolchain configuration. Automated checks are **GoogleTest unit tests and
clang-format 18 only**. GoogleTest is host-only and never a parser runtime dependency.

## CMake consumption

The source/install package exports the compiled static `AdaptiveJSON5::parser`
target and the supporting `AdaptiveJSON5::build_options` metadata target.
Consumers receive only the public `includes/` search root. The core currently
exposes owned document/value/storage components and the public `parse_document`
entry; the optional schema target remains later work.

```cmake
add_subdirectory(path/to/Adaptive_JSON5)
target_link_libraries(my_application PRIVATE AdaptiveJSON5::parser)
```

Tests default OFF as a subproject. A tests-disabled compiled package can be
configured, built and installed without GoogleTest:

```sh
cmake --preset library
cmake --build --preset library
cmake --install build/library --config Release --prefix path/to/install
```

Before calling `parse_document`, read [memory, limits and failure behavior](docs/memory-and-limits.md).
It covers the storage you supply, what each limit counts, what happens when a
parse fails and how to retry, and decided changes that are not yet implemented.

Use `find_package(AdaptiveJSON5 0.1 CONFIG REQUIRED)` with the installation prefix
in `CMAKE_PREFIX_PATH`. The current version identifies the development package,
not a supported parser release.

Release packaging builds GCC static libraries for Linux x86-64, Linux ARM64,
Windows MinGW x86-64 and the pinned AdaptivePi ARM64 Yocto SDK. Each archive
contains the `.a` beside its public `include/` folder, exported CMake targets,
licenses and source/toolchain metadata. See [release packages](docs/release-packages.md)
for pinned downloads, checksums, offline use and the consumer CMake import helper.
macOS release binaries are outside this packaging scope.

## Documentation and work

The implemented [source layout guide](core/docs/design-source-layout.md) and
[layout decision](core/docs/adr-0002-public-private-source-layout.md) separate
public `includes/` from private `core/includes/`, with files grouped by topic.
Compiled implementation files live under `core/src/<topic>/`.
Internal units live under `core/tests/`; public-header/API and whole-library
behavior units stay under top-level `tests/`. This is the direction for new work;
the former `include/` layout has been migrated. Internal declarations are not
installed, and public headers compile without the private include root.

The [examples folder](examples/README.md) contains JSON/JSON5 inputs, intentional
failures, an optional schema and a planned requirement demonstration manifest.
Enable `ADAPTIVE_JSON5_BUILD_EXECUTABLE` to expose its data target in CMake.
The command-line executable will be implemented later; no production C++ is added.

- [Original proposal Markdown](core/docs/requirements-proposal-json5-parser-repository.md): preserved Draft input, superseded where later user decisions differ.
- [Active requirements](docs/requirements.md): J5-001–101, including embedded portability.
- [Implementation sequence and issue tracking](core/docs/requirements-json5-implementation-order.md) and [traceability](docs/traceability.md): ordered issues, prerequisites and unit evidence.
- [Memory, limits and failure behavior](docs/memory-and-limits.md) for library users, and [storage and budget decisions](core/docs/adr-0003-storage-and-budget-decisions.md) (ADR 0003).
- [Architecture](core/docs/design-architecture.md), [embedded boundary](docs/embedded.md) and [scope ADR](docs/adr/0001-project-scope.md).
- [Contributing](CONTRIBUTING.md), [agent constraints](AGENTS.md) and adapted repository skills in `.agents/skills/`.
- [Dependencies/provenance](docs/dependencies.md) and [optional consumer example](docs/consumer-example.md).

## License

Copyright 2026 Razvan-Stefan Roman.

[Apache-2.0](LICENSE). Incorporated dependencies, Unicode data and fixtures retain
their own applicable notices and recorded versions.

See [licensing and redistribution](docs/licensing.md) and [NOTICE](NOTICE) for
attribution and distribution obligations.

For core parser work, follow the [official-specification implementation sequence](core/docs/requirements-json5-implementation-order.md).
P001–P066 are ordered so all prerequisites come earlier; existing J5 IDs remain
unchanged for issue and requirement traceability. Optional schema/editor/CLI work
does not block this core parser sequence.

The sequential baseline now includes optional schema P042–P055, CLI/example
P056–P059 and VS Code P060–P066. Core parser P001–P041 remains independent.
