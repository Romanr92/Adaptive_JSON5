# Development setup verification

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
