# Temporary: issue #93 implementation plan

> **Temporary document.** It tracks implementing
> [ADR 0004](../../docs/adr/0004-simple-api-and-umbrella-header.md) and **will be
> deleted before the #93 branch is merged**. Permanent records are ADR 0004, the
> README, the [user guide](../../docs/memory-and-limits.md) and
> [traceability](../../docs/traceability.md).

Design and rejected alternatives: ADR 0004.
Branch: `93-umbrella-header-adaptive_json5adaptive_json5hpp`.

## Steps

| # | Step | Status |
|---|---|---|
| 1 | ADR 0004 + README link | done, committed e4275f7 |
| 2 | `buffer_arena` (`includes/adaptive_json5/memory/buffer_arena.hpp`, `core/src/memory/buffer_arena.cpp`) + units | done; 9/9 units in each exception mode, RTTI OFF; clang-format 18.1.8 passed |
| 3 | `json5_document` with buffer parsing and the storage owner (`includes/adaptive_json5/json5_document.hpp`) + units | done; overlap check added; 11 units in `tests/api/json5_document_tests.cpp`; full suite 2494/2494 OFF and ON; clang-format passed. `std::align` replaced by `detail::align_within` (J5-042 allowlist) |
| 4 | `json5_value`: navigation, UTF-8 member lookup (private, non-allocating), scalar reads + units | done; 10 units in `tests/api/json5_value_tests.cpp`; full suite 2504/2504 OFF and ON; clang-format passed |
| 5 | `describe` error/warning text, warning access + units | done; 5 units added to `tests/api/json5_document_tests.cpp`; full suite 2509/2509 OFF and ON; clang-format passed |
| 6 | `hosted::parse` returns `json5_document`; remove `hosted_document`; restate #94 units and guide section | done; shared construction through private `detail::json5_document_access`; hosted target gets `core/includes` privately; `json5_document::source()` added; 10 units in `tests/api/hosted_parse_tests.cpp`; full suite 2510/2510 OFF and ON; clang-format passed |
| 7 | Umbrella `adaptive_json5.hpp` (all public headers except `hosted/`) + compile unit + coverage unit | done; 4 units in `tests/api/umbrella_header_tests.cpp` and `umbrella_coverage_tests.cpp`; standalone header units now also cover the #93 headers and the umbrella; full suite 2514/2514 OFF and ON; clang-format passed |
| 8 | README: default include, one short example, memory-switch tutorial; user guide; traceability; ADR 0004 status to implemented | done; README quick start compiled and run outside the repo (exceptions/RTTI OFF); user guide section in `docs/memory-and-limits.md`; ADR 0004 implemented with evidence: units 2514/2514 OFF and ON, validation 921/921 OFF and ON, clang-format passed |

## Developer effort

Estimated hours for the developer's part of each step, in implementation order:
writing the production code from the walkthrough and fixing review findings. Units,
documentation and test runs are not included.

| Order | Developer task | Estimate (h) | Status |
|---|---|---|---|
| 1 | Review and commit ADR 0004 | 0.5 | Done |
| 2 | `buffer_arena`: header and source (control block in the buffer, bump allocation, release counting and reset, accounting limit) | 3 | Done |
| 3 | `json5_document`: storage owner (buffer arena or heap block), `parse(text, buffer, options)`, ownership and move | 4 | Done |
| 4 | `json5_value`: navigation, kind checks, scalar reads; private UTF-8 member-name comparison without allocation | 5 | Done |
| 5 | Error and warning text (`describe`), warning count and access | 2 | Done |
| 6 | `hosted::parse` returning `json5_document`; remove `hosted_document`; CMake unchanged | 2 | Done |
| 7 | Umbrella header `adaptive_json5.hpp` | 0.5 | Done |
| 8 | Review README tutorial, user guide and ADR 0004 status | 1 | Not done |
| 9 | Replace #93's issue description; final CI check and merge | 0.5 | Not done |
| | **Total** | **18.5** | **17 estimated hours of tasks done** |

## Details to remember

- #108 is merged and already in this branch's history, so step 6 can start any time.
- Small choices recorded in ADR 0004: duplicate member name → error; `as_double`
  scratch from the document's storage; `as_string` writes UTF-8 into a caller buffer.
- `lookup_member` today takes a `decoded_string`; step 4 needs a UTF-8 comparison
  that does not allocate.
- README tutorial (step 8): what OFF/ON do; activate (command line, subproject via
  `set(... CACHE BOOL "" FORCE)` before `add_subdirectory`, `target_link_libraries`
  with `AdaptiveJSON5::hosted`, extra include, how to check the install); deactivate
  (option OFF, remove link and include, switch to `json5_document::parse(text, buffer)`,
  cached value in old build directories); which to choose; `.a` files per mode.
- README example stays minimal: the next issue creates simple and advanced usage
  examples (consider `examples/` with `ADAPTIVE_JSON5_BUILD_EXECUTABLE`).
- #93's issue description is to be replaced with the acceptance criteria from
  ADR 0004, including the README memory-switch line.

## Before merging

1. ADR 0004 status updated to implemented, with evidence (units both modes,
   validation tests, repository-wide format check).
2. Delete this file.

## Step 2 local evidence (2026-10-28)

Reviewed the developer-owned arena header/source and CMake registration. Added
`tests/memory/buffer_arena_tests.cpp` and the `buffer_arena_units` label. Nine
GoogleTest cases passed in each of `debug-unit-tests` and
`debug-unit-tests-exceptions`, with RTTI OFF in both. clang-format 18.1.8 passed
the two production files and the new unit file. These are scoped host results;
full PR units/validation/formatting remain separate. J5 requirements remain Draft.

## Step 3 starting checkpoint

Proposed placement: public declarations in
`includes/adaptive_json5/json5_document.hpp`, ordinary definitions in
`core/src/value/json5_document.cpp`, registered in `core/src/CMakeLists.txt`.
Private shared control-block declarations belong under `core/includes/memory/`
only if needed by both core and hosted implementations. Public headers use only
forward declarations for those private definitions, following the existing hosted
owner pattern.

First implement the storage owner and caller-buffer creation path. Align a
control block within the buffer before constructing it; reject null/insufficient
storage with `allocation_failure`. The block contains a `buffer_arena` over the
remaining bytes followed by `accounted_storage` using its callbacks and
`options.limits.max_owned_bytes`. Provider overhead is excluded from the existing
requested-byte ceiling; it still consumes physical caller-buffer capacity.

`json5_document` declares its storage owner before its `parsed_document`, so the
parsed values, warnings and retained source release before the control block is
destroyed. Move construction transfers ownership without relocating the block;
copy and move assignment stay deleted for this initial design. The owner needs
a nonallocating release operation usable by the later hosted path without adding
heap allocation/deallocation dependencies to the core.

The parse factory calls the existing `parse_document(input, options, accounting)`
and forwards its exact explicit error. On failure, parsed temporaries must be
destroyed before the control block. On success, transfer both owners into the
result. The caller buffer stays alive and exclusively reserved until document
destruction; input and storage must not overlap while parsing. Reusing the buffer
for another document while the first lives is unsupported. Do not silently force
source retention: source-location behavior is completed in step 5.

The developer implemented `document_storage_owner` and the private
`make_buffer_document_storage` factory. Their internal units live in
`core/tests/memory/document_storage_tests.cpp`: nine cases passed in each of
`debug-unit-tests` and `debug-unit-tests-exceptions` on 2026-10-28, with RTTI OFF
in both. clang-format 18.1.8 passed the new unit file. This covers ownership
transfer/release, misaligned caller buffers and guards, unusable buffers, physical
exhaustion/retry, a zero accounting ceiling and buffer reuse after destruction.

Step 3 remains in progress: `json5_document` and its parse factory are still the
next production checkpoint. These scoped storage results do not establish that
document parsing or the complete step is implemented or verified.
