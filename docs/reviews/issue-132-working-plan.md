# Issue #132 temporary working plan

Status: planning only, 2026-11-16. No implementation or executed verification is
claimed by this checklist.

**DELETE THIS FILE BEFORE MERGING THE ISSUE #132 PR.** This is a temporary work
aid, not a permanent issue report. Before deletion, retain essential contracts,
usage and actual verification evidence in the existing permanent documentation
and PR. Remove any links to this file as well. The final pre-merge review must
confirm this file is absent from the PR's resulting tree.

## Scope and authority

- [Issue #132](https://github.com/Romanr92/Adaptive_JSON5/issues/132) covers test
  allocator fixes, sanitizer runs and a test-only libFuzzer harness.
- The owner's 2026-11-07 comment supersedes the original scheduled/non-blocking
  proposal: sanitizers block every PR; fuzzing runs for a fixed time on every PR
  and blocks on crash, timeout or out-of-memory, with a longer weekly run.
- The owner's later comment requires separate sanitizer and fuzz README badges.
  The 2026-11-16 user instruction also includes MISRA and unit-test coverage badges.
- Follow [the verification strategy](../ISO26262-safety/verification-strategy.md),
  J5-097/J5-098 in [core quality requirements](../../core/docs/requirements-quality.md),
  [ADR 0002](../../core/docs/adr-0002-public-private-source-layout.md) and the
  repository requirement-guidance and unit-test skills.
- AI changes stay in test code, build setup, workflows and documentation. Romanr92
  owns all production C++ and public headers. Report production defects for him
  to fix; do not suppress sanitizer findings to make CI pass.

## 1. Confirm the baseline

- [ ] Inspect the current branch, pending changes and related #116/#141 work.
- [ ] Inventory current unit and validation executables and their actual counts;
  the issue's historical 3,435-case result is not a current acceptance count.
- [ ] Reproduce allocator-related sanitizer failures and retain logs.
- [ ] Confirm compiler/runtime availability and existing CMake flag propagation.
- [ ] Reconcile obsolete statements excluding fuzz/sanitizers in quality guidance
  with the recorded owner decision, without changing unrelated contracts/status.

## 2. Repair test allocation probes

- [ ] Review `tests/api/pure_memory_tests.cpp` and
  `tests/api/diagnostic_format_tests.cpp` and their executable source lists.
- [ ] Provide consistent scalar/array, sized, nothrow and aligned allocation and
  deallocation overloads, including corresponding failure-cleanup forms.
- [ ] Preserve alignment, zero-size handling and allocation counting. Keep each
  replacement confined to its host test executable and avoid duplicate definitions.
- [ ] Verify the probe still detects allocations and allocation-free operations.
- [ ] Run affected GoogleTest cases in exceptions OFF/ON, RTTI OFF, normally and
  under ASan+UBSan. Do not disable alloc-dealloc-mismatch detection.

## 3. Add isolated sanitizer configuration

- [ ] Add opt-in host build support for ASan+UBSan, instrumenting the compiled
  parser and all tested project code, with matching executable link flags.
- [ ] Keep flags out of exported consumer interfaces and ordinary build defaults.
- [ ] Build/run internal and public units plus independently enabled validation
  tests in exceptions OFF/ON, RTTI OFF; sanitizer findings must fail the run.
- [ ] Keep the pinned GoogleTest revision and offline override intact.

## 4. Add the libFuzzer harness and corpus

- [ ] Add an opt-in Clang host-only target under `tests/fuzz/`, using public
  interfaces and linked compiled parser code; no private include access.
- [ ] Instrument the parser for fuzz coverage as well as the harness. Keep the
  fuzzer runtime out of GoogleTest executables and installed consumer targets.
- [ ] Exercise JSON/JSON5, duplicate policies, bounded limits/caller storage,
  successful tree access, numeric conversions, checked UTF-8 export, diagnostic
  formatting and the simple API. Inspect current declarations before choosing calls.
- [ ] Keep traversal and per-input resource use bounded. Expected syntax,
  conversion and resource errors are valid outcomes, not crashes.
- [ ] Seed from pinned committed fixtures; preserve versions, licenses and notices.
  Include malformed input, Unicode, number, duplicate-name and nesting boundaries.
- [ ] Keep fuzz builds independent of GoogleTest acquisition when unit and
  validation flags are OFF. Wire fixture acquisition explicitly where needed.
- [ ] Verify a local smoke run and reproducer replay. Turn confirmed defects into
  deterministic GoogleTest regressions in the appropriate test tree.

## 5. Add CI checks and retained evidence

- [ ] Add a sanitizer workflow on PRs, with both exception modes and RTTI OFF.
- [ ] Add a separate fuzz workflow on PRs and a weekly schedule. Proposed budgets:
  5 minutes per PR and 30 minutes weekly, with explicit per-input timeout and
  memory limits and enough outer job time for build/artifact upload.
- [ ] Fail fuzz runs on crash, timeout or OOM; keep infrastructure/build failures
  visible. Do not gate on execution count or coverage growth.
- [ ] Use read-only permissions, immutable action revisions and existing compiler
  conventions. Preserve logs, versions, configuration and crash/reproducer files
  on failures, including the seed/configuration needed to replay them.
- [ ] Confirm sanitizer/fuzz failures propagate to their check results. Verify
  required-check activation separately from workflow existence; record any
  repository-administration step still outstanding.

## 6. README badges and coverage reporting

- [ ] Add separate sanitizer and fuzz workflow badges, visible in either state.
- [ ] Preserve/verify the existing MISRA badge (`misra-cpp.yml`); do not duplicate
  it. Its status reports analysis execution, not MISRA compliance.
- [ ] Add a distinct unit-test coverage badge. The current coverage job lives in
  `unit-tests.yml` and uploads `unit-test-coverage-html`; an overall unit workflow
  badge cannot identify that job's individual result.
- [ ] Give coverage an independently addressable workflow/status source, or publish
  a badge from the measured report. Choose and document the mechanism during
  implementation; do not display a hard-coded percentage or invent a result.
- [ ] Label status versus measured percentage accurately and link to the report
  or check. Preserve the existing measurement scope: GCC 14, exceptions OFF,
  RTTI OFF, unit tests only, validation excluded. If showing a percentage, label
  the metric (prefer branch coverage) and identify the measured revision.
- [ ] Preserve the current coverage threshold policy; adding a badge does not
  independently activate the planned full-branch-coverage gate.

## 7. Acceptance and permanent evidence

- [ ] All current units and validation cases pass under ASan+UBSan, with no
  allocator mismatch suppressions, exceptions OFF/ON and RTTI OFF.
- [ ] Timed fuzz checks finish without crash, timeout or OOM and reproduce findings
  when supplied an artifact. Ordinary required jobs and formatting pass.
- [ ] A tests/fuzz-disabled consumer build acquires neither GoogleTest nor
  libFuzzer, and no test code or instrumentation leaks into its exported package.
- [ ] Update existing verification documentation and traceability with exact
  configurations, revisions, named regression units and actual results. Record
  fuzz evidence separately from GoogleTest inventory; retain Draft statuses.
- [ ] Fill the PR template's safety/security impact section after checking sections
  K/L and both assumption documents. Assess robustness (SSR-01/CSR-01), explicit
  failures, bounds and storage evidence according to actual changes; name affected
  IDs and missing evidence without claiming safety/security approval.

## 8. Mandatory pre-merge cleanup

- [ ] Transfer essential usage and actual results to their permanent homes and PR.
- [ ] Delete `docs/reviews/issue-132-working-plan.md` and remove incoming links.
- [ ] Review the final diff and confirm this temporary plan is absent.
- [ ] Confirm required CI checks and outstanding gate-activation tasks before merge.

Do not merge while this temporary file remains in the resulting tree.
