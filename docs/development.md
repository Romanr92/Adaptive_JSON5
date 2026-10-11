# Native development and GoogleTest

Use CMake 3.25+ and a compatible C++17 compiler. Windows: Visual Studio Build
Tools with the C++ workload/SDK. Linux: GCC or Clang. macOS: Xcode Command Line
Tools. Ninja is optional. No WSL or consumer SDK is needed for host development.

## Test dependency

CMake FetchContent downloads and builds GoogleTest 1.17.0 at immutable revision
`52eb8108c5bdec04579160ae17225d66034bd723` into `tools/gtest` when host tests
are enabled. The first configuration requires Git and network access; subsequent
configurations reuse the checkout without checking for updates.

For offline work, supply an existing 1.17.0 checkout through
`-DADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR=path/to/googletest`. Relative paths are
resolved from the root source directory. Tests-disabled builds neither acquire
nor build GoogleTest. GoogleTest is not installed with the library.

## Unit commands

```sh
cmake --preset debug-unit-tests
cmake --build --preset debug-unit-tests
ctest --preset debug-unit-tests
cmake --preset debug-unit-tests-exceptions
cmake --build --preset debug-unit-tests-exceptions
ctest --preset debug-unit-tests-exceptions
```

The initial three GoogleTest cases check C++17, exception and RTTI build settings.
They do not establish parser semantics. Add requirement-backed units as the human
developer implements the production API. The adapted
[write-unit-tests skill](../.agents/skills/write-unit-tests/SKILL.md) explains
traceability, parameterization and Arrange/Act/Expect comments.

| Option | Default | Purpose |
| --- | --- | --- |
| `ADAPTIVE_JSON5_BUILD_TESTS` | ON at root; OFF as subproject | Host GoogleTest units |
| `ADAPTIVE_JSON5_ENABLE_EXCEPTIONS` | OFF | Exception mode for selected project targets |
| `ADAPTIVE_JSON5_ENABLE_RTTI` | OFF | RTTI mode for selected project targets |
| `ADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR` | Empty | Offline source override; otherwise CMake FetchContent |

C++17 and compiler options apply to selected targets. Parent flags are not
overwritten. Future production targets must use the reviewed exception/runtime
policy; a configurable test build is not evidence of an implemented parser.

## VS Code

The tracked `.vscode/` setup is adapted from AdaptivePi. Install the recommended
extensions, open this repository folder and select the `debug-unit-tests` CMake preset and
your native compiler. CMake Tools supplies IntelliSense and the executable path.
Tasks provide configure, build and unit tests for both exception modes, plus
formatting check/fix. Formatting on save uses `clang-format` on PATH; install
version 18 and override `clang-format.executable` in your user settings if needed.

Select `adaptive_json5_unit_tests` as the CMake launch target before debugging.
Use the Windows configuration for MSVC, or the Linux/macOS configuration with
GDB/LLDB installed. Choose the matching `debug-unit-tests` build preset in CMake Tools. For a
single case, add `--gtest_filter=SuiteName.TestName` to the launch arguments.
Build tasks configure through CMake, including the host-only GoogleTest fetch.
Keep personal toolchain paths in user settings or `CMakeUserPresets.json`.

## Formatting commands

Use Python 3.9+ and clang-format 18:

```sh
python -m pip install clang-format==18.1.8
python tools/format.py --check
python tools/format.py --fix
```

Use `--clang-format path/to/executable` if needed. Substitute `python3` on systems
using that command. The helper excludes generated, build and third-party files.

## Package and embedded configuration

The tests-disabled `library` preset exports only development metadata today.
Install it with `cmake --install build/library --config Release --prefix path/to/install`.
Future issues add the implemented parser/schema targets and their public headers.
Schema, CLI and editor directories are future components, not available options.

Embedded builds use a caller-owned CMake toolchain file and
`-DADAPTIVE_JSON5_BUILD_TESTS=OFF`; the core must not depend on host GoogleTest.
See [embedded requirements](embedded.md). Embedded compiler/runtime and device-specific execution remain unverified until
the consuming project supplies them. No particular SDK or vendor port is mandated.

GitHub Actions provides the same host GoogleTest suite on Windows/MSVC,
Linux/GCC and macOS/Clang with exceptions OFF/ON and RTTI OFF, plus formatting.
No integration, fuzz, sanitizer, benchmark, coverage or static-analysis gate is added.

## Component switches and bottom toolbar

Three independent component switches select what to prepare/build:

| Switch | Default | Current scaffold behavior |
| --- | --- | --- |
| `ADAPTIVE_JSON5_BUILD_LIBRARY` | ON | Export/install development library metadata; parser C++ pending |
| `ADAPTIVE_JSON5_BUILD_EXECUTABLE` | OFF | Example data and future `examples/main.cpp` target; executable C++ pending |
| `ADAPTIVE_JSON5_BUILD_TESTS` | ON at root, OFF as subproject | Fetch/build/run host GoogleTest units |

Select `Debug: Library`, `Release: Library`, `Debug: Example executable`,
`Debug: Unit Tests` or `Debug: Unit Tests (exceptions ON)` in the bottom CMake
configuration-preset control. Then select its matching build preset and press
Build. Test presets are provided for unit configurations. CMake Tools must be
installed/enabled and the repository opened as a folder. If controls are hidden,
run **CMake: Select Configure Preset** once and check that VS Code's status bar is
shown. Tracked settings keep configure/build/test presets and actions visible.

Native generator/compiler selection follows your environment; no Linux compiler,
Ninja, WSL or SDK is hardcoded. Use personal presets for toolchain overrides.
The `dev`, `dev-exceptions` and `library` aliases keep earlier commands working.
Example/library selections currently configure infrastructure only and explicitly
report the missing executable; they do not produce a working parser or demo.
When both library and tests are OFF, no install package or GoogleTest is produced.
The metadata interface remains available internally for test build settings.
