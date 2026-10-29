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
(P001–P040).** A simple API, `json5_document` with `json5_value`, parses into a
caller buffer and is reachable through one umbrella header (see the quick start below).
The P041 technical review is recorded; final claim status and
required PR evidence are in the
[JSON5 1.0.0 support statement](docs/conformance.md), together with versions,
limits, implementation-defined behaviour, deviations and executed results. Requirement
rows remain Draft.
GoogleTest inventories are in [traceability](docs/traceability.md), and the
per-production view is the [grammar evidence matrix](core/docs/design-grammar-evidence.md).
Production code (C++ sources and public headers) is written by Romanr92. AI agents
can change build setup, documentation and unit tests, not production code. See
[AGENTS.md](AGENTS.md).

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

## Quick start

Include one header, link one library and parse into memory you provide:

```cpp
#include "adaptive_json5/adaptive_json5.hpp"

#include <cstdio>

static unsigned char memory[16384];   // the parsed document lives here; no heap is used

void read_config(const char* text, std::size_t size)
{
  const auto parsed = adaptive_json5::json5_document::parse({text, size}, memory, sizeof(memory));
  if (!parsed.has_value())
  {
    char message[128];
    const auto described = adaptive_json5::describe(*parsed.error_if(), {text, size}, message);
    std::puts(described.has_value() ? message : "invalid input");   // "line: 1, column: 5: ..."
    return;
  }

  const auto port = parsed.value_if()->root().member("port").as_int64();
  if (port.has_value())
  {
    std::printf("port %lld\n", static_cast<long long>(*port.value_if()));
  }
}
```

```cmake
target_link_libraries(my_application PRIVATE AdaptiveJSON5::parser)
```

- Every call returns a `result`: check `has_value()` before `value_if()`. Errors are
  values, never exceptions.
- `member()` and `at()` can be chained; a missing member or a wrong type is reported
  by the final read.
- `memory` must stay alive, and must not be used for anything else, while the
  document exists.

The [user guide](docs/memory-and-limits.md#simple-api-json5_document-current-behavior)
covers buffer sizing, warnings, limits and the advanced API. On desktop or server
systems the heap can replace the buffer; see the
[memory switch](#memory-switch-adaptive_json5_build_hosted).

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
Consumers receive only the public `includes/` search root. The default include is
`adaptive_json5/adaptive_json5.hpp`, which contains the whole embedded API: the
simple `json5_document` and the advanced owned document/value/storage components
with `parse_document`. The optional schema target remains later work.

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

Before parsing, read [memory, limits and failure behavior](docs/memory-and-limits.md).
It covers the simple API, the storage you supply, what each limit counts, what
happens when a parse fails and how to retry.

### Memory switch: `ADAPTIVE_JSON5_BUILD_HOSTED`

The CMake option `ADAPTIVE_JSON5_BUILD_HOSTED` decides whether the library may use
the global heap. It is **OFF by default**.

| | OFF (default) | ON |
|---|---|---|
| Libraries | `AdaptiveJSON5::parser` | `AdaptiveJSON5::parser` and `AdaptiveJSON5::hosted` |
| Static library files (GCC/Clang) | `libadaptive_json5_parser.a` | `libadaptive_json5_parser.a` and `libadaptive_json5_hosted.a` |
| Memory | a caller buffer (`json5_document::parse`) or your storage callbacks (`parse_document`) | additionally the heap, through `hosted::parse` |
| Headers installed | all public headers except `adaptive_json5/hosted/` | all public headers |
| Suits | embedded devices and any target without a reliable heap | desktop and server applications |

Both modes return the same `json5_document`, and every limit applies in both.

**Activate it**

1. Configure with the option ON, then build and install:

   ```sh
   cmake -S . -B build/hosted -DADAPTIVE_JSON5_BUILD_HOSTED=ON
   cmake --build build/hosted
   cmake --install build/hosted --prefix path/to/install
   ```

   As a subproject, set it before `add_subdirectory`:

   ```cmake
   set(ADAPTIVE_JSON5_BUILD_HOSTED ON CACHE BOOL "" FORCE)
   add_subdirectory(path/to/Adaptive_JSON5)
   ```

2. Link `AdaptiveJSON5::hosted`. It brings `AdaptiveJSON5::parser` with it:

   ```cmake
   target_link_libraries(my_application PRIVATE AdaptiveJSON5::hosted)
   ```

3. Add the hosted include next to the umbrella header and parse in one call:

   ```cpp
   #include "adaptive_json5/adaptive_json5.hpp"
   #include "adaptive_json5/hosted/parse.hpp"

   const auto parsed = adaptive_json5::hosted::parse({text, size});   // heap storage
   ```

4. Check the installation: `include/adaptive_json5/hosted/parse.hpp` and
   `lib/libadaptive_json5_hosted.a` exist under the prefix, and after
   `find_package(AdaptiveJSON5 CONFIG REQUIRED)` the condition
   `if(TARGET AdaptiveJSON5::hosted)` is true.

**Deactivate it**

1. Configure with `-DADAPTIVE_JSON5_BUILD_HOSTED=OFF`, or remove the `set(...)` line.
   An existing build directory keeps the cached value, so pass `OFF` explicitly or
   configure a fresh directory (`cmake --fresh`).
2. Link `AdaptiveJSON5::parser` instead of `AdaptiveJSON5::hosted`.
3. Remove the hosted include and replace `hosted::parse(input)` with
   `json5_document::parse(input, buffer, sizeof(buffer))`. The result is the same
   `json5_document`, so the code that reads it stays unchanged.

A leftover hosted include then fails at compile time, because the header is not
installed. Building the unit tests compiles the hosted library for its own tests
even when the option is OFF; it is installed only when the option is ON.

**Which to choose:** keep it OFF for embedded targets, for code that must bound its
memory up front, and whenever no reliable heap exists. Turn it ON for desktop and
server tools that prefer one call without managing a buffer.

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
- [Simple API, umbrella header and memory switch](docs/adr/0004-simple-api-and-umbrella-header.md) (ADR 0004).
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
