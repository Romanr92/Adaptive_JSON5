# Adaptive_JSON5

[![Unit tests](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/unit-tests.yml)
[![Formatting](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/formatting.yml/badge.svg)](https://github.com/Romanr92/Adaptive_JSON5/actions/workflows/formatting.yml)

A planned portable C++17 JSON5 reader for compatible C++ projects, including
hosted systems and embedded devices after compilation into a compatible project.
The core is OS-agnostic; MICROSAR is one possible environment, not a required vendor port.
The baseline covers full JSON5 1.0, selectable strict JSON, owned values, exact
numbers, explicit recoverable errors and configurable resource limits. Draft-07
validation, a CLI and VS Code support are independently optional components.

**Status: P002 result/error foundation implemented; no parser API is available.**
The developer's public headers provide minimal errors and value/status results.
GoogleTest covers their behavior and the build settings; local exceptions
OFF/ON results are recorded in [verification notes](docs/verification.md).
The repository also supplies CMake/package scaffolding, formatting,
documentation and an ordered issue backlog. Parser conformance remains pending.
Production C++ implementation is reserved for the developer; see [AGENTS.md](AGENTS.md).

Native Windows, Linux and macOS development requires no WSL. The planned parser
will require neither AdaptivePi, an operating-system service, Node.js, VS Code nor
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
ctest --preset debug-unit-tests -R "^result_units$" --output-on-failure
```

Add `--verbose` to a `ctest` command to see each GoogleTest case.

Exceptions and RTTI default OFF; the second preset enables exceptions. Run
Windows commands in a Visual Studio developer terminal when using its compiler.
See [native development](docs/development.md) for offline GoogleTest, formatting
and toolchain configuration. Automated checks are **GoogleTest unit tests and
clang-format 18 only**. GoogleTest is host-only and never a parser runtime dependency.

## CMake consumption scaffold

The current source/install package exports `AdaptiveJSON5::build_options`, an
interface target expressing the C++17 development baseline. It contains no parser
and should not be mistaken for a working JSON5 library. Future implementation
issues will add `AdaptiveJSON5::parser` and an optional schema target.

```cmake
add_subdirectory(path/to/Adaptive_JSON5)
# Current development metadata only:
target_link_libraries(my_application PRIVATE AdaptiveJSON5::build_options)
```

Tests default OFF as a subproject. A tests-disabled metadata package can be
configured, built and installed without GoogleTest:

```sh
cmake --preset library
cmake --build --preset library
cmake --install build/library --config Release --prefix path/to/install
```

Use `find_package(AdaptiveJSON5 0.1 CONFIG REQUIRED)` with the installation prefix
in `CMAKE_PREFIX_PATH`. The current version identifies the development package,
not a supported parser release.

## Documentation and work

The [examples folder](examples/README.md) contains JSON/JSON5 inputs, intentional
failures, an optional schema and a planned requirement demonstration manifest.
Enable `ADAPTIVE_JSON5_BUILD_EXECUTABLE` to expose its data target in CMake.
The command-line executable will be implemented later; no production C++ is added.

- [Original proposal Markdown](docs/requirements/proposals/json5-parser-repository.md): preserved Draft input, superseded where later user decisions differ.
- [Active requirements](docs/requirements.md): J5-001–101, including embedded portability.
- [Roadmap](docs/roadmap.md) and [traceability](docs/traceability.md): ordered issues, prerequisites and unit evidence.
- [Architecture](docs/architecture.md), [embedded boundary](docs/embedded.md) and [scope ADR](docs/adr/0001-project-scope.md).
- [Contributing](CONTRIBUTING.md), [agent constraints](AGENTS.md) and adapted repository skills in `.agents/skills/`.
- [Dependencies/provenance](docs/dependencies.md) and [optional consumer example](docs/consumer-example.md).

## License

Copyright 2026 Razvan-Stefan Roman.

[Apache-2.0](LICENSE). Incorporated dependencies, Unicode data and fixtures retain
their own applicable notices and recorded versions.

See [licensing and redistribution](docs/licensing.md) and [NOTICE](NOTICE) for
attribution and distribution obligations.

For core parser work, follow the [official-specification implementation sequence](docs/requirements/json5-implementation-order.md).
P001–P066 are ordered so all prerequisites come earlier; existing J5 IDs remain
unchanged for issue and requirement traceability. Optional schema/editor/CLI work
does not block this core parser sequence.

The sequential baseline now includes optional schema P042–P055, CLI/example
P056–P059 and VS Code P060–P066. Core parser P001–P041 remains independent.
