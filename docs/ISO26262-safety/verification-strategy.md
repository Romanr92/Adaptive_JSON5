# Verification strategy

| Item | Value |
| --- | --- |
| Work product | Software verification strategy, including structural coverage (ISO 26262-6, clauses 9 and 10; ISO 26262-8, clause 9) |
| Issue | [#138](https://github.com/Romanr92/Adaptive_JSON5/issues/138); implementation in [#132](https://github.com/Romanr92/Adaptive_JSON5/issues/132) and #141 (Clang) |
| Status | Draft, not reviewed. The owner decided the methods below on 2026-11-07; most of the new checks are not implemented yet. |
| Scope | Parser library `AdaptiveJSON5::parser`. The hosted add-on is QM. |

This document says how the library's safety requirements are verified: which
methods are used, which of them block a pull request, and what evidence each one
produces. It does not claim that any requirement is verified yet.

## 1. Owner decision on the scope of automated checks

Until now the project allowed only unit tests, validation tests and formatting as
automated checks, and explicitly excluded fuzz, sanitizer and coverage checks. For a
parser of untrusted input developed at ASIL B(D), those are exactly the evidence an
assessor expects, so on 2026-11-07 the owner changed the scope:

- **Sanitizer runs, fuzzing, branch coverage and a second compiler (Clang) are now
  allowed, and they block pull requests** as described below.
- Integration, end-to-end, benchmark and static-analysis checks are still out of
  scope unless the owner decides otherwise.
- The checks are build and CI work, so AI agents may set them up (see `AGENTS.md`).

## 2. Verification methods

| # | Method | What it shows | Runs on | Blocks a PR? | Status |
| --- | --- | --- | --- | --- | --- |
| V1 | GoogleTest unit tests, exceptions OFF and ON, RTTI OFF | Each part behaves as specified, including errors and limit boundaries | Every PR, Linux x86-64 and ARM64, Windows MinGW | Yes | In place |
| V2 | Validation tests over pinned third-party fixtures (JSONTestSuite, json5-tests) | The grammar accepts and rejects what the specifications say | Every PR | Yes | In place |
| V3 | Formatting check (clang-format 18.1.8) | Code layout follows the style guide | Every PR | Yes | In place |
| V4 | **Sanitizer runs:** the existing unit and validation tests rebuilt with AddressSanitizer and UndefinedBehaviorSanitizer | No out-of-bounds access, use after free or undefined behaviour on any tested path (SSR-01, SSR-08) | Every PR | **Yes**, like the unit tests | Planned in #132 |
| V5 | **Fuzzing:** a libFuzzer harness that feeds random and mutated input through parsing, value access, conversions and error formatting | No crash, hang or memory error on input nobody thought of (SSR-01, SSR-02, SSR-08) | Every PR for a fixed time (about 5 to 10 minutes); a longer run weekly | **Yes**, but only when it finds a crash, timeout or out-of-memory | Planned in #132 |
| V6 | **Branch coverage**, measured with GCC, gcov and gcovr | Every branch in the parser library was taken both ways by the tests | Every PR | **Yes**, once full branch coverage is reached (target: end of M4) | Measured locally today with `tools/coverage.py`; CI job planned |
| V7 | **MC/DC**, measured with GCC 14 and gcovr | Each condition in a decision was shown to change the outcome on its own | Every PR | No, reported as extra evidence | Planned |
| V8 | **Second compiler:** build and run all unit and validation tests with Clang | Results do not depend on one compiler; guards against a single faulty or compromised compiler (see [tool confidence](tool-confidence.md)) | Every PR | **Yes** | Planned in #141 |

Unit tests are the main method. V4 to V8 run the **same** tests or the same code in
different ways, so a defect has several independent chances to show up.

## 3. Structural coverage

### 3.1 What is required

ISO 26262-6 recommends structural coverage at the unit level according to the ASIL.
For ASIL B, branch coverage is highly recommended. MC/DC is highly recommended
only at ASIL D. The library is developed at ASIL B(D), so:

- **Branch coverage** is the required measure. The target is **full branch coverage**
  of the parser library by the end of milestone M4. From then on the coverage job
  blocks any pull request that lowers it.
- **MC/DC** is measured and reported, but does not block. It is extra evidence that
  strengthens the argument, especially for integrators who reuse the library in an
  ASIL D context without decomposition.
- **Justification of uncovered code** (a written reason for every branch that stays
  uncovered) is done **after M4**, once the remaining gaps are known.

The review run at 0.2.0 measured 95.7 % lines, 98.5 % functions and 90.0 % branches.

### 3.2 How coverage is measured

The tools split the work like this:

- **GoogleTest only runs the tests.** It does not measure coverage.
- **The compiler measures.** Building with `--coverage` makes GCC add counters to the
  compiled code. Running the tests fills the counters, and **gcov** reads them.
- **gcovr** (version 8.6, pinned in `tools/coverage.py`) collects the gcov data and
  produces the line, branch and condition reports.
- **MC/DC** comes from the same chain. GCC 14 added the option `-fcondition-coverage`,
  which records masking MC/DC, and gcovr reports it as condition coverage. The
  current CI uses GCC 13, so the coverage job needs GCC 14 or later. The unit tests
  themselves do not change.

## 4. Evidence per release

For each release, the following is recorded with the release tag:

- the CI results of V1 to V8 for the release commit;
- the coverage reports (branch, and MC/DC as information);
- which configurations the evidence covers: compilers, build types (Debug or
  Release), exception modes, and that the tests ran on host machines only, not on a
  target device.

Evidence covers only the configurations that were actually tested. Today all test
jobs build `Debug`, while the release packages are built as `Release`; #130 adds a
`Release` test job so that the shipped configuration is also tested.

## 5. Out of scope

- **Target testing.** Tests run on host machines. Running the library on the
  integrator's target hardware, with the integrator's compiler and options, is the
  integrator's job.
- **Integration testing** with the integrator's software, including the E2E check
  required by A-08, is the integrator's job.
