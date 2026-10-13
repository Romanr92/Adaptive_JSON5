# Development setup verification

## Issue #1 input/options declarations, 2026-10-13

The developer's `input.hpp` and `options.hpp` define borrowed immutable input
and JSON5/strict-JSON selection with a JSON5 default. The existing
[architecture contract](architecture.md#input-and-syntax-options-contract)
documents exact byte lengths, caller lifetimes, non-NUL input, embedded zero
bytes, empty descriptions and explicit result-state construction.

Five `J5_004_InputView` units and three `J5_005_ParseOptions` units execute in
the new `adaptive_json5_api_unit_tests` binary (`api_configuration_units` CTest
entry). They verify declaration behavior only: no parser reads, syntax
acceptance, malformed/truncated-input handling or read-boundary implementation
is claimed. P004's cursor and P042's strict grammar remain pending.

Both native Windows/GCC 15.2.0 C++17 configurations passed: exceptions OFF/ON,
RTTI OFF, with 51 GoogleTest cases per mode (8 API, 40 result, 3 configuration)
in three CTest entries. Compile-time checks also ensure result types have no
public default construction. Both new headers independently passed first/only
include compilation with `-pedantic-errors` in both modes. clang-format 18.1.8
passed for all eight C++ files; the agent formatted test sources only.

This completes Issue #1's input/defaults specification and available-declaration
unit acceptance item. It does not establish full J5-004/J5-005 behavior or the
native compiler matrix. Requirement statuses remain Draft.

## Issue #1 recoverable-error specification, 2026-10-13

The [architecture error contract](architecture.md#recoverable-error-contract)
now specifies equivalent outcomes in exceptions OFF/ON builds, category mapping,
checked inspection, allocation-free minimal reporting, no invalid-input
assert/abort/throw, cleanup and failure atomicity. This is completion of an API
specification acceptance item, not executed evidence for future parser,
conversion, access or storage operations. Existing P002 execution results apply
to result containers only. No production changes or additional test runs were
needed for this documentation change; linked J5 statuses remain Draft.

## Issue #1 current public-header review, 2026-10-13

Reviewed `error.hpp` and `result.hpp` for standard C++17 declarations and absence
of OS, vendor, AdaptivePi, WSL, host-test and editor dependencies. Each header
passed an independent first/only-include compile check with native Windows GCC
15.2.0, `-std=c++17 -pedantic-errors -fno-rtti`, in exceptions OFF and ON modes.
This evidence covers the current header inventory only; the full native compiler
matrix, embedded runtime and broader J5-001 verification remain pending. No
production files changed, no new quality gate was added and broader J5 statuses
remain Draft.

## P002 result/error units, 2026-10-13

The developer's `error.hpp` and `result.hpp` now provide minimal errors,
`result<T, E>`, `result<void, E>` and the `status_result` alias. The agent added
only test C++, test build setup and documentation for this verification step.

`tests/result_tests.cpp` contains 14 GoogleTest declarations expanding to 40
executed cases. Coverage includes both success/error alternatives, const/mutable
inspection, all seven stable codes at byte zero, byte 17 and the maximum
`std::size_t` offset, missing-location distinction, copy/move construction,
move-only ownership transfer, identical value/error types, status assignment
and balanced destruction of nontrivial active alternatives.

Compile-time checks cover the public non-throwing operations, pointer types,
move-only copy prohibition and disabled value-result assignment. Allocation
failure reporting is also constructed and inspected in C++17 constant
evaluation, where this path cannot dynamically allocate. This is evidence for
minimal reporting, not allocator-exhaustion injection into a parser or proof
that arbitrary custom error types never allocate.

| Local native Windows configuration | Executed result |
| --- | --- |
| GCC 15.2.0, C++17, exceptions OFF, RTTI OFF | Both CTest entries passed: 40 result cases and 3 configuration cases |
| GCC 15.2.0, C++17, exceptions ON, RTTI OFF | The same 43 GoogleTest cases passed |
| clang-format 18.1.8 | Passed for all 4 C++ files; only the new test source was formatted by the agent |

Both configurations used Ninja, CMake 4.4.4 and the pinned GoogleTest revision
with `-DADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR=tools/gtest`. Reproduce with
`cmake --build --preset debug-unit-tests` and `ctest --preset debug-unit-tests`,
then the corresponding `debug-unit-tests-exceptions` commands. The new CTest
entry is `result_units`, running `adaptive_json5_result_unit_tests`.

P002's current result/error component has local executed unit evidence. Broader
J5-003, J5-043 and J5-045 contracts remain Draft because their parser, access,
conversion and resource-failure behavior is not implemented here. No new remote
compiler matrix or embedded target execution was performed.

## P001 starting point, 2026-10-13

Reviewed P001 (independent C++17 build) and its J5-001, J5-002, J5-091 and
J5-101 links. No production files or requirement statuses changed.

Local native Windows results with GCC 15.2.0, Ninja, CMake 4.4.4 and GoogleTest
at `52eb8108c5bdec04579160ae17225d66034bd723`:

| Configuration | Executed result |
| --- | --- |
| `debug-unit-tests`, exceptions OFF, RTTI OFF | Build passed; all three configuration units listed below passed |
| `debug-unit-tests-exceptions`, exceptions ON, RTTI OFF | Build passed; the same three units passed |
| `debug-library`, tests OFF | Configure/build passed; metadata only, no production compilation |

Unit compile commands used `-std=c++17`, `-fno-rtti` and the selected exception
flag. This establishes local configuration evidence only, not public-header,
parser, full J5-001 portability or embedded runtime verification. No remote
matrix was executed in this session. C++ sources were unchanged.

The default Windows generator attempted unavailable NMake. Configuring with
`-G Ninja -DCMAKE_CXX_COMPILER=g++` selected the installed native toolchain.
The initial dependency fetch required network access. Separate build directories
attempted to populate the shared `tools/gtest` checkout; after completing the
fetch sequentially, the OFF configuration was regenerated using
`-DADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR=tools/gtest`. Use that offline override
for additional build directories reusing this checkout, and finish dependency
configuration before starting builds against it.

See the [P001 implementation handoff](requirements/p001-build-handoff.md) for
the remaining production boundary and next step.

## Setup follow-up, 2026-10-12

- Latest published licensing/setup unit workflow: [passed](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/38180963812).
- Latest published formatting workflow: [passed](https://github.com/Romanr92/Adaptive_JSON5/actions/runs/38180964060).
- Local library/example-data configuration and metadata-package installation passed.
  LICENSE, NOTICE, licensing/provenance documents and the GoogleTest notice retain
  their directory structure in the installation.
- Getting-started commands now use the visible component preset names.
- No production C++ parser or demonstration executable is present.

The repository has no production C++ or parser API. Its three GoogleTest units
verify the C++17 baseline and selected exception/RTTI flags only:

- `J5_001_BuildConfiguration.UsesCxx17OrNewer`
- `J5_003_BuildConfiguration.AppliesSelectedExceptionMode`
- `J5_101_BuildConfiguration.AppliesSelectedRttiMode`

They do not verify parsing, schema validation, a public non-throwing API, an
editor host or embedded device execution. Those requirements remain Draft.

Preparation checks on 2026-10-12:

| Check | Result |
| --- | --- |
| Local native Windows CMake/GoogleTest build, exceptions OFF, RTTI OFF | Passed |
| Local native Windows GoogleTest configuration suite, exceptions OFF | Passed: three cases in one CTest entry |
| Local native Windows build/suite, exceptions ON, RTTI OFF | Passed: the same three GoogleTest cases |
| clang-format 18.1.8 | Passed for the test-only C++ source |
| Skill frontmatter validation | Passed for both adapted skills |
| Requirement/issue mapping | 101 unique IDs; one owner per ID; 35 issues; ordered prerequisites |
| Original proposal preservation | Preserved as the supplied Markdown snapshot |

The local isolated compiler reports Clang 19.1.7 (Zig-provided), with CMake 4.0.1
and GoogleTest 1.17.0 at the documented immutable revision. The declared hosted
CI matrix uses Windows/MSVC, Linux/GCC and macOS/Clang, exceptions OFF/ON and
RTTI OFF, plus clang-format. Consult GitHub Actions for executed remote results;
do not treat a declared matrix as a passed matrix.

No vendor SDK, target device, integration/e2e test, sanitizer, fuzzer, benchmark,
static-analysis or coverage gate is included in the current quality scope.
