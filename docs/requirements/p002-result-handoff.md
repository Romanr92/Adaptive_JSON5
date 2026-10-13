# P002: Non-throwing result foundation

Status: Proposed implementation guidance; production implementation and unit
evidence pending. This does not change the baseline or J5 contract statuses.

The ordered sequence reaches its first production C++ work here, after the
[P001 configuration evidence](../verification.md). P001's actual production
target remains pending until there is a real component to compile.

P002 requires success/error alternatives carrying a stable minimal error code
and byte offset. Reporting a failure must neither allocate nor throw. Its linked
contracts are J5-003, J5-043 and J5-045 in [requirements](../requirements.md).

## Suggested first production design

The following names and representation choices are suggestions, not existing
public API or additional requirements. The human developer should introduce
the declarations under `include/adaptive_json5/`, with any out-of-line definitions
under `src/`. A header-only foundation is also possible.

1. Define an `error_code` enum with explicit stable values. Distinguish syntax,
   encoding, duplicate policy, wrong type/access, numeric conversion, resource
   limit and allocation failure. Later schema/tool categories belong to those
   layers. Do not require message text to identify a failure.
2. Define a small `error` value containing the code and original zero-based byte
   offset, using an unsigned size type appropriate for the input length. Document
   how operations without an input location represent that absence; an offset
   of zero alone must not ambiguously mean both the first byte and no location.
3. Define explicit success and failure construction for a result. Store the
   error inline. An initial result with no success payload can support status
   operations; a payload-bearing result must safely manage the lifetime of its
   active alternative. Do not assume arbitrary payload constructors or moves
   are non-throwing: constrain supported payload operations or design explicit
   fallible construction before adopting a generic result template.
4. Expose a non-throwing state query and checked access to the active
   alternative. Wrong-alternative access must give an explicit unavailable/error
   outcome, rather than throwing, asserting or aborting. Pointer-returning
   inspection is one possible design.
5. Guarantee `noexcept` for minimal error construction, failure construction and
   state/error inspection. They must use no dynamic storage, OS calls, RTTI or
   exception recovery. Rich diagnostic strings and paths are later additions.

An allocation failure at byte 17 must therefore return the allocation-failure
code and offset 17 even when no further storage can be obtained. Compiler
exception support must not change that observable result.

## Units to add against the real declarations

| Case | Expected observation |
| --- | --- |
| Explicit success construction | Success state; error inspection reports absence |
| Each minimal failure category | Failure state; exact code survives construction and inspection |
| Offset zero and a representative nonzero offset | Original offset is preserved |
| Largest representable offset | No narrowing or wraparound in result storage |
| Failure without an input location | Documented absence is distinguishable from byte zero |
| Wrong-alternative inspection | Explicit unavailable/error outcome; no termination |
| Copy/move where supported | State and inline error survive; payload ownership follows its documented policy |
| Guaranteed non-throwing operations | Compile-time `noexcept` checks against the public declarations |

Run common units with exceptions OFF and ON and RTTI OFF. Inspect minimal result
storage and implementation for allocation-free reporting; later P003 allocator
failure injection supplies additional evidence when real storage APIs exist.
Do not invent a fake parser or result implementation under `tests/` to make
these cases pass. Add exact test names to traceability only when units exist,
and record execution separately.

## Build connection and next ordered step

Once the developer supplies the real component, connect it to the production
target described in the [P001 handoff](p001-build-handoff.md), then add the units
above using the repository's test skill. For a header-only foundation, CMake can
express that component as an interface target; no dummy source is necessary.

P003 follows: caller-supplied fallible storage, checked size accounting and
cleanup. Its implementation depends on P002's explicit result/error contract.
