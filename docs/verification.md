# Development setup verification

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
