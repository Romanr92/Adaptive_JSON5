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
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --preset dev-exceptions
cmake --build --preset dev-exceptions
ctest --preset dev-exceptions
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

## Formatting

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
