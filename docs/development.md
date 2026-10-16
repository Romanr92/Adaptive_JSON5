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
```

The initial three GoogleTest cases check C++17, exception and RTTI build settings.
The separate `adaptive_json5_result_unit_tests` executable supplies 40 P002
result/error cases under the `result_units` CTest label. The storage executable
supplies 39 cases under `storage_units`, and `adaptive_json5_api_unit_tests`
supplies 16 input/options/cursor cases under `api_configuration_units`.
CTest discovers individual GoogleTest cases, including parameterized cases,
and reports 98 cases in the current build. Use `-L "^result_units$"` to select
the result group, or `-R` to match individual case names.
These do not establish parser semantics.
Add requirement-backed units as the human developer implements
the production API. The adapted
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

All project C++ targets, including unit-test executables, treat compiler warnings
as errors through `adaptive_json5_configure_target()`: `-Wparentheses -Werror`
for GCC/Clang/AppleClang, and `/W4 /WX /w14706` for MSVC-compatible builds.
The flags are private to project targets and do not alter GoogleTest targets,
parent-project flags or exported consumer options. Both exception presets use
the same warning policy. See [coding guidelines](coding-style.md).

## VS Code

The tracked `.vscode/` setup is adapted from AdaptivePi. Install the recommended
extensions, open this repository folder and select the `debug-unit-tests` CMake preset and
your native compiler. CMake Tools supplies IntelliSense and the executable path.
Tasks provide configure, build and unit tests, plus
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

## Optional local coverage report

For a manually requested GCC/gcovr report, use Python 3.10+, CMake, Ninja and
GCC with its matching `gcov` on PATH:

```sh
python -m pip install gcovr==8.6
python tools/coverage.py
```

If gcovr is installed in a virtual environment, run the helper with that
environment's Python. For example, the local verification environment on Windows
can be reused with `build/coverage-tools/Scripts/python.exe tools/coverage.py`.

Open `build/coverage/report/index.html` for annotated source reports. The helper
uses a separate Debug build, runs the existing host units with exceptions and
RTTI OFF, and clears only that build's old runtime counters before execution.
Reports include common C/C++ source, header, inline/template and module files
(`.c`, `.cc`, `.cp`, `.cpp`, `.cxx`, `.c++`, `.h`, `.hh`, `.hp`, `.hpp`, `.hxx`,
`.h++`, `.inl`, `.inc`, `.ipp`, `.tpp`, `.tcc`, `.txx`, `.ixx`, `.cppm`, `.cxxm`,
`.ccm`, `.c++m`), including uppercase variants. `tests/` is excluded.
Entries are requested even for files without instrumentation data. Build/output,
generated and third-party directories (including `tools/gtest`) are excluded.
Use `--compiler PATH`, `--gcov PATH`, `--jobs N` or `--gtest-source PATH` when needed;
the last option retains the existing offline GoogleTest override. GCC and gcov
must come from the same toolchain. This is an optional local diagnostic, with no
coverage threshold, CI job or additional merge gate. Coverage does not establish
complete requirement verification. Entries without instrumentation data do not
provide executable-line or branch information and must not be interpreted as
proof that those files were exercised.

## Package and embedded configuration

The tests-disabled `library` preset exports only development metadata today.
Install it with `cmake --install build/library --config Release --prefix path/to/install`.
Future issues add the implemented parser/schema targets and their public headers.
Schema, CLI and editor directories are future components, not available options.

Embedded builds use a caller-owned CMake toolchain file and
`-DADAPTIVE_JSON5_BUILD_TESTS=OFF`; the core must not depend on host GoogleTest.
See [embedded requirements](embedded.md). Embedded compiler/runtime and device-specific execution remain unverified until
the consuming project supplies them. No particular SDK or vendor port is mandated.

GitHub Actions runs one host GoogleTest job on Ubuntu with exceptions OFF and
RTTI OFF, plus one formatting job, only for pull requests. Updating a pull request
cancels its older in-progress runs.
Both unit-tests and formatting are required by the prepared
[main branch ruleset](branch-protection.md) once activated in repository settings.
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
