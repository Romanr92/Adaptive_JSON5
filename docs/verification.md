# Development setup verification

## Public/private compiled-library refactor, 2026-10-19

Pre-commit source review compared against committed baseline `83f14a6`:
all 200 explicit-`noexcept` function definitions (including compile-time/template
definitions) and constructor initializer lists retain their original executable
tokens after the mechanical document payload-helper relocation. Fifteen
fundamental declaration/data headers retain the same tokens apart from include
paths and comments, including result/error types, record payloads and Unicode
tables. All 286 original GoogleTest declaration bodies and assertions are
unchanged; the only added runtime unit is the public-library boundary case.
The remaining additions are compiled-target/test/package wiring, separated
public descriptors, and documentation. No new parser grammar, numeric operation,
allocation strategy or P034+ feature was introduced. The review corrected a
malformed Doxygen paragraph; it found no executable-code regression. This is
source-review evidence, not a formal equivalence or standards-compliance proof.

The user explicitly authorized AI production changes for the structural
refactor described in [ADR 0002](adr/0002-public-private-source-layout.md).
The former header-only `include/adaptive_json5/` layout was replaced with
20 supported public headers under `includes/`, 50 private headers under
`src/includes/`, and 48 ordinary implementation translation units grouped by
topic under `src/src/`. Templates, compile-time definitions and small defaulted
operations remain in headers where needed. No complete-document parser entry
or subsequent P034–P037 feature was added.

`AdaptiveJSON5::parser` now names a compiled static library. The public include
interface contains no private source paths. Document construction retains its
existing allocation-free by-value storage representation: its supporting
`detail::value_storage` declaration resides in the public document header, while
record/payload definitions and storage implementations are private. No opaque
state allocation or new global-heap path was introduced. Number classification
and warning descriptors have public headers separate from private lexer tokens.

Internal component units moved to `src/tests/`; consumer-facing units remain in
top-level `tests/`. Mixed document, allocation, string and accounting units were
split along that boundary, retaining all **286 original test declarations** and
their assertions. One additional public-library unit verifies owned document
construction, movement, checked access and UTF-8 export through supported public
headers. Each of the **20 public headers** is also compiled in a separate unit
translation unit without private include access. Test names and existing labels
were retained when tests changed executable or path.

Executed with MinGW GCC 16.2.0, explicit C++17 and **32 parallel workers**, matching
this PC's reported logical processor count:

| Configuration / operation | Result |
| --- | --- |
| Full `debug-unit-tests`, exceptions OFF / RTTI OFF | **1422/1422 passed** |
| Full `debug-unit-tests-exceptions`, exceptions ON / RTTI OFF | **1422/1422 passed** |
| clang-format 18.1.8, all current C++ source/header files | **157 files passed** |
| Tests-disabled Release core configuration/build | Passed, without acquiring/building GoogleTest |
| Installation into a workspace-local prefix | Compiled target and 20 public headers installed; no private/test headers or private include-root export |

Both unit builds use the existing pinned offline GoogleTest source override.
Splitting translation units exposed formerly transitive dependencies; explicit
private includes were added to affected internal units. The full exceptions-ON
build also exposed this compiler's C++20 default through existing `u8` string
fixtures. Project targets now explicitly request C++17 rather than relying only
on the minimum compile feature. The successful results above follow those build
corrections. Generated Unicode table data, versions and notices are unchanged;
the generator's default output/include paths now follow the private layout.

Commands used for both full unit configurations:

```powershell
cmake --build --preset debug-unit-tests --parallel 32
ctest --preset debug-unit-tests --parallel 32 --output-on-failure
cmake --build --preset debug-unit-tests-exceptions --parallel 32
ctest --preset debug-unit-tests-exceptions --parallel 32 --output-on-failure
```

The install operation verifies generated package contents, not an installed
consumer execution or an additional automated integration gate. Native Windows
results do not establish another compiler/device, standards compliance or full
language conformance. Requirements remain Draft, and subsequent production
features remain subject to the normal human-owned implementation workflow.

## P033 iterative container engine units, 2026-10-19

Added `tests/parser_engine_tests.cpp` and the CTest label
`parser_engine_units`: **75 discovered cases**. The named declarations are
recorded separately in [traceability](traceability.md). Coverage includes empty
and mixed array/object nesting in both modes, exact heterogeneous content and
child spans, malformed structural roots and missing delimiters, every unfinished
prefix of a nested fixture, original encoding-error offsets, JSON5-only nested
grammar, internal consumed-prefix semantics, warning aggregation across names
and values, input/configuration rejection, source overwrite and document moves.

Resource units cover inclusive depth/node/member/string/name/aggregate/numeric/
exponent/input limits, zero-unit empty content, scalar roots with zero container
depth, mixed depth 64 and depth 65 rejection, failed deep cleanup, all allocation
requests in a representative mixed parse, increasing work allowances through
the first success, and byte ceilings below/at/above measured outstanding peak.
Direct component units inspect pending-name ownership across frame relocation,
growth allocation failure, provisional-frame rollback after record allocation
failure, and close-mismatch preservation of frames/source spans. Failure reuse
means a **fresh engine** on the same live accounting adapter, not rerunning an
abandoned engine instance.

Executed locally with MinGW GCC 16.2.0 on Windows:

| Configuration | Executed scope | Result |
| --- | --- | --- |
| `debug-unit-tests`, exceptions OFF, RTTI OFF | Full available GoogleTest suite | **1421/1421 passed**, including **75/75 P033** |
| `debug-unit-tests-exceptions`, exceptions ON, RTTI OFF | P033, primitive parser, document and resource guards | **296/296 passed**, including **75/75 P033** |
| clang-format 18.1.8 | New `parser_engine_tests.cpp`, `--dry-run --Werror` | Passed |

Commands used after configuration/build:

```powershell
ctest --preset debug-unit-tests --parallel 4 --output-on-failure
ctest --preset debug-unit-tests-exceptions -L '^(parser_engine_units|primitive_parser_units|document_units|resource_guards_units)$' --parallel 4 --output-on-failure
```

Both builds used the existing offline-source override pointing at `tools/gtest`,
checked out at pinned revision `52eb8108c5bdec04579160ae17225d66034bd723`.
The initial exceptions-ON configure attempted a network fetch and failed; the
generated dependency checkout was restored and pinned before successful builds.
Four initial failures were two incorrect test-fixture expectations instantiated
in both modes: the mixed container fixture has six records, and its separate
unterminated fixture ends at byte 12. Correcting those independent counts required
no production changes. The final executions above supersede that initial run.

No production C++ was changed. These results establish P033 unit evidence, not
completion of all P034–P036 acceptance, P037 complete-input behavior, the later
whole-grammar evidence matrix or cross-component #69 accounting. The internal
engine deliberately returns a root prefix; its successful result does not
validate a trailing suffix. Requirements remain Draft. No embedded execution,
real-time bound, MISRA/CERT/AUTOSAR compliance or CI result is inferred.

## Individual CTest case registration, 2026-10-13

CMake now uses `gtest_discover_tests` to register each GoogleTest case, including
parameterized cases, separately. The four former executable entry names remain
group labels selectable with `ctest -L`; `-R` selects individual case names.
Historical executable-level totals below describe the registration at that time.

The Zed `launch-tests` workflow rebuilt and passed all 98 individual cases with
exceptions OFF and RTTI OFF. The exceptions-ON preset also rebuilt and passed
all 98 cases with RTTI OFF. No production C++ or test assertions changed.
The formatting check could not run because the installed clang-format executable
failed its `--version` invocation; formatting remains unverified for this change.

## Issue #1 public-contract documentation, 2026-10-13

The existing [architecture document](architecture.md#public-ownership-value-and-policy-contracts)
now covers syntax defaults, input/result lifetimes, owned documents and views,
duplicate preservation/rejection, exact numbers and checked conversions, error
categories, caller storage, named limits, cleanup, concurrency and determinism.
Unresolved signatures, document-move view stability, allocator details,
conversion policies and numeric limit defaults are explicitly Draft.

This completes the remaining documentation acceptance item for Issue #1. It
does not implement these future APIs, change J5 statuses or establish a parser
release. The previous 51-case OFF/ON unit evidence applies to current
declarations only. No new production changes or test executions were needed
for this documentation update.

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

## P004 cursor units, 2026-10-13

The developer's `cursor.hpp` provides explicit length bounding, forward consumption,
arbitrary remaining-bounded lookahead (`peek(delta)`), clamped advance, and subrange
span slicing (`span_from`) over borrowed `input_view` buffers. The agent added only
test C++ (`tests/cursor_tests.cpp`), test build setup (`tests/CMakeLists.txt`), and
traceability/verification updates.

`tests/cursor_tests.cpp` exercises 8 GoogleTest cases across 3 requirements:
- `J5_004_Cursor`: default-constructed cursor immediate end state; empty non-null
  view immediate end state; exact length consumption without terminating NUL
  assumption; reading across embedded zero bytes; bounded lookahead (`peek`);
  advance clamped at buffer end.
- `J5_041_Cursor`: source span extraction up to current offset with boundary clamps.
- `J5_044_Cursor`: exact zero-based byte offset preservation throughout consumption.

| Local native Windows configuration | Executed result |
| --- | --- |
| GCC 15.2.0, C++17, exceptions OFF, RTTI OFF | All 4 CTest test executables passed (98 total test cases across suites, 16 in `api_configuration_units`) |
| GCC 15.2.0, C++17, exceptions ON, RTTI OFF | All 4 CTest test executables passed (98 total test cases across suites, 16 in `api_configuration_units`) |
| clang-format 18.1.8 | Passed for all 14 C++ files (`python tools/format.py --check`) |

Both configurations used Ninja, CMake 4.4.4 and the pinned GoogleTest revision.
Reproduce with `cmake --build --preset debug-unit-tests` and `ctest --preset debug-unit-tests`,
followed by the corresponding `debug-unit-tests-exceptions` preset commands.

P004's cursor component has local executed unit evidence. Broader J5-004, J5-041,
and J5-044 parser diagnostics, token spans, and full input acceptance remain Draft.

## P003 storage and accounting units, 2026-10-13

The developer's `checked_size.hpp`, `storage.hpp` and `owned_block.hpp` provide
overflow-checked size arithmetic, caller-supplied storage callback types,
move-only `owned_block` RAII handles, and `allocate_block` / `allocate_array`
fallible allocation primitives. The agent added only test C++
(`tests/storage_tests.cpp`), test build setup (`tests/CMakeLists.txt`) and
traceability/verification documentation for this step.

`tests/storage_tests.cpp` exercises 39 GoogleTest cases across 9 test suites:
- `checked_add` and `checked_multiply` parameterized across zero, one, small
  values, maximum unsigned limits and overflow boundaries returning
  `error_code::resource_limit` with no byte offset.
- `storage_callbacks` default construction and callback signature contracts.
- Allocation-point failure injection via a test `counting_provider`, verifying
  explicit `error_code::allocation_failure` outcomes and zero outstanding bytes.
- Missing callback validation rejecting null allocate or deallocate pointers.
- Alignment validation rejecting zero or non-power-of-two alignments with
  `error_code::resource_limit` without calling the provider.
- `allocate_array` calculating exact byte sizes and aborting on size overflow
  before calling the provider.
- `owned_block` default construction owning nothing, single balanced release
  matching original address, bytes and alignment upon destruction, move
  construction transferring ownership without double release, and zero-byte
  allocation producing an empty block without invoking the provider.
- Operation atomicity and provider reuse (J5-051) verifying that an oversized
  allocation failure leaves the provider intact and reusable for subsequent
  successful allocations.

| Local native Windows configuration | Executed result |
| --- | --- |
| GCC 15.2.0, C++17, exceptions OFF, RTTI OFF | All 4 CTest test executables passed (90 total test cases across suites, 39 in `storage_units`) |
| GCC 15.2.0, C++17, exceptions ON, RTTI OFF | All 4 CTest test executables passed (90 total test cases across suites, 39 in `storage_units`) |
| clang-format 18.1.8 | Passed for all 12 C++ files (`python tools/format.py --check`) |

Both configurations used Ninja, CMake 4.4.4 and the pinned GoogleTest revision
with `-DADAPTIVE_JSON5_GOOGLETEST_SOURCE_DIR=tools/gtest`. Reproduce with
`cmake --build --preset debug-unit-tests` and `ctest --preset debug-unit-tests`,
followed by the corresponding `debug-unit-tests-exceptions` preset commands. The
new CTest entry is `storage_units`, running `adaptive_json5_storage_unit_tests`.

P003's current storage and accounting primitives have local executed unit evidence.
Broader J5-040, J5-046, J5-051 and J5-101 contracts remain Draft because parser
integration, dynamic document containers and embedded device verification remain
pending. No remote compiler matrix or hardware target run was performed.

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

## P005 contract-first units, 2026-10-13

`tests/utf8_tests.cpp` declares eight unit patterns, expanding to 47 GoogleTest
cases once the developer supplies `include/adaptive_json5/utf8.hpp`. These
are pending tests, not passing execution evidence. J5-009 remains Draft.

The proposed test-facing API is `decode_utf8(cursor&) noexcept` returning
`result<std::optional<decoded_character>>`. A character has `char32_t value`,
`std::size_t byte_offset` and `std::size_t byte_length`. Successful empty optional
means EOF. A failed sequence reports `error_code::encoding` at its starting
byte offset and leaves the cursor unchanged. These API/state conventions are
project design suggestions locked into these initial tests, not RFC mandates;
the human developer can select another API and the units must then be adapted.

Coverage includes scalar/width boundaries, NUL and all ASCII bytes, BOM and
noncharacters, supplementary scalars, every forbidden leading octet, every
noncontinuation octet in each continuation position, overlong encodings,
surrogates, out-of-range values, all nonempty truncations of boundary encodings,
nonzero byte offsets, unchanged buffers, retry after failure and repeated EOF.
Parser integration must later demonstrate validation within comments and
trailing trivia in both syntax modes; these decoder units do not establish that.

CMake configuration succeeded for exceptions OFF/RTTI OFF and exceptions
ON/RTTI OFF. Building `adaptive_json5_utf8_unit_tests` in both configurations
stopped at the expected missing `adaptive_json5/utf8.hpp`; no P005 cases ran.
clang-format 18 is unavailable in this environment, and attempts to acquire
18.1.8 into ignored build storage failed; formatting remains pending.

After the production header exists, build the unit-test preset and run:

```powershell
cmake --build --preset debug-unit-tests --target adaptive_json5_utf8_unit_tests
ctest --preset debug-unit-tests -L "^utf8_units$" --output-on-failure
```

Repeat with exceptions ON and RTTI OFF. No production C++ was added by the agent.

### Formatting follow-up

clang-format 18.1.8 was subsequently installed into ignored `build/format-tools`
storage. Zed's C/C++ project settings now select that executable with formatting
on save. Both configured commands passed a stdin/stdout formatting smoke check;
an actual save in the Zed UI was not observed. `tests/utf8_tests.cpp` was formatted.
The repository-wide formatting check currently flags the developer's new
`utf8.hpp` namespace brace; no production header was changed by the agent.

The formatter settings were subsequently made portable through
`tools/zed_format.py`: the helper and style are located from the edited buffer
path, and the executable is selected from local build storage or PATH. Windows
smoke checks passed for C/C++ with an unrelated working directory and a copied
checkout whose path contains spaces. Formatting the returned buffer again was
idempotent. Linux/macOS name handling is present but was not executed here.

### P005 developer implementation, 2026-10-13

The developer supplied `include/adaptive_json5/utf8.hpp`. All 47 decoder cases
passed locally on Windows/GCC 16.2.0 with exceptions OFF/RTTI OFF in
`build/debug-unit-tests` and exceptions ON/RTTI OFF in
`build/p005-units-exceptions`. The configured builds used the compiler's C++20
default; a separate C++17 syntax check of the header with exceptions OFF and
RTTI OFF also passed. clang-format 18.1.8 checking passed for all 16 C++ files.
This supersedes the earlier missing-header/pending execution state at P005
component scope. J5-009 remains Draft: comment/trailing-trivia validation in both
parser modes and remote matrix execution are not established by these units.

## P006 generated Unicode tables, 2026-10-13

The user explicitly authorized generation of `unicode_tables.hpp`; this narrow
exception does not authorize edits to the other production headers. Unicode
17.0.0 and 3.0.0 inputs and the complete Unicode License V3 snapshot are retained
with SHA-256 hashes and source URLs. Offline regeneration produced byte-identical
output (35,132 bytes). The generated header has 686 identifier-start ranges,
398 additional continuation ranges, 7 modern Zs ranges and 6 historical Zs ranges.

Fourteen table-only GoogleTest cases passed locally on Windows/GCC 16.2.0 with
C++17, RTTI OFF and exceptions OFF/ON. Commands used the
`adaptive_json5_unicode_table_unit_tests` target and `unicode_table_units` label
in `build/debug-unit-tests` and `build/p005-units-exceptions`. These units cover
range invariants and selected independent category fixtures, including historical
category changes and supplementary characters. They do not test classification
functions, identifier parsing or whitespace skipping, which remain pending.
The broader J5 contracts remain Draft. clang-format 18.1.8 checking passed for
all 20 C++ files. Remote unit jobs were not executed for this change.

## P006 classifier units, 2026-10-14

The developer corrected the binary search's earlier-half update and moved its
miss return after the loop. Forty-six lookup/classifier cases now pass locally
on Windows/GCC 16.2.0 with C++17, RTTI OFF and exceptions OFF/ON. These are
additional to the 14 generated-table cases. The affected target is
`adaptive_json5_unicode_unit_tests`, selected with CTest label `unicode_units`
in `build/debug-unit-tests` and `build/p005-units-exceptions`.

The units exercise both binary-search directions, range endpoints and gaps,
Unicode category representatives, dollar/underscore, ZWNJ/ZWJ, historical
memberships, supplementary characters and invalid scalars. clang-format 18.1.8
passed for all 21 C++ files. The first search branch's explanatory comments
still describe the later half, although its implementation now correctly sets
`end = middle`; production comments were not edited by the agent.

This is component evidence. Full identifier grammar, escaped names, whitespace
skipping, complete Unicode/category conformance and remote jobs are not verified
by these cases. The broader J5 contracts remain Draft.

## P007 line-terminator units, 2026-10-14

Twenty-five GoogleTest cases were added in `tests/line_terminator_tests.cpp`
for exact spans, nonterminators, CRLF grouping, EOF and bounded reads, malformed
UTF-8 and unchanged-cursor failures. Builds succeeded with C++17, RTTI OFF and
exceptions OFF/ON on Windows/GCC 16.2.0. Both runs passed 23 cases and failed 2:
`LineSeparator` and `SubscriptZero` under the
`SequenceFixtures/J5_011_LineTerminator.RecognizesExactSpanOrLeavesInput` pattern.
The developer's `c_line_separator` is still `0x2080`; it must be `0x2028`.
This causes U+2028 to be missed and U+2080 to be consumed incorrectly.
Production code was not changed by the agent. P007 remains incomplete, and
all broader linked J5 contracts remain Draft. Results used target
`adaptive_json5_line_terminator_unit_tests` and label `line_terminator_units`
in the existing OFF/ON build directories.

### P007 correction and passing execution

The developer corrected `c_line_separator` to U+2028. All 25 line-terminator
cases passed on 2026-10-14 in both existing C++17 exception configurations,
with RTTI OFF. clang-format 18.1.8 passed for all 23 C++ files. This supersedes
the preceding 23-pass/2-fail execution result at P007 component scope; broader
parser location/continuation contracts and remote verification remain pending.

## P008 whitespace units, 2026-10-14

The developer supplied `whitespace.hpp` and corrected its line-count field name.
All 38 cases in `tests/whitespace_tests.cpp` passed on Windows/GCC 16.2.0 with
C++17, RTTI OFF and exceptions OFF/ON. The affected target is
`adaptive_json5_whitespace_unit_tests`, selected with CTest label
`whitespace_units` in the existing OFF/ON build directories.

The cases include every admitted individual space, retained historical Zs,
line terminators/CRLF counts, nonwhitespace distinctions, EOF and exact-length
views, byte spans at zero/nonzero offsets, leading/interior BOMs and rollback
with original encoding-error offsets. Parameter cases check before/after
boundaries for structural punctuation, both quote styles, primitive/number
spellings and identifiers using a simulated token-consumer seam. They do not
parse those tokens. clang-format 18.1.8 passed for all 25 C++ files.

This is P008 component evidence. Full parser/token behavior, strict-mode BOM
policy and remote jobs remain unverified; broader J5 contracts remain Draft.

## P009-P012 comment, trivia and punctuation units, 2026-10-14

Added 68 GoogleTest cases in `tests/lexer_trivia_tests.cpp`, built as
`adaptive_json5_lexer_trivia_unit_tests` and selected with label
`lexer_trivia_units`. On Windows/GCC 15.2.0 with C++17, all 68 passed with
exceptions OFF and ON, both with RTTI OFF. The complete host suite passed
336/336 cases in each configuration using `debug-unit-tests` and
`debug-unit-tests-exceptions`. clang-format 18.1.8 passed for all 29 C++ files.

The first OFF run passed 67/68 cases: the independent DelimitersInLine fixture
incorrectly expected 14 bytes rather than 15. Correcting that fixture produced
the passing executions above; no production code was changed by the agent.

Coverage includes line EOF and all terminators, UTF-8 and embedded NUL contents,
first block closer/non-nesting, every proper prefix of a block-comment fixture,
explicit-length boundaries, located encoding/syntax errors and cursor rollback.
Trivia cases cover mixed/adjacent comments, mode-specific whitespace, strict
comment rejection, empty/EOF input and byte-preserving token separation.
Punctuation cases check all six kinds and original spans, every other byte as
no-match, and comment contents excluded by the trivia layer.

These are P009-P012 component results, not a complete parser verification.
The token-boundary unit uses a simulated one-byte token consumer; strings,
numbers, literals, container grammar, trailing-comma rules and complete-input
rejection remain responsibilities of later steps. No allocation-failure injection
is needed for these allocation-free operations. No remote CI, editor-host or
embedded-device execution was performed. Broader J5 contracts remain Draft.


## P013 owned string storage and conversion units, 2026-10-15

Added 42 GoogleTest cases in `tests/string_storage_tests.cpp`, built as
`adaptive_json5_string_storage_unit_tests` with label `string_storage_units`.
On native Windows/GCC 15.2.0 with C++17 and RTTI OFF, all 42 passed with
exceptions OFF and ON. The full host suite passed 378/378 cases in each mode
using `debug-unit-tests` and `debug-unit-tests-exceptions`.
clang-format 18.1.8 passed for all 35 C++ files.

Coverage includes explicit-length/NUL/control-unit storage, supplementary scalar
and UTF-8 boundaries, lone/reversed surrogate preservation with checked export
rejection, invalid scalar/index failures, exact output capacities and untouched
failure buffers, input independence, moves, finish/reuse, and unfinished cleanup.
A fixed aligned caller provider checks original release descriptions and balanced
ownership, and injects failure at each of five growth allocations. Tests also
cover atomic pair append failures, exact unit limits, allocation-size overflow,
missing callbacks, and cross-provider/self/empty owned-block move assignment.
The old P003 compile-time assertion was updated to require non-throwing move
assignment, matching the developer's new API. No production C++ was edited.

The exceptions-ON directory initially selected unavailable NMake. Reconfiguring
that local build with Ninja/GCC and the existing offline GoogleTest checkout
allowed the successful execution above; no repository preset was changed.

These are P013 component results. Parser string boundaries/escape lexing,
document integration, full memory accounting, remote CI and embedded-device
execution remain unverified. No tests parse string literals or claim P014-P017
completion. Infeasibly large live-string states are not fabricated to exercise
UTF-8 total-size overflow; primitive overflow is covered by P003 and copy-size
checks. Broader J5 contracts remain Draft.


## P014 quoted-string boundary units, 2026-10-15

Added 26 GoogleTest cases in `tests/string_boundary_tests.cpp`, built as
`adaptive_json5_string_boundary_unit_tests`, selected by `string_boundary_units`.
All 26 passed on native Windows/GCC 15.2.0 with C++17 in both
`debug-unit-tests` (exceptions OFF) and `debug-unit-tests-exceptions`
(exceptions ON), with RTTI OFF in both. clang-format 18.1.8 passed for all
37 C++ files. No production C++ was edited. The full host suite was not rerun
for this step; the preceding P013 full-suite results remain historical evidence.

Coverage includes both delimiter styles and strict single-quote rejection,
all nonquote input bytes as opener no-match, empty views and strings, opposite
quotes as content, adjacent strings, original nonzero offsets, one-to-four-byte
UTF-8 characters, raw U+2028/U+2029 acceptance, every ASCII control in both
modes, CRLF rejection, malformed/overlong/surrogate/out-of-range/truncated UTF-8,
every proper nonempty byte prefix of a plain UTF-8 string, and cursor stability
on failure. Escape handoff checks only the backslash and its unread suffix.

These allocation-free boundary helpers do not constitute a complete string
lexer. Escape decoding/escaped quotes, line continuations, separator warnings,
builder composition and whole-string atomic failure belong to later P steps.
No helper in the tests implements escape or string parsing. Component results
do not promote J5-014 from Draft or establish remote CI or device execution.


## P015 simple escape units, 2026-10-15

Added 27 GoogleTest cases in `tests/simple_escape_tests.cpp`, built as
`adaptive_json5_simple_escape_unit_tests` with label `simple_escape_units`.
All 27 passed on native Windows/GCC 15.2.0 with C++17 in both
`debug-unit-tests` (exceptions OFF) and `debug-unit-tests-exceptions`
(exceptions ON), with RTTI OFF in both. clang-format 18.1.8 passed for all
39 C++ files. No production C++ was changed. Only the affected unit target
was rebuilt and run for this step; prior full-suite evidence remains historical.

Independent numeric expectations check all eleven simple spellings under both
modes, including backspace/form-feed corrections made by the developer before
these runs. Cases cover strict JSON rejection of apostrophe/vertical-tab/NUL,
exact-length escape suffixes, empty input/EOF, nonzero original offsets,
every decimal digit after zero, nondigit lookahead including raw NUL, hidden
lookahead beyond the explicit view, and every other byte as unchanged no-match.
Two direct helper-composition cases check escaped-quote handoff and EOF after
an accepted NUL escape without implementing a string lexer in test support.

This is P015 component evidence, not complete J5-015/J5-016 verification.
Hex/Unicode escapes, non-escape characters, continuations and complete-string
composition remain later steps. No-match for an unowned byte is not syntax
acceptance. No allocation failure is injected into these allocation-free helpers.
Draft status, remote CI and embedded-device support claims are unchanged.


## P016 hexadecimal escape units, 2026-10-15

Added 30 GoogleTest cases in `tests/hex_escape_tests.cpp`, built as
`adaptive_json5_hex_escape_unit_tests`, selected with label `hex_escape_units`.
All 30 passed on native Windows/GCC 15.2.0 with C++17, exceptions OFF and ON,
and RTTI OFF in both presets. clang-format 18.1.8 passed for all 41 C++ files.
Only the affected target was rebuilt and run for this step; no fresh full-suite
execution or remote CI is claimed. No production C++ was edited.

Independent literal expectations cover all 22 hex-digit spellings, nonhex byte
classification and Unicode lookalikes, zero/maximum decoded bytes, nibble order,
case combinations, quotes/backslashes as decoded content, exact-length buffers,
all proper escape prefixes, strict JSON rejection, located invalid-digit and
UTF-8 failures, explicit-view bounds and cursor rollback. Every non-x byte is
checked as unchanged no-match; following hex/quote/NUL/malformed bytes remain
unread after exactly two digits. Tests implement no decoding algorithm.

This is P016 component evidence. Complete string syntax, Unicode escapes,
non-escape characters, continuation/warning behavior and composition remain in
their later steps. No allocation injection is needed for these allocation-free
helpers. Broader J5-016 remains Draft; embedded-device support is not inferred.


## P017 Unicode escape units, 2026-10-15

Added 41 GoogleTest cases in `tests/unicode_escape_tests.cpp`, built as
`adaptive_json5_unicode_escape_unit_tests` and selected by `unicode_escape_units`.
All 41 passed on native Windows/GCC 15.2.0 with C++17 in both exception modes,
with RTTI OFF. clang-format 18.1.8 passed for all 43 C++ files. Only the affected
target was rebuilt and run; no new full-suite or remote CI execution is claimed.
No production C++ was changed.

Coverage includes literal nibble-position, BMP, NUL, quote, backslash,
case-combination and surrogate-boundary values. Both syntax-mode contexts use
the real P014 opener/backslash handoff before Unicode decoding. Every proper
escape prefix, invalid character/UTF-8 spelling at every digit position,
truncated supplementary source character prefix, non-u byte and unread trailing
byte are checked with exact cursor/error expectations. Literal paired, reversed,
high/high, low/low and high/ASCII escapes compose with the existing builder,
UTF-16 scalar decoder and UTF-8 exporter. Valid min/music/max pairs produce
independent literal UTF-8 bytes. Lone boundary surrogates remain exact units and
export fails without replacement or destination modification. Test callbacks
supply host storage only; no test helper implements production decoding.

These are P017 component results. P013-P017 now have local component unit
execution evidence, not a complete string lexer or finished parser. Full-string
rollback, non-escape characters, continuations, warnings and token composition
remain later steps. The Unicode escape helper itself allocates nothing; storage
failure injection belongs to the existing P013 units. Shared J5 rows stay Draft;
issue completion/PR review/merge and device support are separate evidence.


## P018 non-escape character units, 2026-10-15

Added 31 GoogleTest cases in `tests/non_escape_tests.cpp`, built as
`adaptive_json5_non_escape_unit_tests` and selected by `non_escape_units`.
All 31 passed on native Windows/GCC 15.2.0 with C++17 in both exception modes,
with RTTI OFF. clang-format 18.1.8 passed for all 45 C++ files. Only the affected
target was rebuilt and run; no new full-suite or remote CI execution is claimed.
No production C++ was edited.

Independent literal cases check a/e/slash/space, uppercase X/U, punctuation,
raw tab/NUL and two-/three-/four-byte source characters, preserving scalars,
original offsets, byte lengths, input and unread suffixes. Strict-mode rejection,
all digits 1-9, all special escape spellings and all five line-terminator
sequences, malformed UTF-8 classes, every partial valid UTF-8 width, bounded
input and EOF are exercised with cursor stability and explicit error locations.
Direct checks against existing hex/Unicode decoders show their malformed forms
remain failures and are not accepted by the fallback helper. A supplementary
non-escape composes with the existing UTF-16 encoder without unit truncation.

These are P018 component results. Direct decoder calls do not establish the
later P021 dispatcher's error propagation or full-string atomicity. Continuation
removal and separator warnings remain P019/P020. The non-escape helper allocates
nothing, so no allocation injection is required. Broader J5-016 remains Draft;
embedded-device execution is not inferred from host tests.


## P019 line continuation units, 2026-10-15

Added 21 GoogleTest cases in `tests/line_continuation_tests.cpp`, built as
`adaptive_json5_line_continuation_unit_tests`, selected by `line_continuation_units`.
All 21 passed on native Windows/GCC 15.2.0 with C++17 in both exception modes,
with RTTI OFF. clang-format 18.1.8 passed for all 47 C++ files. Only the affected
target was rebuilt and run; no new full-suite or remote CI execution is claimed.
No production C++ was edited.

Coverage includes LF/CR/CRLF/U+2028/U+2029 exact spans, nonzero offsets,
strict-mode rejection and rollback, EOF, bounded CR lookahead, separated LF,
additional raw terminators, nonterminating ASCII including NUL/VT/FF and Unicode,
malformed UTF-8 classes, all proper Unicode separator prefixes and unread
malformed suffixes after a complete CR. Five fixed public-API composition units
call the opener, boundary reader, continuation recognizer, builder and exporter
explicitly, producing exact A-space-B bytes without continuation content or
indentation removal. No test helper implements string parsing.

These are P019 component results. Continuation recognition is allocation-free;
no allocation-failure injection is needed here. Host callbacks supply storage
only for the visible composition cases. General string composition, dispatch
and whole-token rollback remain P021; raw-separator warnings remain P020.
Broader J5-017 stays Draft and device execution is not inferred.


## P020 raw separator warning units, 2026-10-15

Added six GoogleTest cases in `tests/string_warning_tests.cpp`, built as
`adaptive_json5_string_warning_unit_tests` with label `string_warning_units`.
All six passed on native Windows/GCC 15.2.0 with C++17 in both exception modes,
with RTTI OFF. clang-format 18.1.8 passed for all 49 C++ files. Only the affected
target was rebuilt and run; no fresh full-suite or remote CI execution is claimed.
No production C++ was edited.

Cases use the real boundary reader to check raw U+2028/U+2029 warnings, both
JSON5 quote styles, strict-mode acceptance without JSON5 advisories, original
byte offsets after a multibyte prefix, three-byte spans and repeatable inspection.
Existing builder/export APIs confirm unchanged separator content and successful
closing boundaries. Actual escape-start pieces are inspected before existing
Unicode/continuation decoders handle escaped or continued separators, avoiding
artificial reconstructed raw pieces. Other ASCII source/syntax pieces,
neighboring scalars and supplementary characters do not warn. Adjacent separators
produce independent descriptors with exact categories and source offsets.

These are P020 descriptor/component results. Warning collection/exposure and
whole-token success/failure remain P021 work. The inspected helper performs no
allocation; host storage callbacks are only used to check retained content.
No test helper implements parsing or warning collection. Broader J5-017 remains
Draft, and no embedded-device support is inferred.


## P021 complete string lexer units, 2026-10-15

Added 51 GoogleTest cases in `tests/string_lexer_tests.cpp`, built as
`adaptive_json5_string_lexer_unit_tests`, selected by `string_lexer_units`.
All 51 passed on native Windows/GCC 15.2.0 with C++17, exceptions OFF and ON,
and RTTI OFF in both configurations. The complete host unit suite was rebuilt
and passed 611/611 cases in each preset. clang-format 18.1.8 passed for all
53 C++ files. No production C++ was edited.

Literal expectations cover empty and both quote styles, JSON/JSON5 simple
escapes, hex/Unicode combinations, raw controls/NUL, raw and escaped supplementary
characters, lone/reversed surrogate preservation, comment-like string content
and all five continuations. Explicit failures check exact original error offsets
and full-token cursor/storage rollback for malformed and mode-forbidden syntax
and encoding. Every proper nonempty byte prefix of a combined escape/UTF-8/
continuation/warning string fails without publishing a partial result or warning.
No-match checks every initial nonquote byte and empty views without allocation.
Closing boundaries preserve adjacent strings and malformed following source.

The fixed host provider injects failure at each actual growth allocation,
verifies release descriptions and balanced ownership, and permits retry from the
unchanged cursor. Exact decoded limits exercise raw scalars, escaped scalar pairs,
escaped surrogate units and resource failure after an observed raw separator.
Empty/continuation-only content needs no provider; nonempty missing-provider
failure is explicit. Token moves and input overwrite/destruction preserve owned
content. Warning observers capture ordered original spans only after construction;
metadata/content agree with disabled callbacks, strict separators do not warn,
and later lexical/storage/limit failures suppress notifications. Callback replay
is checked for no additional allocations between notifications.

These are complete string-token component results for P021 and its P018-P020
composition, not a complete JSON5 parser. The lexer intentionally stops after
its closing quote; later token-boundary/document work owns suffix validation.
Warning replay relies on documented immutable input and non-throwing observer
contracts; callback mutation/failure is unsupported. Global parser budgets,
cancellation, containers, document-level integration, remote CI and embedded
execution remain later evidence. Shared J5 requirements stay Draft; issue review,
required PR checks and merge status are separate from these local results.


## P022 ordinary identifier units, 2026-10-16

Added 20 GoogleTest cases in `tests/identifier_tests.cpp`, built as
`adaptive_json5_identifier_unit_tests`, selected by `identifier_units`.
All 20 passed on native Windows/GCC 15.2.0, C++17, exceptions OFF and ON,
with RTTI OFF in both presets. Only the affected target was built and run;
no fresh full-suite or remote CI result is claimed. clang-format 18.1.8
passed for all 56 C++ files. No production C++ was edited.

Literal expected UTF-16 content covers dollar/underscore, Unicode start categories,
continuation marks/digits/connector punctuation/ZWNJ/ZWJ, supplementary letters,
reserved words and literal-looking names. Negative starts, empty views, explicit
NUL, nonzero offsets and unconsumed ASCII/multibyte terminators are checked.
Malformed and truncated UTF-8 produces exact original encoding error locations
and cursor/storage rollback. Exact UTF-16 limits include supplementary pairs.
Every observed growth allocation is failed independently, temporary storage is
released and retry succeeds from the unchanged cursor. Missing storage returns
an explicit allocation failure; moved tokens preserve content after input
mutation/destruction. Compile-time checks cover non-throwing recognition and
move-only token ownership.

These are P022 component results. Identifier escapes (P023), literal classification
(P024), strict JSON key policy, complete token/document boundaries and embedded
execution are separate work. J5-019 remains Draft; issue completion, PR checks
and merge status are not inferred from these local results.


## P023 identifier escape units, 2026-10-16

Added 29 GoogleTest cases to `tests/identifier_tests.cpp`. All 49 P022/P023
cases passed on native Windows/GCC 15.2.0, C++17, exceptions OFF and ON,
with RTTI OFF in both presets. Only `adaptive_json5_identifier_unit_tests`
was rebuilt and `identifier_units` executed; no fresh full-suite or remote CI
result is claimed. clang-format 18.1.8 passed for all 56 C++ files.
No production C++ was edited during this test work.

Literal fixtures cover escaped starts/parts, dollar/underscore, combining marks,
connector punctuation, joiners, hexadecimal digit case, mixed raw supplementary
letters and escaped BMP content, reserved words and literal-looking names.
Assertions check exact decoded UTF-16 units, original byte spans, escape metadata
and unconsumed terminators at nonzero source offsets. Invalid escaped positions,
NUL, surrogate units/pairs, unsupported forms, nonhex digits and malformed UTF-8
inside/after escapes report explicit categories and exact original error offsets.
Every incomplete prefix of a Unicode escape is tested using a shortened view over
complete backing bytes, before and after an accepted name prefix. Failure leaves
the cursor unchanged and releases all temporary allocations.

Decoded-unit limits report the first excluded escape's backslash offset. Each
observed allocation is failed independently, with balanced release and successful
retry from the unchanged cursor. Moved escaped tokens retain exact content and
metadata after input overwrite/destruction. Actual identifier/string readers
produce literal expected matching UTF-16 content for bare, escaped and quoted
spellings; case and canonical-normalization variants preserve distinct sequences.
This establishes representation evidence, not an implemented equality/hash API.

P024 literal classification, object lookup/duplicate policy, hash collisions,
complete document boundaries and embedded execution remain separate evidence.
J5-020 and J5-033 remain Draft; local unit results do not establish issue closure,
required PR checks or merge completion.


## P024 identifier classification units, 2026-10-16

Added 58 parameterized cases in `tests/identifier_tests.cpp`, covering 29 source
spellings in JSON5 and strict JSON. All 107 P022-P024 cases passed on native
Windows/GCC 15.2.0, C++17, exceptions OFF and ON, with RTTI OFF in both presets.
Only `adaptive_json5_identifier_unit_tests` was rebuilt and `identifier_units`
executed; no fresh full-suite or remote CI result is claimed. clang-format
18.1.8 passed for all 57 C++ files. No production C++ was edited.

The real identifier reader supplies tokens for exact null/true/false, case and
suffix variants, every nonempty proper literal prefix, a same-length mismatch,
escaped literal spellings, unknown/reserved names and Unicode lookalikes/suffixes.
Every case is classified repeatedly in value context and as a member name.
JSON5 member names succeed; strict JSON member names report syntax at the
original nonzero byte offset after a multibyte prefix. Value classifications
match independent literal expectations in both modes. Classification preserves
exact decoded units, source metadata, escape flags, input bytes and cursor.
Arming the next provider allocation to fail confirms no new caller-storage
requests, releases or ownership changes occur during classification. A
compile-time assertion checks the public classification call is noexcept.

Infinity/NaN cases establish only that this primitive classifier defers them;
future numeric recognition remains responsible for their mode-specific handling.
Unrecognized value classification is not string acceptance or document rejection.
Quoted keys, container parsing, complete document/token boundaries, remote PR
checks and embedded execution remain separate work. Shared J5-019/J5-023 status
remains Draft; these local results do not imply issue closure or merge completion.


## P025 decimal numeric component units, 2026-10-16

Added 85 GoogleTest cases in `tests/decimal_tests.cpp`, built as
`adaptive_json5_decimal_unit_tests`, selected by `decimal_units`. All 85 passed
on native Windows/GCC 15.2.0, C++17, exceptions OFF and ON, RTTI OFF in both
presets. Only the affected target was built and run; no fresh full-suite or
remote CI result is claimed. clang-format 18.1.8 passed for all 60 C++ files.
No production C++ was edited during this test work.

Forty explicit spelling cases run in both modes. Independent expectations cover
zero/negative zero, integers, fractions, exponent case/sign/leading zeros,
JSON5 leading plus and leading/trailing decimal points, and exact syntax error
locations for leading zeros, missing components, malformed signs and exponents.
Success checks original spelling spans after a multibyte source prefix and an
unconsumed comma; errors leave the source cursor unchanged. Each explicit-length
prefix of a complete fraction/exponent backing buffer checks acceptance or
EOF-located failure without reading beyond the exposed view.

Additional cases check empty/null-empty views, ordinary nonmatches and deferred
hex/non-finite families; exact delimiter/trivia/NUL boundaries; and encoding
errors at required digit positions after signs, decimal points or exponent
markers/signs. Integers beyond 2^53 and signed/unsigned 64-bit ranges, 4096-digit
integers/fractions/exponents remain lexical successes without numeric conversion.
The public scanner has a compile-time noexcept check and requires no storage
callback. These tests do not add an allocation failure path to an allocation-free
API or instrument global allocations.

These are P025 decimal-component results, not complete numeric token validation.
Unconsumed suffix validation remains P028; hex/non-finite recognition remains
P026/P027. Owned numeric representation, configurable parser budgets and actual
numeric conversion remain later steps. No parser/document or embedded-device
support is inferred. J5-021/J5-022 remain Draft; PR checks, merge and issue closure
are separate from local component evidence.


## Compiler warnings-as-errors policy, 2026-10-16

Project targets, including all unit-test executables, now receive
`-Wparentheses -Werror` on GCC/Clang/AppleClang, or `/W4 /WX /w14706` on
MSVC-compatible builds through the shared private target configuration function.
Third-party targets and exported consumer flags are unchanged. MSVC/Clang
execution was not performed locally.

Both native Windows/GCC 15.2.0 presets were configured and full builds attempted
with exceptions OFF and ON, RTTI OFF. Both builds fail on the existing mixed
logical expression in `include/adaptive_json5/utf16.hpp:49`, now diagnosed as
`-Werror=parentheses`. Building remaining targets with Ninja's keep-going option
reported no other unique compiler errors. No production C++ was edited to bypass
or repair the diagnostic; the developer must explicitly group the surrogate
range conjunction. Full-suite success under the new flags is not claimed.

The freshly built decimal and build-configuration groups pass 88/88 cases in
each preset under the new warning flags. clang-format 18.1.8 passes for all
61 C++ files. These partial results do not replace a successful complete host
build/unit run or required PR checks.


## P026 hexadecimal numeric component units, 2026-10-16

Added 69 GoogleTest cases in `tests/hexadecimal_tests.cpp`, built as
`adaptive_json5_hexadecimal_unit_tests`, selected by `hexadecimal_units`.
All 69 passed on native Windows/GCC 15.2.0, C++17, exceptions OFF and ON,
RTTI OFF in both presets, with `-Wparentheses -Werror`. Only the affected target
was built and run; no fresh full-suite, remote CI or MSVC/Clang execution is
claimed. clang-format 18.1.8 passed for all 62 C++ files. No production C++
was edited during this unit-test work.

Thirty-two explicit valid/invalid spellings run in both modes. Independent
expectations check both prefix cases and all signs, negative/positive zero,
leading hex zeros, all ASCII hex digits, e as a digit, missing/invalid first
digits, spaces/signs before digits, identifier/Unicode suffixes, separators,
escape suffixes, NUL before digits and unsupported hexadecimal fractions.
Strict JSON failures locate the original token start. JSON5 failures locate
the exact offending character or EOF. Original spans are checked after a
multibyte prefix, and every failure preserves the original cursor/input.

Every exposed byte prefix of a signed hexadecimal backing buffer checks
no-match, incomplete-prefix error or exact success without overreading. Other
spellings and doubled signs return component no-match without consumption;
this does not establish document acceptance. Punctuation, comments, Unicode
spaces and explicit NUL remain unconsumed after a recognized component.
Malformed/truncated UTF-8 before/after digits produces exact encoding locations
and cursor rollback. Values beyond 64-bit ranges and 4096 hex digits succeed
without numeric conversion. The public scanner has a compile-time noexcept
check; its API requires no allocation callbacks.

P027 non-finite numbers and P028 general suffix/context validation remain
separate work, including rejection of unsupported complete spellings that this
component leaves for other recognizers. P029 owned numeric representation,
conversion and parser budgets remain later evidence. Shared J5 contracts stay
Draft; no full MISRA/CERT assessment, embedded support, issue closure or merge
completion is inferred from these host component results.


## P027 non-finite numeric component units, 2026-10-16

Added 58 GoogleTest cases in `tests/non_finite_tests.cpp`, built as
`adaptive_json5_non_finite_unit_tests`, selected by `non_finite_units`.
All 58 passed on native Windows/GCC 15.2.0, C++17, exceptions OFF and ON,
RTTI OFF in both presets, with `-Wparentheses -Werror`. Only the affected target
was built and run; no fresh full-suite, remote CI or MSVC/Clang execution is
claimed. clang-format 18.1.8 passed for all 64 C++ files. No production C++
was edited during this unit-test work.

Twenty-seven literal spellings run in both modes. The six exact signed/unsigned
Infinity/NaN forms produce independent expected kinds and original source spans.
Case mismatches, identifier/digit/underscore/dollar/Unicode suffixes, decimal
points, escaped spellings/suffixes, internal spaces/comments/NUL and Unicode
lookalikes produce exact syntax offsets. Strict mode rejects attempted I/N
spellings at the original token start after a multibyte prefix. Error paths
preserve the caller cursor, and the parameterized units check unchanged input.

Every bounded prefix of all six forms is tested over complete backing bytes,
checking pre-attempt no-match, truncated-word EOF failures and complete tokens.
Other initial spellings, lowercase names, doubled/missing signs and null-empty
views return component no-match without movement. This is not document acceptance.
Each complete form preserves adjacent punctuation, comments, Unicode space and
explicit NUL. Invalid/truncated UTF-8 within attempted words and after complete
words reports original encoding offsets with cursor rollback. The public
recognizer has a compile-time noexcept assertion; no numeric conversion or
storage-callback failure path is introduced into its metadata-only API.

P028 caller dispatch and complete suffix/context validation remain separate,
including final rejection of wrong-case/no-match value names and the distinct
identifier path for member names. P029 numeric ownership/representation also
remains later work. Shared J5 contracts stay Draft; host component results do
not establish MISRA/CERT compliance, embedded execution, PR/merge completion
or issue closure.


## P028 scalar and numeric token completion units, 2026-10-16

Added 125 GoogleTest cases in `tests/token_completion_tests.cpp`, built as
`adaptive_json5_token_completion_unit_tests`, selected by `token_completion_units`.
All 125 passed on native Windows/GCC 15.2.0, C++17, exceptions OFF and ON,
RTTI OFF, with `-Wparentheses -Werror`. Both complete host builds succeeded;
the full suite passed 1055/1055 cases in each preset. clang-format 18.1.8 passed
for all 69 C++ files. No production C++ was edited during this test work.
Execution logs are local build artifacts under `build/p028-full-*-results.log`;
no remote CI, MSVC/Clang or embedded execution is claimed.

Boundary fixtures check EOF, all six structural punctuators, strict JSON spaces,
JSON5-only whitespace/separators and comment openers, and explicit invalid
letters/digits/operators/quotes/backslashes/NUL. They preserve caller position
and input. Invalid scalar fixtures check exact categories/offsets and rollback
for truex, unknown/wrong-case/escaped names, numeric suffixes, binary/octal/
separator forms, incomplete hex, non-finite suffixes, doubled signs, repeated
points, adjacent quoted content and invalid slash/NUL boundaries. Applicable
numeric cases independently exercise the public number wrapper as well.

Success checks the actual variant alternative, literal/numeric kind, exact string
units and original spans. Scalar numbers work without a provider; strings and
temporary identifier-based literal recognition use the explicit provider and
unit limit. EOF/structural no-match does not allocate. Malformed UTF-8 after each
scalar family preserves the encoding category and original suffix location,
without publishing cursor changes or leaking storage. Mode-specific comment/
Unicode-space boundaries are exercised through every scalar alternative.
Incomplete string/escape/exponent components propagate their EOF syntax failures.

Real trivia processing between independently read scalar components confirms
that whitespace/comments never join 1 and 2 or tr and ue into one spelling.
These are component composition units, not a container/document parser. Context
validation of multiple root values or missing container separators remains later
work. Every observed allocation for string/literal recognition is failed
independently with cleanup and retry; decoded-unit limits, missing providers
and boundary failure after a warning-bearing string also roll back atomically.
Moving a string variant preserves owned units and original warning metadata
after input mutation/destruction. Compile-time checks cover noexcept entry
points and non-throwing, noncopyable scalar-token ownership.

Known documentation follow-up: token_boundary.hpp still describes intended
callers as unquoted tokens only; scalar dispatch also applies its delimiter
policy to quoted scalar strings. The developer should update that comment.
Comment bodies/later trivia, document/container context, P029 numeric ownership,
complete parser budgets and complete grammar conformance remain separate work.
Shared requirements stay Draft; local component evidence is not a MISRA/CERT
assessment, remote PR check, merge result or automatic issue closure.


## P029 owned numeric spelling checkpoint units, 2026-10-16

Added 70 GoogleTest cases in `tests/owned_number_tests.cpp`, built as
`adaptive_json5_owned_number_unit_tests`, selected by `owned_number_units`.
All 70 passed on native Windows/GCC 15.2.0, C++17, exceptions OFF and ON,
RTTI OFF, with `-Wparentheses -Werror`. Only the affected target was built/run;
no fresh full-suite or remote CI result is claimed. clang-format 18.1.8 passed
for all 71 C++ files. No production C++ was edited during this test work.

Twenty-one spelling fixtures run in both syntax modes with literal numeric-kind
expectations: ordinary numbers, negative-zero variants, preserved fraction/
exponent spelling, values beyond 2^53 and signed/unsigned 64-bit ranges, huge
positive/negative finite exponents, JSON5 decimal forms, hex and all signed
non-finite forms. Success retains exact original bytes in an independent,
exact-size allocation without a terminator; input mutation leaves content intact.
Strict rejection precedes allocation. Twelve invalid complete spellings run in
both modes, checking no-match, trivia/additional tokens, malformed grammar,
unsupported binary/NUL suffixes and encoding errors with exact diagnostics.

Additional cases retain 4096-digit integer/fraction/exponent/hex spellings without
conversion, transfer ownership twice, preserve a borrowed view after input
mutation/destruction, and confirm empty moved-from spelling with retained kind
metadata. The sole allocation is failed while another owner remains live,
then retried through the same provider. Missing providers, invalid/null-empty
views and below/equal/above byte limits have explicit outcomes. Allocation and
release descriptions/balances are checked. Compile-time checks cover noexcept
factory/accessors, non-throwing moves/destruction and disabled copy/move assignment.

This is owned spelling and numeric-family evidence, not executed exact arithmetic,
integrality queries or numeric conversion. Those operations and the full P031
work/exponent/memory accounting are separate. P030 containers and P032 primitive
owned-value composition are also pending. Shared J5 requirements remain Draft;
no MISRA/CERT compliance, embedded execution, issue closure or merge result is
inferred from this checkpoint.

## P030 owned document and checked access units (2026-10-16)

Added 40 GoogleTest cases in `tests/document_tests.cpp`, built as
`adaptive_json5_document_unit_tests` and selected by `document_units`.
All 40 passed on native Windows/GCC 15.2.0, C++17, with exceptions OFF
and ON, RTTI OFF, and project warnings treated as errors. Only the affected
target was built and run; no fresh full-suite or remote CI result is claimed.
clang-format 18.1.8 passed for all 83 C++ files. Production C++ was unchanged.

The units construct and inspect all six alternatives and move each root twice.
They check exact owned string/number payloads after input mutation and record
growth, source reuse after document moves, ordered repeated array elements,
nested direct-child traversal, duplicate-preserving ordered object members,
empty names and absent/unique/ambiguous lookup. Ten name fixtures run both as
standalone comparisons and through lookup, including case differences, unequal
lengths, normalization-sensitive content, embedded NUL and lone surrogates.

Invalid views, indices, wrong types, empty containers, repeated iterator
completion and invalid iterators have explicit results. Relationship tests
exercise wrong containers, self-attachment, cycles, repeated attachment,
root reuse and preservation of supplied member names on failure. Allocation
tracking validates release descriptions and ownership balance, including a
2048-level tree destroyed without traversing child ownership recursively.
Zero/exact/exceeded record limits and missing providers are checked. Each
of seven record-growth allocations is independently failed and retried while
existing records remain readable; failed internal storage append preserves
an externally owned string record for retry. Read-only lookup and traversal
are checked to make no provider allocation requests.

These are P030 component results before recursive parsing. Bare indices retain
the documented same-document precondition; in-range foreign indices cannot be
identified. Iteration requires relationships to remain unchanged. Full P031
memory/work accounting and P032 primitive dispatch remain separate. Shared
requirements remain Draft; these host results do not establish embedded target
execution or MISRA/CERT compliance.

## P031 resource-guard component units (2026-10-17)

Added 36 GoogleTest cases in `tests/resource_guards_tests.cpp`, built as
`adaptive_json5_resource_guards_unit_tests`, selected by `resource_guards_units`.
All 36 plus the 16 existing `api_configuration_units` cases passed in each native
Windows/GCC 15.2.0 C++17 configuration: exceptions OFF and ON, RTTI OFF, with
project warnings treated as errors. The affected targets were rebuilt in both
modes. No fresh full-suite or remote matrix result is claimed. clang-format
18.1.8 passed for all 88 C++ files. Production files were not edited by the agent.

Literal defaults and independent options overrides are checked alongside
backward-compatible syntax selection. Input, numeric spelling and exponent
lengths cover zero, below/equal/above limits and maximum size_t. Reservation
units cover node/member coupling, depth and diagnostic limits, underflow,
mismatched releases, rollback and reuse. String units cover incremental
per-string and aggregate ceilings, failed-charge atomicity, zero and size_t
maximum arithmetic without allocation.

Caller-storage units distinguish resource_limit from allocation_failure, verify
that byte-limit rejection does not call the provider, account for overlapping
blocks and retained peak usage, and follow moves, replacement and release of
owned blocks. They also exercise provider failure/retry while an allocation is
live, missing callbacks, invalid alignments, empty blocks and checked array
multiplication. Allocation descriptions and release balance are checked by a
fixed-record host provider. Work units cover zero, exact exhaustion, failed
charges and maximum-size overflow; exhausted work does not prevent owned-block
cleanup or refund consumed work.

This checkpoint verifies reusable P031 guards and options configuration only.
Existing lexer, document and lookup paths do not automatically use these guards.
Parser-wide accounting, operation-specific work charges and pre-growth checks
must be wired as the parser/conversion paths are developed; #69 tracks complete
cross-cutting accounting. The current components accept all size_t ceilings,
including zero and restrictive cross-limit combinations; they do not expose a
separate invalid-combination validator. No cancellation API is implemented by
these units. Requirements remain Draft; parser-wide enforcement, embedded target
execution and MISRA/CERT compliance are not inferred from these host results.

## P032 primitive-root parsing and storage/lexer regressions (2026-10-18)

Added 145 GoogleTest cases in `tests/primitive_parser_tests.cpp`, built as
`adaptive_json5_primitive_parser_unit_tests`, selected by `primitive_parser_units`.
The complete host suite passed 1346/1346 in each native Windows/GCC 15.2.0 C++17
configuration: exceptions OFF and ON, RTTI OFF, with project warnings treated
as errors. All project unit targets were rebuilt in both modes. clang-format
18.1.8 passed for all 90 C++ files. No remote CI or other native-platform result
is claimed, and no production C++ was edited by the agent during test authoring.

Seventeen primitive fixtures run in both syntax modes with literal type,
decoded UTF-16, Boolean and numeric-family expectations. They include empty
strings, escaped NUL and lone surrogates, negative zero, an integer beyond
2^53, preserved fractional/exponent spelling, JSON5-only quoted/decimal forms,
hex and non-finite values. Forty invalid-input cases check exact original
syntax offsets and cleanup, including empty/trivia-only input, extra values,
unknown/case-variant/escaped identifiers, malformed numbers/strings, embedded
NUL and explicitly unsupported container roots. Comment/trivia behavior is
checked separately in JSON5 and strict JSON.

Source-span units check absent metadata versus invalid access, replacement,
failed-update atomicity, overflow-safe bounds, growth and independent copied
spans. Parsed ownership survives input mutation/destruction and document moves.
Warnings retain their total occurrence count and first original source span;
only the retained descriptor consumes a diagnostic entry. Root string limits
and temporary identifier limits remain independent, allowing literals when
owned string-content limits are zero.

Thirty-six configured-limit fixtures cover input, nodes, strings, aggregate
root-string units, numeric spelling, exponent digits, bytes, diagnostics and
work. Work boundaries verify the documented phase model, including 13 units
for `true` and 17 for ` true `. Owned-byte limits are exercised below/equal/above
the observed requested-byte peak, including another live allocation sharing
the same accounting ceiling. Invalid input views and mismatched accounting
configuration reject before provider calls. Fourteen direct-entry cases verify
that decimal, complete-number and independently limited scalar recognition
forward exponent limits and preserve the caller cursor on failure.

Every reachable allocation in five representative roots is failed independently
and retried while an earlier document remains live. Transaction units check
recognition/construction work failure, record-growth failure and successful
reuse without publishing cursor advancement or appended records on failure.
Result-only and dual-callback storage regressions verify dispatch preference,
no fallback after failure, null-success rejection, missing deallocation,
original release descriptions, nested accounting, and explicit error/peak
preservation. The numeric allocation regression returns a provider-origin
located resource error at a nonzero input offset: its category survives, while
the owned-number boundary deliberately removes its location before numeric
substring rebasing. Separate validation errors retain and rebase their numeric
substring locations.

These results cover the primitive-only P032 entry point, not a complete JSON5
parser. Arrays and objects remain rejected here until P033. Returned documents
borrow a caller-owned accounted_storage object, which must outlive all their
allocations. The byte ceiling is shared with other live allocations using that
object. Repeated low-level consume_primitive calls do not share aggregate string
or diagnostic counters; only the complete single-root wrapper has the stated
root-level aggregate guarantee. Work charges are deterministic conservative
phase allowances, not measured instructions/time or fine-grained cancellation.
Cleanup remains independent of exhausted work. Later parser, conversion and
schema paths still require shared accounting integration under #69. Requirements
remain Draft; no MISRA/CERT compliance or embedded execution is inferred.

## Coverage-directed unit additions (2026-10-19)

Added 66 GoogleTest cases in seven topic-based files under `src/tests/` and
`tests/value/access_boundary_tests.cpp`. Each test body explains the behavioral
reason for the case alongside Arrange/Act/Expect sections. Parameterized faults
have descriptive case names. The units cover the complete container-dispatch
state/mode table, rejected open/close/attachment transitions, read-only frame
lifetime, typed access errors, moved-from numbers, invalid input metadata,
empty roots, malformed trivia, warning-descriptor ownership, absent observers,
non-scalar property queries, malformed surrogate pairs, and allocation failure
while decoding escaped BMP/supplementary characters.

Internal relationship fault injection operates on live `detail::value_record`
objects through the existing mutable internal storage interface. It checks
defensive traversal/attachment failures and unchanged relationships. These
fixtures are deliberately inconsistent internal descriptions, not trees produced
by the public document builder, and do not demonstrate a parser defect.
No private-access macro, invented allocation, production change or coverage
exclusion was used. Existing J5 requirement statuses remain Draft; named units
are inventoried separately in [traceability](traceability.md).

Executed all **1488/1488** cases successfully in both native Windows C++17
configurations, using **32 CTest workers**, with RTTI OFF: GCC 15.2.0 with
exceptions OFF in `build/coverage`, and GCC 16.2.0 with exceptions ON in
`build/debug-unit-tests-exceptions`. clang-format 18.1.8 passes all 164 C++ files.
These are local host results, not remote CI or embedded execution evidence.

The optional GCC 15.2.0/gcovr 8.6 report includes the production roots only:
public `includes/`, private `src/includes/`, and compiled `src/src/`.
Parallel-only counter collection produced different omissions between passing
runs because separate test processes write the same compiled core's `.gcda`
files. The helper retains full-parallel CTest validation, then resets counters
and executes each of the **35 currently registered unit executables** once,
sequentially, for deterministic collection. It does not replay obsolete binaries
left in the build directory. This additional collection is an optional local
diagnostic, not a new automated quality gate.

The requested 100% line/function/branch target is **not achieved**. The report
reaches 100% functions; the remaining lines/branches include invariant defenses
and host-infeasible size-overflow paths. Tests do not mutate immutable input
during decoding, fabricate enormous storage, force private engine states or
throw to create valueless variants. Doing so would violate component contracts
or misrepresent executable behavior. Production changes remain reserved for the
human developer; any simplification of redundant guards needs production design
review. No requirements are marked Verified from coverage percentages.

Final report: **3099/3158 lines (98.1%)**, **528/528 functions (100%)**,
**1537/1618 branches (95.0%)**. `python tools/coverage.py` completed with exit
code 0. Artifacts are local and ignored: `build/coverage/report/index.html`
and `build/coverage/report/coverage.json`. Counts describe the instrumented
executable code, not complete JSON5 conformance or uninstrumented declarations.
The retained report has 59 unexecuted lines and 81 unexecuted branches; these
are still included in the denominators. The following audit distinguishes
invariant-dependent defenses from physically infeasible allocation sizes.
Branch-only entries include short-circuit operands, not additional missing lines.

| Source | Unexecuted source lines | Reason for remaining gap under the current contracts |
| --- | --- | --- |
| [comment.cpp](../src/src/lexer/comment.cpp) | 57 | The line-terminator probe already decodes the same bytes; the block loop guarantees a current byte. |
| [identifier_classification.cpp](../src/src/lexer/identifier/identifier_classification.cpp) | None (branch operands only) | Length-bounded immutable unit access cannot fail; literal classification propagates that defensive result. |
| [scalar_lexer.cpp](../src/src/lexer/scalar_lexer.cpp) | 102, 109 | After a readable non-punctuation start, identifier recognition either yields a token or fails; classification uses valid immutable decoded units. |
| [string_escape.cpp](../src/src/lexer/string/string_escape.cpp) | 81, 88 | Earlier escape recognizers partition no-match/error cases; a decoded source scalar always fits UTF-16. |
| [string_lexer.cpp](../src/src/lexer/string/string_lexer.cpp) | 45, 107 | Warning replay requires the same successfully validated, unchanged body; its repeated piece/escape failures cannot arise through consume_string. |
| [trivia.cpp](../src/src/lexer/trivia.cpp) | None (branch operands only) | With a nonnull buffer and a loop position before EOF, UTF-8 decoding cannot successfully return an absent character. |
| [whitespace.cpp](../src/src/lexer/whitespace.cpp) | 49 | The line-terminator probe already validated the same scalar; the loop guarantees a readable non-EOF position. |
| [accounted_storage.cpp](../src/src/memory/accounted_storage.cpp) | None (branch operands only) | Every allocation preserves outstanding bytes <= the copied byte limit; the already-over-limit operand is defensive. |
| [decoded_string_builder.cpp](../src/src/memory/decoded_string_builder.cpp) | 69 | Size/doubled-capacity overflow requires host-infeasible live string storage; scalar appends add at most two units. |
| [resource_budget.cpp](../src/src/memory/resource_budget.cpp) | None (branch operands only) | Internal counters cannot exceed copied limits after successful reservations; the missing operand is an already-over-limit counter. |
| [work_budget.cpp](../src/src/memory/work_budget.cpp) | None (branch operands only) | Successful charges preserve consumed <= limit; callers cannot change private counters. |
| [parser_attachment.cpp](../src/src/parser/parser_attachment.cpp) | 53 | Adding one to the number of actually allocated records cannot reach size_t overflow on this host. |
| [parser_container_open.cpp](../src/src/parser/parser_container_open.cpp) | 54 | Record-count overflow requires host-infeasible allocated record storage. |
| [parser_engine.cpp](../src/src/parser/parser_engine.cpp) | 45, 59, 66, 72, 73, 104, 123, 128, 143, 189, 196, 215, 222, 244, 274, 279, 287, 321, 328, 369, 379, 390, 403, 404, 409, 418, 436, 451, 470, 590, 606, 607, 616 | Private states/names/root and mirrored resource counters are maintained by dispatch and construction; spans/indices were just validated. Other guards require host-infeasible count/size overflow. |
| [parser_stack.cpp](../src/src/parser/parser_stack.cpp) | 41 | push enforces the frame ceiling before reserve; doubled-capacity overflow requires host-infeasible live frames. |
| [primitive_parser.cpp](../src/src/parser/primitive_parser.cpp) | 63, 168, 175, 293, 300 | Validated numeric errors stay inside their spelling; the scalar variant always has an active known alternative. Real record-count arithmetic cannot overflow on this host; new detached roots and recorded spans satisfy publication checks. |
| [utf16_decode.cpp](../src/src/unicode/utf16_decode.cpp) | 52 | The second unit read follows an explicit remaining-unit bounds check on an immutable string. |
| [decoded_name_equal.cpp](../src/src/value/decoded_name_equal.cpp) | 31 | Equal immutable lengths bound both unit reads before comparison. |
| [document.cpp](../src/src/value/document.cpp) | None (branch operands only) | The validated builder sets parent whenever it sets member/sibling metadata; earlier parent rejection short-circuits the remaining invalid-root operands. |
| [member_lookup.cpp](../src/src/value/member_lookup.cpp) | 67, 74 | Iterator validation proves a live named member before name access; immutable equality cannot fail after its bounds checks. |
| [utf8_export.cpp](../src/src/value/utf8_export.cpp) | 54, 111 | Output is decoded only after whole-string validation; total-byte overflow requires host-infeasible string storage. |
| [value_storage.cpp](../src/src/value/value_storage.cpp) | 51 | append enforces the record ceiling before reserve; doubled-capacity overflow requires host-infeasible live records. |
| [value_view.cpp](../src/src/value/value_view.cpp) | 114, 157 | Every admitted child_at loop returns by the requested ordinal; the nonthrowing payload variant cannot become valueless. |


## P034 array grammar units (2026-10-19)

Added 30 runtime cases in `src/tests/parser/array_grammar_tests.cpp`: 28 named
array grammar cases and two mixed/repeated/nested order cases. The grammar cases
isolate empty and heterogeneous arrays, JSON5-only trailing commas (including
nested arrays and comments), holes, doubled commas, missing separators, misplaced
colons, missing values at EOF and mismatched closers. Rejections check original
byte offsets and cleanup without a successful partial document. Order cases check
indexing and iteration against independent expected kinds and source spans,
including repeated values and nested descendants that must not become siblings.
Each declaration includes purpose and reasoning comments inside its body.

Executed full CTest suites in `build/debug-unit-tests` and
`build/debug-unit-tests-exceptions`: **1518/1518 passed in each**, C++17,
exceptions OFF/ON respectively, RTTI OFF, using all 32 logical CPU workers.
clang-format 18 passed for all 165 C++ files. Logs: `build/p034-off-tests.log`
and `build/p034-on-tests.log` (local build artifacts).

Production C++ and public headers were unchanged. This is host unit evidence
for P034, not complete-input P037 or full J5 verification; requirement statuses
remain unchanged.


## P035 object grammar review and units (2026-10-19)

Source review found the P035 behavior present in the internal engine: quoted
and JSON5 IdentifierName recognition, decoded pending-name ownership, required
colon/value/comma states, mode-dependent trailing commas, nested completion and
source-order attachment retaining every member. No P035 production defect was
identified; this is distinct from full J5 verification. Duplicate rejection is
P036 and complete-input enforcement remains P037.

Added 56 runtime cases in `src/tests/parser/object_grammar_tests.cpp`: 44 named
grammar cases, ten exact decoded-name cases and two member-retention cases.
Grammar cases isolate empty/mixed/nested objects, trailing commas and comments,
bare/single-quoted names, missing names/colons/values/commas, doubled punctuation,
numeric names, incomplete input and mismatched closers. Failures assert exact
original byte offsets, no successful partial tree and storage cleanup. Name cases
exercise reserved words, identifier escapes, quoted escapes, empty names and
embedded NUL. Retention cases overwrite source bytes after parsing and verify
owned names, repeated boolean members, nested names/numbers, source order through
indexing and iteration, and end/out-of-range behavior. Test comments explain
purpose and reasons for the assertions.

Full CTest: **1574/1574 passed in each** of `build/debug-unit-tests` and
`build/debug-unit-tests-exceptions`, GCC 16.2.0, C++17, exceptions OFF/ON
respectively, RTTI OFF, all 32 logical CPU workers. Logs are local build artifacts
`build/p035-off-tests.log` and `build/p035-on-tests.log`. clang-format 18 passed
for all 166 C++ files. P035 review/test work changed no production C++; the
preceding separately authorized comment annotations remain in the working tree.
Requirement statuses remain unchanged.


## Host test timeout investigation (2026-10-19)

A reported timeout for `J5_046_ScalarStorage.FailsEveryAllocationAndAllowsRetry`
was not reproduced: 100 consecutive isolated executions passed in each exception
mode, typically around 0.01 seconds per execution. The unit uses bounded input
and measured allocation-failure loops. This does not establish the cause of the
reported timeout. The shared CTest hang guard was raised from 10 to 60 seconds
to tolerate host process startup/scheduling delays with all 32 workers active;
it is not a parser timing contract. Both generated configurations report the
effective 60-second property. After regeneration, full suites passed 1574/1574
in each exception mode, RTTI OFF, using 32 workers. Logs: local build artifacts
`build/timeout-off-repeat.log`, `build/timeout-on-repeat.log`,
`build/timeout-off-tests.log` and `build/timeout-on-tests.log`.
No production implementation or test assertions were changed for this adjustment.


## P036 duplicate-name policy units (2026-10-19)

Added **69 runtime cases**: 56 named combinations of source examples and
preserve/reject policies, six exact work-budget boundaries, and seven focused
units. Internal cases are in `src/tests/parser/duplicate_policy_tests.cpp`;
public error/span API cases are in `tests/value/member_name_span_tests.cpp`.
The existing parser attachment units now supply the required input-size argument.

The units check quoted, bare, escaped, reserved-word, empty, embedded-NUL and
surrogate-unit names; case and normalization distinctions; parent/child and
separate-object independence; duplicate names after nested values; original
first/repeated byte offsets and lengths; default preservation of both ordered
Boolean values; and retained name spans. Fixed fixture counts/locations are
independent expected data. Duplicate error metadata is checked through result
copying and compile-time construction.

Operational checks cover every observed allocation failure with fresh-engine
retry and balanced accounted storage, checker work boundaries before iteration
and comparison, invalid object access and missing provenance, span-range overflow,
invalid/unnamed views, optional absent metadata, atomic name/span attachment, and
frame ownership/state retention on range failure followed by successful retry.
Purpose and Arrange/Act/Expect comments accompany each declaration, with reasoning
comments inside each body.

Full suites passed **1643/1643 in each configuration**: GCC 16.2.0, C++17,
exceptions OFF/ON respectively, RTTI OFF, all 32 logical CPU workers. Build/test
logs are local artifacts `build/p036-tests-off-build.log`,
`build/p036-tests-on-build.log`, `build/p036-tests-off.log` and
`build/p036-tests-on.log`. clang-format 18 passed for all **170 C++ files**.
Production implementation and public headers were not changed by this unit task.
This is P036 host unit evidence; J5 statuses and P037 complete-document scope
remain unchanged.

## P037 complete-document public API units (2026-10-19)

Added **67 runtime cases** in `tests/parser/complete_document_tests.cpp`, using
only supported public headers: 21 root/trivia examples, 27 rejected complete
inputs, ten resource-boundary examples and nine focused units. Each test has
purpose/Arrange/Act/Expect comments and explains the reason for non-obvious checks
inside its body. `parser.hpp` also joins the independent public-header compilation
inventory without access to private include directories.

The examples check every admitted root type in both syntax modes, exact scalar
content and number spelling, established roots, container/record counts and root
spans excluding surrounding trivia. They cover JSON5 EOF line comments, block
comments and Unicode trivia; empty/trivia-only input; adjacent and separated extra
roots; junk, delimiters and embedded NUL after a completed root; strict-mode comment
and whitespace differences; malformed/truncated UTF-8 suffixes; and exact original
error categories/offsets without partial success payloads. Every proper byte prefix
of a representative nested object fails in both modes. An explicit-length example
accepts a root whose backing array contains additional bytes outside the input view.

Operational units inject failure at every observed allocation point with successful
retry through the same accounting object. They alternate failures and successes
while another returned document remains live, verify cleanup restores prior byte
usage, and check moved public results after overwriting input. Names, embedded-NUL
UTF-16 content, exact numeric spelling, name spans, duplicate-policy diagnostics and
warning count/first descriptor are asserted through public accessors.

Inclusive limits cover complete input bytes, depth, nodes, members, individual and
aggregate string units, numeric spelling, exponent digits, retained diagnostics
and shared work accounting including trailing trivia. Peak requested-byte tests
use the measured target layout, check one byte below the peak, and retain a separate
caller allocation to verify that it shares the ceiling. Invalid input views,
mismatched accounting ceilings and unavailable storage report explicit failures.

Full suites passed **1710/1710 in each configuration**: GCC 16.2.0, C++17,
exceptions OFF/ON respectively, RTTI OFF, 32 parallel workers. Local logs:
`build/p037-build-off.log`, `build/p037-build-on.log`,
`build/p037-tests-off.log` and `build/p037-tests-on.log`.
clang-format 18 passed for all **173 C++ files**.
No production C++ or public headers were changed by this unit-test task.
These are host unit results; requirement statuses remain Draft and no CI, vendor
target or whole-milestone verification is implied.
