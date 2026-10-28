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
| 2 | `buffer_arena` (`includes/adaptive_json5/memory/buffer_arena.hpp`, `core/src/memory/buffer_arena.cpp`) + units | not started |
| 3 | `json5_document` with buffer parsing and the storage owner (`includes/adaptive_json5/json5_document.hpp`) + units | not started |
| 4 | `json5_value`: navigation, UTF-8 member lookup (private, non-allocating), scalar reads + units | not started |
| 5 | `describe` error/warning text, warning access + units | not started |
| 6 | `hosted::parse` returns `json5_document`; remove `hosted_document`; restate #94 units and guide section | unblocked: #108 merged (5eac8ef is in this branch) |
| 7 | Umbrella `adaptive_json5.hpp` (all public headers except `hosted/`) + compile unit + coverage unit | not started |
| 8 | README: default include, one short example, memory-switch tutorial; user guide; traceability; ADR 0004 status to implemented | not started |

## Developer effort

Estimated hours for the developer's part of each step, in implementation order:
writing the production code from the walkthrough and fixing review findings. Units,
documentation and test runs are not included.

| Order | Developer task | Estimate (h) | Status |
|---|---|---|---|
| 1 | Review and commit ADR 0004 | 0.5 | Done |
| 2 | `buffer_arena`: header and source (control block in the buffer, bump allocation, release counting and reset, accounting limit) | 3 | Not done |
| 3 | `json5_document`: storage owner (buffer arena or heap block), `parse(text, buffer, options)`, ownership and move | 4 | Not done |
| 4 | `json5_value`: navigation, kind checks, scalar reads; private UTF-8 member-name comparison without allocation | 5 | Not done |
| 5 | Error and warning text (`describe`), warning count and access | 2 | Not done |
| 6 | `hosted::parse` returning `json5_document`; remove `hosted_document`; CMake unchanged | 2 | Not done |
| 7 | Umbrella header `adaptive_json5.hpp` | 0.5 | Not done |
| 8 | Review README tutorial, user guide and ADR 0004 status | 1 | Not done |
| 9 | Replace #93's issue description; final CI check and merge | 0.5 | Not done |
| | **Total** | **18.5** | **0.5 done** |

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
